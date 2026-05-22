param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json"
)

$ErrorActionPreference = "Stop"

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
