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
    [string]$WorkLogPath = ".codex_local/automation_run_log.csv",
    [string]$RunStateDirectory = ".codex_local"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

function ConvertTo-CsvField {
    param([object]$Value)

    $text = if ($null -eq $Value) { "" } else { [string]$Value }
    return '"' + $text.Replace('"', '""') + '"'
}

function Get-AutomationHintFileName {
    param([string]$Prefix, [string]$AutomationIdValue)

    $safeAutomationId = ($AutomationIdValue -replace '[^A-Za-z0-9._-]', '-').Trim('-')
    if ([string]::IsNullOrWhiteSpace($safeAutomationId)) {
        $safeAutomationId = "unknown"
    }

    return "{0}_{1}.txt" -f $Prefix, $safeAutomationId
}

function Ensure-ParentDirectory {
    param([string]$Path)

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function Write-TextFile {
    param([string]$Path, [string]$Content)

    Ensure-ParentDirectory -Path $Path
    Set-Content -LiteralPath $Path -Encoding UTF8 -Value $Content
}

function Remove-TextFileIfCurrent {
    param([string]$Path, [string]$ExpectedContent)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    try {
        $current = (Get-Content -LiteralPath $Path -Raw).Trim()
        if ($current -eq $ExpectedContent) {
            Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
        }
    } catch {
    }
}

$logPath = Resolve-ProjectPath -Path $WorkLogPath
$runStateRoot = Resolve-ProjectPath -Path $RunStateDirectory

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

$currentAutomationRunIdPath = Join-Path $runStateRoot "current_automation_run_id.txt"
$currentRunIdByAutomationPath = Join-Path $runStateRoot (Get-AutomationHintFileName -Prefix "current_run_id" -AutomationIdValue $AutomationId)
$lastAutomationRunIdPath = Join-Path $runStateRoot "last_automation_run_id.txt"
$lastRunIdByAutomationPath = Join-Path $runStateRoot (Get-AutomationHintFileName -Prefix "last_run_id" -AutomationIdValue $AutomationId)
$freeworkActiveRunIdPath = Join-Path $runStateRoot "freework_active_run_id.txt"
$isFreeworkAutomation = $AutomationId -like "sn-bounded-free-work-execution*"

if ($Result -eq "started") {
    Write-TextFile -Path $currentAutomationRunIdPath -Content $RunId
    Write-TextFile -Path $currentRunIdByAutomationPath -Content $RunId
    if ($isFreeworkAutomation) {
        Write-TextFile -Path $freeworkActiveRunIdPath -Content $RunId
    }
} else {
    Write-TextFile -Path $lastAutomationRunIdPath -Content $RunId
    Write-TextFile -Path $lastRunIdByAutomationPath -Content $RunId
    Remove-TextFileIfCurrent -Path $currentAutomationRunIdPath -ExpectedContent $RunId
    Remove-TextFileIfCurrent -Path $currentRunIdByAutomationPath -ExpectedContent $RunId
    if ($isFreeworkAutomation) {
        Remove-TextFileIfCurrent -Path $freeworkActiveRunIdPath -ExpectedContent $RunId
    }
}

Write-Host "automation_run_logged=$logPath"
Write-Host "automation_run_id=$RunId"
Write-Host "automation_result=$Result"
