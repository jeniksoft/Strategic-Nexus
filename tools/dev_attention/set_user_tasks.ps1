param(
    [string]$TasksJson = "",
    [string]$TaskFileJsonPath = "",
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

if ($TaskFileJsonPath) {
    $inputPath = if ([System.IO.Path]::IsPathRooted($TaskFileJsonPath)) {
        $TaskFileJsonPath
    } else {
        Join-Path $repoRoot $TaskFileJsonPath
    }
    $TasksJson = Get-Content -Raw -LiteralPath $inputPath
}

if ([string]::IsNullOrWhiteSpace($TasksJson)) {
    throw "Provide -TasksJson or -TaskFileJsonPath."
}

$tasks = $TasksJson | ConvertFrom-Json
if ($tasks -isnot [array]) {
    $tasks = @($tasks)
}

$document = [pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    tasks = @($tasks)
}

$document | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $taskPath -Encoding UTF8
Write-Host "user_tasks_written=$taskPath"
