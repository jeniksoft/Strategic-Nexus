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

$shouldAppend = $true
try {
    $header = "timestamp_local,timezone,automation_id,run_id,trigger,result,summary,details"
    $recentLines = Get-Content -LiteralPath $logPath -Tail 200 -ErrorAction Stop |
        Where-Object { $_ -and -not $_.StartsWith("timestamp_local") }

    if ($recentLines -and $recentLines.Count -gt 0) {
        $recent = ($header + "`n" + ($recentLines -join "`n") | ConvertFrom-Csv)
        if ($null -ne $recent) {
            $match = $recent | Where-Object {
                ($_.automation_id -eq $AutomationId) -and
                ($_.run_id -eq $RunId) -and
                ($_.trigger -eq $Trigger) -and
                ($_.result -eq $Result) -and
                ($_.summary -eq $Summary) -and
                ($_.details -eq $Details)
            } | Select-Object -First 1

            if ($null -ne $match) {
                $shouldAppend = $false
            }
        }
    }
} catch {
    $shouldAppend = $true
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

if ($shouldAppend) {
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $line
}

Write-Host "automation_run_logged=$logPath"
Write-Host "automation_run_id=$RunId"
Write-Host "automation_result=$Result"
