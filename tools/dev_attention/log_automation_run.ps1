param(
    [Parameter(Mandatory = $true)]
    [string]$AutomationId,
    [string]$RunId = "",
    [ValidateSet("scheduled", "manual_ui", "unknown")]
    [string]$Trigger = "unknown",
    [ValidateSet("started", "implemented", "blocked", "quiet", "no_safe_task", "failed")]
    [string]$Result = "started",
    [string]$Summary = "",
    [string]$Details = "",
    [string]$WorkLogPath = ".codex_local/automation_run_log.csv"
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

if ([string]::IsNullOrWhiteSpace($RunId)) {
    $RunId = "run-" + (Get-Date).ToString("yyyyMMddHHmmss") + "-" + ([Guid]::NewGuid().ToString("N").Substring(0, 8))
}

$directory = Split-Path -Parent $logPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

if (-not (Test-Path -LiteralPath $logPath)) {
    Set-Content -LiteralPath $logPath -Encoding UTF8 -Value "timestamp_local,timezone,automation_id,run_id,trigger,result,summary,details"
}

$line = @(
    ConvertTo-CsvField (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    ConvertTo-CsvField ([System.TimeZoneInfo]::Local.Id)
    ConvertTo-CsvField $AutomationId
    ConvertTo-CsvField $RunId
    ConvertTo-CsvField $Trigger
    ConvertTo-CsvField $Result
    ConvertTo-CsvField $Summary
    ConvertTo-CsvField $Details
) -join ","

Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $line

Write-Host "automation_run_logged=$logPath"
Write-Host "automation_run_id=$RunId"
Write-Host "automation_result=$Result"
