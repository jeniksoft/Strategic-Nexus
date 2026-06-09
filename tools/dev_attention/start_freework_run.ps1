param(
    [string]$RepoRoot = "",
    [string]$AutomationId = "sn-bounded-free-work-execution-2",
    [string]$RunId = "",
    [ValidateSet("scheduled", "manual_ui", "unknown")]
    [string]$Trigger = "unknown",
    [int]$ForegroundLockMaxAgeMinutes = 120,
    [int]$ActiveRunMaxAgeMinutes = 45,
    [string]$GateOutputPath = "dist/private_reports/freework_concurrency_gate.json",
    [string]$RunLogPath = ".codex_local/automation_run_log.csv",
    [string]$RunStateDirectory = ".codex_local"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $repoRoot = (Resolve-Path $RepoRoot).Path
}

if ([string]::IsNullOrWhiteSpace($RunId)) {
    $RunId = [Guid]::NewGuid().ToString()
}

$gateScript = Join-Path $PSScriptRoot "test_freework_concurrency_gate.ps1"
$logScript = Join-Path $PSScriptRoot "log_automation_run.ps1"
$resolvedRunLogPath = if ([System.IO.Path]::IsPathRooted($RunLogPath)) {
    $RunLogPath
} else {
    Join-Path $repoRoot $RunLogPath
}
$resolvedRunStateDirectory = if ([System.IO.Path]::IsPathRooted($RunStateDirectory)) {
    $RunStateDirectory
} else {
    Join-Path $repoRoot $RunStateDirectory
}

& $gateScript `
    -RepoRoot $repoRoot `
    -AutomationId $AutomationId `
    -RunId $RunId `
    -ForegroundLockMaxAgeMinutes $ForegroundLockMaxAgeMinutes `
    -ActiveRunMaxAgeMinutes $ActiveRunMaxAgeMinutes `
    -RunLogPath $RunLogPath `
    -OutputPath $GateOutputPath | Out-Host

$resolvedGateOutputPath = if ([System.IO.Path]::IsPathRooted($GateOutputPath)) {
    $GateOutputPath
} else {
    Join-Path $repoRoot $GateOutputPath
}

if (-not (Test-Path -LiteralPath $resolvedGateOutputPath)) {
    & $logScript `
        -AutomationId $AutomationId `
        -RunId $RunId `
        -Trigger $Trigger `
        -Result failed `
        -Summary "concurrency gate failed" `
        -Details "gate output was not written" `
        -WorkLogPath $resolvedRunLogPath `
        -RunStateDirectory $resolvedRunStateDirectory
    throw "Free Work concurrency gate output was not written: $resolvedGateOutputPath"
}

$gate = Get-Content -Raw -LiteralPath $resolvedGateOutputPath | ConvertFrom-Json
if ([bool]$gate.should_skip) {
    $reason = [string]$gate.skip_reason
    if ([string]::IsNullOrWhiteSpace($reason)) {
        $reason = "concurrency gate requested skip"
    }
    & $logScript `
        -AutomationId $AutomationId `
        -RunId $RunId `
        -Trigger $Trigger `
        -Result quiet `
        -Summary "concurrency gate skip" `
        -Details $reason `
        -WorkLogPath $resolvedRunLogPath `
        -RunStateDirectory $resolvedRunStateDirectory | Out-Host

    Write-Host "freework_entered=false"
    Write-Host "freework_should_continue=false"
    Write-Host ("freework_run_id=" + $RunId)
    Write-Host ("freework_skip_reason=" + $reason)
    Write-Host ("freework_gate_output=" + $resolvedGateOutputPath)
    exit 0
}

& $logScript `
    -AutomationId $AutomationId `
    -RunId $RunId `
    -Trigger $Trigger `
    -Result started `
    -Summary "started" `
    -Details "concurrency gate passed" `
    -WorkLogPath $resolvedRunLogPath `
    -RunStateDirectory $resolvedRunStateDirectory | Out-Host

Write-Host "freework_entered=true"
Write-Host "freework_should_continue=true"
Write-Host ("freework_run_id=" + $RunId)
Write-Host ("freework_gate_output=" + $resolvedGateOutputPath)
