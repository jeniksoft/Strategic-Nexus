param()

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("sn_task_board_hygiene_" + [Guid]::NewGuid().ToString("N"))
$tasksPath = Join-Path $tempRoot "user_tasks.json"
$reportsPath = Join-Path $tempRoot "user_reports.json"
$suggestionsPath = Join-Path $tempRoot "user_suggestions.json"
$updateScript = Join-Path $repoRoot "tools/dev_attention/update_user_task_progress.cmd"
$reportScript = Join-Path $repoRoot "tools/dev_attention/add_user_report.cmd"
$boardScript = Join-Path $repoRoot "tools/dev_attention/user_task_board.cmd"

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    Ensure-Directory $Path
    $Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

try {
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

    Write-JsonFile -Path $tasksPath -Value ([pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        tasks = @(
            [pscustomobject]@{
                id = "keep-task"
                title = "Ponechat aktivni ukol"
                severity = "normal"
                status = "open"
                progress_percent = 25
            }
            [pscustomobject]@{
                id = "remove-task"
                title = "Odstranit hotovy ukol"
                severity = "normal"
                status = "open"
                progress_percent = 99
            }
        )
    })

    Write-JsonFile -Path $reportsPath -Value ([pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        reports = @()
    })

    Write-JsonFile -Path $suggestionsPath -Value ([pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        suggestions = @()
    })

    & $updateScript -Id "remove-task" -ProgressPercent 100 -TaskFilePath $tasksPath | Out-Null

    $tasksDocument = Get-Content -Raw -LiteralPath $tasksPath | ConvertFrom-Json
    $remainingTaskIds = @($tasksDocument.tasks | ForEach-Object { [string]$_.id })
    Assert-True ($remainingTaskIds.Count -eq 1) "Expected exactly one remaining task after completion cleanup."
    Assert-True ($remainingTaskIds[0] -eq "keep-task") "Expected keep-task to remain after completion cleanup."

    $listOutput = & $boardScript -Mode list -TaskFilePath $tasksPath -ReportFilePath $reportsPath -SuggestionFilePath $suggestionsPath -Top 5
    $listText = @($listOutput) -join [Environment]::NewLine
    Assert-True ($listText -match "task\|keep-task") "Task board list output did not show the remaining active task."
    Assert-True (-not ($listText -match "task\|remove-task")) "Task board list output still showed the completed task."

    $reportId = "denni-souhrn-2026-06-11-freework"
    & $reportScript -Id $reportId -Title "Denni souhrn" -Category "prace" -Summary "Prvni verze shrnuti." -Why "Test replace path." -Next "Nic." -Severity "normal" -ReportFilePath $reportsPath -ReplaceExisting | Out-Null
    & $reportScript -Id $reportId -Title "Denni souhrn" -Category "prace" -Summary "Druha verze shrnuti." -Why "Test replace path." -Next "Nic." -Severity "normal" -ReportFilePath $reportsPath -ReplaceExisting | Out-Null

    $reportsDocument = Get-Content -Raw -LiteralPath $reportsPath | ConvertFrom-Json
    $reports = @($reportsDocument.reports)
    Assert-True ($reports.Count -eq 1) "Expected ReplaceExisting to keep a single report row."
    Assert-True ([string]$reports[0].summary -eq "Druha verze shrnuti.") "Expected the replaced report summary to be updated."

    Write-Host "task_board_hygiene_test_passed=true"
    Write-Host "task_board_hygiene_remaining_tasks=$($remainingTaskIds -join ',')"
    Write-Host "task_board_hygiene_report_count=$($reports.Count)"
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
