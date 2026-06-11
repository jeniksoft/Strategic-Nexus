param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "task_board_json_io.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$taskPath = if ([System.IO.Path]::IsPathRooted($TaskFilePath)) {
    $TaskFilePath
} else {
    Join-Path $repoRoot $TaskFilePath
}

$directory = Split-Path -Parent $taskPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

@"
{
  "schema_version": 1,
  "updated_at": "$((Get-Date).ToString("o"))",
  "tasks": []
}
"@ | Set-Content -LiteralPath $taskPath -Encoding UTF8

Write-Host "user_tasks_cleared=$taskPath"
if (Test-TaskBoardPathMatchesDefault -RepoRoot $repoRoot -Path $taskPath -DefaultRelativePath "dist/private_reports/user_tasks.json") {
    if (Invoke-TaskBoardStateSync) {
        Write-Host "task_board_state_sync_invoked=true"
    }
}
