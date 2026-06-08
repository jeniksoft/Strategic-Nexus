$ErrorActionPreference = "Stop"

$startupFolder = [Environment]::GetFolderPath("Startup")
$shortcutPath = if ([string]::IsNullOrWhiteSpace($startupFolder)) {
    $null
} else {
    Join-Path $startupFolder "Strategic Nexus Companion.lnk"
}

if ([string]::IsNullOrWhiteSpace($shortcutPath)) {
    throw "Windows Startup folder path is unavailable."
}

$existedBefore = Test-Path -LiteralPath $shortcutPath -PathType Leaf
if ($existedBefore) {
    Remove-Item -LiteralPath $shortcutPath -Force
}

Write-Host "snc_startup_shortcut_removed=true"
Write-Host ("snc_startup_shortcut_existed_before=" + ($(if ($existedBefore) { "true" } else { "false" })))
Write-Host ("snc_startup_shortcut_path=" + $shortcutPath)
