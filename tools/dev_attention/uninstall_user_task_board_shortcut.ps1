$ErrorActionPreference = "Stop"

$desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Strategic Nexus Task Board.lnk"
$startupShortcut = Join-Path ([Environment]::GetFolderPath("Startup")) "Strategic Nexus Task Board.lnk"

Remove-Item -LiteralPath $desktopShortcut -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $startupShortcut -Force -ErrorAction SilentlyContinue

Write-Host "task_board_shortcuts_removed=true"
