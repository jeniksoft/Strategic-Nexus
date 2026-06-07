$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$trayExe = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"
$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "Strategic Nexus Companion.lnk"
$sncAppUserModelId = "StrategicNexus.Companion"

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
        } finally {
            if (store != null && Marshal.IsComObject(store)) {
                Marshal.FinalReleaseComObject(store);
            }
        }
    }
}
"@

if (-not (Test-Path -LiteralPath $trayExe)) {
    throw "SNC tray executable not found: $trayExe"
}

if (-not (Test-Path -LiteralPath $startupFolder)) {
    New-Item -ItemType Directory -Force -Path $startupFolder | Out-Null
}

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $trayExe
$shortcut.WorkingDirectory = Split-Path -Parent $trayExe
$shortcut.Description = "Strategic Nexus Companion"
$shortcut.IconLocation = "$trayExe,0"
$shortcut.WindowStyle = 7
$shortcut.Save()
[StrategicNexusShortcutProperties]::SetAppUserModelId($shortcutPath, $sncAppUserModelId)

Write-Host "snc_startup_shortcut_installed=true"
Write-Host ("snc_startup_shortcut_path=" + $shortcutPath)
Write-Host ("snc_startup_shortcut_target=" + $trayExe)
Write-Host ("snc_startup_shortcut_icon=" + "$trayExe,0")
Write-Host ("snc_app_user_model_id=" + $sncAppUserModelId)
