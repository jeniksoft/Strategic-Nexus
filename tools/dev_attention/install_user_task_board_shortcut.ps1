param(
    [switch]$InstallStartupShortcut = $true,
    [switch]$InstallDesktopShortcut = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$watchdogScript = Join-Path $repoRoot "tools\dev_attention\run_user_task_board_watchdog.ps1"
$iconPath = Join-Path $repoRoot "tools\dev_attention\assets\task_board_calm.ico"

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function New-TaskBoardShortcut {
    param([string]$ShortcutPath)

    Ensure-Directory $ShortcutPath

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    $shortcut.TargetPath = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
    $shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$watchdogScript`""
    $shortcut.WorkingDirectory = $repoRoot
    $shortcut.WindowStyle = 7
    $shortcut.Description = "Strategic Nexus user task board"
    $shortcut.IconLocation = "$iconPath,0"
    $shortcut.Save()
}

if ($InstallDesktopShortcut) {
    $desktop = [Environment]::GetFolderPath("Desktop")
    New-TaskBoardShortcut -ShortcutPath (Join-Path $desktop "Strategic Nexus Task Board.lnk")
}

if ($InstallStartupShortcut) {
    $startup = [Environment]::GetFolderPath("Startup")
    New-TaskBoardShortcut -ShortcutPath (Join-Path $startup "Strategic Nexus Task Board.lnk")
}

Write-Host "task_board_icon=$iconPath"
Write-Host "desktop_shortcut_installed=$InstallDesktopShortcut"
Write-Host "startup_shortcut_installed=$InstallStartupShortcut"
