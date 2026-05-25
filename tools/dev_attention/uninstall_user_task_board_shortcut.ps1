$ErrorActionPreference = "Stop"

$desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Strategic Nexus Task Board.lnk"
$startupShortcut = Join-Path ([Environment]::GetFolderPath("Startup")) "Strategic Nexus Task Board.lnk"
$startMenuShortcut = Join-Path (Join-Path ([Environment]::GetFolderPath("Programs")) "Strategic Nexus") "Strategic Nexus Task Board.lnk"
$startMenuFolder = Split-Path -Parent $startMenuShortcut
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$firstRunStartMenuShortcutMarkerPath = Join-Path $repoRoot "dist\private_reports\user_task_board_start_menu_shortcut.checked"

Remove-Item -LiteralPath $desktopShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startupShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startMenuShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startMenuFolder -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $firstRunStartMenuShortcutMarkerPath -Force -ErrorAction SilentlyContinue

Write-Host "task_board_shortcuts_removed=true"
