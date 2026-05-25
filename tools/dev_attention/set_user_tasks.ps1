param(
    [string]$TasksJson = "",
    [string]$TaskFileJsonPath = "",
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

Invoke-TaskBoardJsonMutation -Path $taskPath -Mutate {
    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        tasks = @($tasks)
    }
}
Write-Host "user_tasks_written=$taskPath"
