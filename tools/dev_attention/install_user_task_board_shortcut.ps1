param(
    [switch]$InstallStartupShortcut = $false,
    [switch]$InstallDesktopShortcut = $true,
    [switch]$InstallStartMenuShortcut = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$launcherSourcePath = Join-Path $repoRoot "tools\dev_attention\TaskBoardLauncher.cs"
$launcherOutputPath = Join-Path $repoRoot "dist\private_tools\StrategicNexusTaskBoardLauncher.exe"
$nativeTaskBoardPath = Join-Path $repoRoot "dist\private_tools\StrategicNexusNativeTaskBoard.exe"
$iconPath = Join-Path $repoRoot "tools\dev_attention\assets\task_board_calm.ico"
$shortcutName = "Codex Task Board.lnk"
$legacyShortcutNames = @("Strategic Nexus Task Board.lnk")
$taskBoardAppUserModelId = "Codex.TaskBoard"
$firstRunStartMenuShortcutMarkerPath = Join-Path $repoRoot "dist\private_reports\user_task_board_start_menu_shortcut.checked"

Add-Type @"
using System;
using System.Runtime.InteropServices;

[ComImport]
[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
[Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")]
public interface IPropertyStore
{
    void GetCount(out uint cProps);
    void GetAt(uint iProp, out PropertyKey pkey);
    void GetValue(ref PropertyKey key, out PropVariant pv);
    void SetValue(ref PropertyKey key, ref PropVariant pv);
    void Commit();
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct PropertyKey
{
    public Guid fmtid;
    public uint pid;
}

[StructLayout(LayoutKind.Explicit)]
public struct PropVariant : IDisposable
{
    [FieldOffset(0)]
    public ushort vt;
    [FieldOffset(2)]
    public ushort wReserved1;
    [FieldOffset(4)]
    public ushort wReserved2;
    [FieldOffset(6)]
    public ushort wReserved3;
    [FieldOffset(8)]
    public IntPtr pointerValue;

    public static PropVariant FromString(string value)
    {
        PropVariant variant = new PropVariant();
        variant.vt = 31;
        variant.pointerValue = Marshal.StringToCoTaskMemUni(value);
        return variant;
    }

    public void Dispose()
    {
        if (pointerValue != IntPtr.Zero) {
            Marshal.FreeCoTaskMem(pointerValue);
            pointerValue = IntPtr.Zero;
        }
    }
}

public static class StrategicNexusShortcutProperties
{
    private const uint GPS_READWRITE = 0x00000002;

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = true)]
    private static extern int SHGetPropertyStoreFromParsingName(
        [MarshalAs(UnmanagedType.LPWStr)] string pszPath,
        IntPtr pbc,
        uint flags,
        ref Guid riid,
        [MarshalAs(UnmanagedType.Interface)]
        out IPropertyStore propertyStore);

    public static void SetAppUserModelId(string shortcutPath, string appId)
    {
        IPropertyStore store = null;
        try {
            var propertyStoreId = new Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99");
            int hr = SHGetPropertyStoreFromParsingName(shortcutPath, IntPtr.Zero, GPS_READWRITE, ref propertyStoreId, out store);
            if (hr != 0) {
                Marshal.ThrowExceptionForHR(hr);
            }

            var key = new PropertyKey();
            key.fmtid = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3");
            key.pid = 5;

            var value = PropVariant.FromString(appId);
            try {
                store.SetValue(ref key, ref value);
                store.Commit();
            } finally {
                value.Dispose();
            }
        } catch (Exception ex) {
            throw new InvalidOperationException("Failed to write AppUserModelID to shortcut '" + shortcutPath + "': " + ex.ToString(), ex);
        } finally {
            if (store != null && Marshal.IsComObject(store)) {
                Marshal.FinalReleaseComObject(store);
            }
        }
    }
}
"@

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Ensure-TaskBoardLauncher {
    if (-not (Test-Path -LiteralPath $launcherSourcePath)) {
        throw "Task Board launcher source not found: $launcherSourcePath"
    }

    Ensure-Directory $launcherOutputPath
    $sourceWriteTime = (Get-Item -LiteralPath $launcherSourcePath).LastWriteTimeUtc
    $needsBuild = -not (Test-Path -LiteralPath $launcherOutputPath)
    if (-not $needsBuild) {
        $needsBuild = (Get-Item -LiteralPath $launcherOutputPath).LastWriteTimeUtc -lt $sourceWriteTime
    }

    if (-not $needsBuild) {
        return
    }

    $cscCandidates = @(
        (Join-Path $env:SystemRoot "Microsoft.NET\Framework64\v4.0.30319\csc.exe"),
        (Join-Path $env:SystemRoot "Microsoft.NET\Framework\v4.0.30319\csc.exe")
    )
    $cscPath = @($cscCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1)
    if (-not $cscPath) {
        throw "C# compiler csc.exe not found. Cannot build Task Board launcher."
    }

    & $cscPath /nologo /target:winexe /out:$launcherOutputPath /reference:System.Windows.Forms.dll $launcherSourcePath
    if ($LASTEXITCODE -ne 0) {
        throw "Task Board launcher build failed with exit code $LASTEXITCODE"
    }
}

function Ensure-NativeTaskBoard {
    if (-not (Test-Path -LiteralPath $nativeTaskBoardPath)) {
        throw "Native Task Board executable not found: $nativeTaskBoardPath"
    }
}

function New-TaskBoardShortcut {
    param([string]$ShortcutPath)

    Ensure-Directory $ShortcutPath

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    $shortcut.TargetPath = $nativeTaskBoardPath
    $shortcut.Arguments = ""
    $shortcut.WorkingDirectory = $repoRoot
    $shortcut.WindowStyle = 1
    $shortcut.Description = "Codex task board"
    $shortcut.IconLocation = "$iconPath,0"
    $shortcut.Save()
    [StrategicNexusShortcutProperties]::SetAppUserModelId($ShortcutPath, $taskBoardAppUserModelId)
}

function Remove-LegacyTaskBoardShortcuts {
    param([string]$FolderPath)

    foreach ($legacyShortcutName in $legacyShortcutNames) {
        $legacyShortcutPath = Join-Path $FolderPath $legacyShortcutName
        if (Test-Path -LiteralPath $legacyShortcutPath) {
            Remove-Item -LiteralPath $legacyShortcutPath -Force
        }
    }
}

Ensure-NativeTaskBoard

if ($InstallDesktopShortcut) {
    $desktop = [Environment]::GetFolderPath("Desktop")
    New-TaskBoardShortcut -ShortcutPath (Join-Path $desktop $shortcutName)
    Remove-LegacyTaskBoardShortcuts -FolderPath $desktop
}

$startup = [Environment]::GetFolderPath("Startup")
$startupShortcutPath = Join-Path $startup $shortcutName
if ($InstallStartupShortcut) {
    New-TaskBoardShortcut -ShortcutPath $startupShortcutPath
    Remove-LegacyTaskBoardShortcuts -FolderPath $startup
} elseif (Test-Path -LiteralPath $startupShortcutPath) {
    Remove-Item -LiteralPath $startupShortcutPath -Force
}

if ($InstallStartMenuShortcut) {
    $startMenuPrograms = [Environment]::GetFolderPath("Programs")
    $startMenuFolder = Join-Path $startMenuPrograms "Codex"
    New-TaskBoardShortcut -ShortcutPath (Join-Path $startMenuFolder $shortcutName)
    Remove-LegacyTaskBoardShortcuts -FolderPath $startMenuFolder

    $legacyStartMenuFolder = Join-Path $startMenuPrograms "Strategic Nexus"
    Remove-LegacyTaskBoardShortcuts -FolderPath $legacyStartMenuFolder
    if ((Test-Path -LiteralPath $legacyStartMenuFolder) -and -not (Get-ChildItem -LiteralPath $legacyStartMenuFolder -Force)) {
        Remove-Item -LiteralPath $legacyStartMenuFolder -Force
    }

    Ensure-Directory $firstRunStartMenuShortcutMarkerPath
    Set-Content -LiteralPath $firstRunStartMenuShortcutMarkerPath -Value (Get-Date).ToString("o") -Encoding UTF8
}

Write-Host "task_board_icon=$iconPath"
Write-Host "task_board_target=$nativeTaskBoardPath"
Write-Host "task_board_app_user_model_id=$taskBoardAppUserModelId"
Write-Host "desktop_shortcut_installed=$InstallDesktopShortcut"
Write-Host "startup_shortcut_installed=$InstallStartupShortcut"
Write-Host "start_menu_shortcut_installed=$InstallStartMenuShortcut"
