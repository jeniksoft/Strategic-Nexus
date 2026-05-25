param(
    [Parameter(Mandatory = $true)]
    [string]$StartedAt,
    [Parameter(Mandatory = $true)]
    [string]$EndedAt,
    [string]$Mode = "interactive",
    [string]$RoadmapItem = "",
    [int]$ComplexityPointsDone = 0,
    [string]$Summary = "",
    [string]$CommitSha = "",
    [string]$Files = "",
    [string]$Notes = "",
    [string]$WorkLogPath = ".codex_local/implementation_work_log.csv"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$logPath = if ([System.IO.Path]::IsPathRooted($WorkLogPath)) {
    $WorkLogPath
} else {
    Join-Path $repoRoot $WorkLogPath
}

function ConvertTo-CsvField {
    param([object]$Value)

    $text = if ($null -eq $Value) { "" } else { [string]$Value }
    return '"' + $text.Replace('"', '""') + '"'
}

$start = [DateTime]::Parse($StartedAt)
$end = [DateTime]::Parse($EndedAt)
if ($end -lt $start) {
    throw "EndedAt must be after StartedAt."
}

$durationMinutes = [int][Math]::Round(($end - $start).TotalMinutes)

$directory = Split-Path -Parent $logPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

if (-not (Test-Path -LiteralPath $logPath)) {
    Set-Content -LiteralPath $logPath -Encoding UTF8 -Value "started_at,ended_at,duration_minutes,mode,roadmap_item,complexity_points_done,summary,commit_sha,files,notes"
}

$line = @(
    ConvertTo-CsvField $start.ToString("o")
    ConvertTo-CsvField $end.ToString("o")
    ConvertTo-CsvField $durationMinutes
    ConvertTo-CsvField $Mode
    ConvertTo-CsvField $RoadmapItem
    ConvertTo-CsvField $ComplexityPointsDone
    ConvertTo-CsvField $Summary
    ConvertTo-CsvField $CommitSha
    ConvertTo-CsvField $Files
    ConvertTo-CsvField $Notes
) -join ","

Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $line

Write-Host "implementation_work_logged=$logPath"
Write-Host "implementation_duration_minutes=$durationMinutes"
