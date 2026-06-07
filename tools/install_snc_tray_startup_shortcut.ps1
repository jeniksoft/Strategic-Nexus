$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$trayExe = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"
$iconPath = Join-Path $repoRoot "resources\snc_tray_icon.ico"
$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "Strategic Nexus Companion.lnk"

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
if (Test-Path -LiteralPath $iconPath) {
    $shortcut.IconLocation = "$iconPath,0"
}
$shortcut.WindowStyle = 7
$shortcut.Save()

Write-Host "snc_startup_shortcut_installed=true"
Write-Host ("snc_startup_shortcut_path=" + $shortcutPath)
Write-Host ("snc_startup_shortcut_target=" + $trayExe)
