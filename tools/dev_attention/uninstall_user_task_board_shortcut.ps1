$ErrorActionPreference = "Stop"

$shortcutName = "Codex Task Board.lnk"
$legacyShortcutNames = @("Strategic Nexus Task Board.lnk")
$desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) $shortcutName
$startupShortcut = Join-Path ([Environment]::GetFolderPath("Startup")) $shortcutName
$programsFolder = [Environment]::GetFolderPath("Programs")
$startMenuFolder = Join-Path $programsFolder "Codex"
$startMenuShortcut = Join-Path $startMenuFolder $shortcutName
$legacyStartMenuFolder = Join-Path $programsFolder "Strategic Nexus"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$firstRunStartMenuShortcutMarkerPath = Join-Path $repoRoot "dist\private_reports\user_task_board_start_menu_shortcut.checked"

function Remove-LegacyTaskBoardShortcuts {
    param([string]$FolderPath)

    foreach ($legacyShortcutName in $legacyShortcutNames) {
        $legacyShortcutPath = Join-Path $FolderPath $legacyShortcutName
        Remove-Item -LiteralPath $legacyShortcutPath -Force -ErrorAction SilentlyContinue
    }
}

Remove-Item -LiteralPath $desktopShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startupShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startMenuShortcut -Force -ErrorAction SilentlyContinue
Remove-LegacyTaskBoardShortcuts -FolderPath ([Environment]::GetFolderPath("Desktop"))
Remove-LegacyTaskBoardShortcuts -FolderPath ([Environment]::GetFolderPath("Startup"))
Remove-LegacyTaskBoardShortcuts -FolderPath $startMenuFolder
Remove-LegacyTaskBoardShortcuts -FolderPath $legacyStartMenuFolder
if ((Test-Path -LiteralPath $legacyStartMenuFolder) -and -not (Get-ChildItem -LiteralPath $legacyStartMenuFolder -Force)) {
    Remove-Item -LiteralPath $legacyStartMenuFolder -Force
}
Remove-Item -LiteralPath $firstRunStartMenuShortcutMarkerPath -Force -ErrorAction SilentlyContinue

Write-Host "task_board_shortcuts_removed=true"
