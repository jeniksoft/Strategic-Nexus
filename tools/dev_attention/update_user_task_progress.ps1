param(
    [Parameter(Mandatory = $true)]
    [string]$Id,
    [Parameter(Mandatory = $true)]
    [ValidateRange(0, 100)]
    [int]$ProgressPercent,
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

$found = $false

Invoke-TaskBoardJsonMutation -Path $taskPath -Mutate {
    if (-not (Test-Path -LiteralPath $taskPath)) {
        throw "Task file not found: $taskPath"
    }

    $document = Get-Content -Raw -LiteralPath $taskPath | ConvertFrom-Json
    $tasks = @($document.tasks)
    $remaining = @()

    foreach ($task in $tasks) {
        if ($task.id -eq $Id) {
            $script:found = $true
            if ($ProgressPercent -lt 100) {
                $task | Add-Member -NotePropertyName progress_percent -NotePropertyValue $ProgressPercent -Force
                $remaining += $task
            }
        } else {
            $remaining += $task
        }
    }

    if (-not $script:found) {
        throw "Task id not found: $Id"
    }

    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        tasks = @($remaining)
    }
}
$removed = if ($ProgressPercent -ge 100) { "true" } else { "false" }
Write-Host "user_task_progress_updated=$Id"
Write-Host "user_task_removed=$removed"
