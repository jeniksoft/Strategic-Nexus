$ErrorActionPreference = "Stop"

$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = Join-Path $startupFolder "Strategic Nexus Companion.lnk"

if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

Write-Host "snc_startup_shortcut_removed=true"
Write-Host ("snc_startup_shortcut_path=" + $shortcutPath)
