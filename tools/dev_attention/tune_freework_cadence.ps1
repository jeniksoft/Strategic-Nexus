param(
    [string]$UsageBudgetStatePath = ".codex_local/usage_budget_state.md",
    [string]$ProjectProgressPath = "dist/private_reports/project_progress_estimate.json",
    [string]$FreeworkAutomationPath = "$env:USERPROFILE\.codex\automations\sn-bounded-free-work-execution\automation.toml",
    [string]$OutputPath = "dist/private_reports/freework_cadence_recommendation.json",
    [int]$TargetReservePercent = 5
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

function Get-UsageBudgetState {
    param([string]$Path)

    $state = [pscustomobject]@{
        remaining_percent = $null
        next_reset_local = $null
        reading_source = "missing"
        reset_source = "missing"
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        return $state
    }

    $text = Get-Content -Raw -LiteralPath $Path
    $remainingMatch = [regex]::Match($text, '(?ms)Latest declared remaining weekly usage:\s*```text\s*(?<value>[0-9]+)%')
    if ($remainingMatch.Success) {
        $state.remaining_percent = [int]$remainingMatch.Groups["value"].Value
        $state.reading_source = "latest-declared-section"
    }

    $entryMatches = [regex]::Matches($text, '(?m)^(?<timestamp>[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}(?::[0-9]{2})? [^|]+)\|\s*(?<remaining>[0-9]+)% remaining\s*\|')
    if ($entryMatches.Count -gt 0) {
        $latestEntry = $entryMatches[$entryMatches.Count - 1]
        $state.remaining_percent = [int]$latestEntry.Groups["remaining"].Value
        $state.reading_source = "latest-observation-log-entry"
    }

    $resetMatch = [regex]::Match($text, '(?ms)Estimated next reset:\s*```text\s*(?<value>[0-9]{4}-[0-9]{2}-[0-9]{2}(?:\s+[0-9]{2}:[0-9]{2})?)')
    if ($resetMatch.Success) {
        $resetValue = $resetMatch.Groups["value"].Value
        $resetFormat = if ($resetValue.Length -gt 10) { "yyyy-MM-dd HH:mm" } else { "yyyy-MM-dd" }
        $state.next_reset_local = [DateTime]::ParseExact($resetValue, $resetFormat, [System.Globalization.CultureInfo]::InvariantCulture)
        $state.reset_source = "estimated-next-reset-section"
    }

    return $state
}

function Get-ProjectRemainingPoints {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    try {
        $json = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
        if ($null -ne $json.remaining_complexity_points) {
            return [double]$json.remaining_complexity_points
        }
        if ($null -ne $json.basis) {
            $remainingMatch = [regex]::Match([string]$json.basis, "(?<value>[0-9]+(?:\.[0-9]+)?)\s+zbyva")
            if ($remainingMatch.Success) {
                return [double]::Parse($remainingMatch.Groups["value"].Value, [System.Globalization.CultureInfo]::InvariantCulture)
            }
        }
        if ($null -ne $json.percent) {
            return [Math]::Max(0, 100 - [double]$json.percent)
        }
    } catch {
    }
    return $null
}

function Get-CurrentFreeworkRRule {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    $match = Select-String -LiteralPath $Path -Pattern '^rrule\s*=\s*"(?<rrule>[^"]+)"' | Select-Object -First 1
    if ($match) {
        return [string]$match.Matches[0].Groups["rrule"].Value
    }
    return ""
}

function Get-RRuleForHours {
    param([int]$Hours)

    if ($Hours -le 1) {
        return "FREQ=HOURLY;INTERVAL=1"
    }
    return "FREQ=HOURLY;INTERVAL=$Hours"
}

$usagePath = Resolve-ProjectPath $UsageBudgetStatePath
$progressPath = Resolve-ProjectPath $ProjectProgressPath
$outputResolvedPath = Resolve-ProjectPath $OutputPath

$usage = Get-UsageBudgetState -Path $usagePath
$remainingPercent = if ($null -ne $usage.remaining_percent) { [int]$usage.remaining_percent } else { 60 }
$now = Get-Date
$hoursToReset = $null
if ($null -ne $usage.next_reset_local) {
    $hoursToReset = [Math]::Max(0, ($usage.next_reset_local - $now).TotalHours)
}

$remainingPoints = Get-ProjectRemainingPoints -Path $progressPath
$hasUsefulProjectWork = $true
if ($null -ne $remainingPoints -and $remainingPoints -le 1) {
    $hasUsefulProjectWork = $false
}

$intervalHours = 4
$chunkMode = "normal"
$reason = "default conservative cadence"

if (-not $hasUsefulProjectWork) {
    $intervalHours = 24
    $chunkMode = "none-unless-requested"
    $reason = "project estimate says there is no meaningful remaining roadmap work"
} elseif ($remainingPercent -lt 40) {
    $intervalHours = 12
    $chunkMode = "narrow-critical-only"
    $reason = "remaining budget below 40%; preserve capacity for owner decisions and blockers"
} elseif ($remainingPercent -lt 60) {
    $intervalHours = 6
    $chunkMode = "narrow-important"
    $reason = "remaining budget between 40% and 60%"
} elseif ($remainingPercent -lt 80) {
    $intervalHours = 4
    $chunkMode = "focused"
    $reason = "remaining budget between 60% and 80%"
} else {
    $intervalHours = 2
    $chunkMode = "standard-one-bounded-chunk"
    $reason = "remaining budget above 80%"
}

if ($hasUsefulProjectWork -and $null -ne $hoursToReset) {
    $targetReserve = [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))
    $spendableToReserve = [Math]::Max(0, $remainingPercent - $targetReserve)
    $targetSpendPerDay = if ($hoursToReset -gt 0) {
        $spendableToReserve / $hoursToReset * 24.0
    } else {
        0.0
    }

    if ($spendableToReserve -le 0) {
        $intervalHours = 24
        $chunkMode = "reserve-only"
        $reason = "remaining budget is already at or below target reserve"
    } elseif ($targetSpendPerDay -ge 18) {
        $intervalHours = 1
        $chunkMode = "frequent-one-bounded-chunk"
        $reason = "target 5% reserve at reset requires high burn rate"
    } elseif ($targetSpendPerDay -ge 10) {
        $intervalHours = 2
        $chunkMode = "standard-one-bounded-chunk"
        $reason = "target 5% reserve at reset requires faster than default cadence"
    } elseif ($targetSpendPerDay -ge 6) {
        $intervalHours = 3
        $chunkMode = "focused"
        $reason = "target 5% reserve at reset requires moderate cadence"
    } elseif ($remainingPercent -ge 85 -and $hoursToReset -le 48) {
        $intervalHours = 1
        $chunkMode = "frequent-one-bounded-chunk"
        $reason = "high remaining budget with less than 48 hours to reset"
    } elseif ($remainingPercent -ge 90 -and $hoursToReset -le 96) {
        $intervalHours = 2
        $chunkMode = "standard-one-bounded-chunk"
        $reason = "very high remaining budget inside final four days"
    }
}

$recommendedRRule = Get-RRuleForHours -Hours $intervalHours
$currentRRule = Get-CurrentFreeworkRRule -Path $FreeworkAutomationPath
$shouldUpdateAutomation = $currentRRule -and ($currentRRule -ne $recommendedRRule)

$directory = Split-Path -Parent $outputResolvedPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

[pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    remaining_budget_percent = $remainingPercent
    remaining_budget_reading_source = $usage.reading_source
    hours_to_estimated_reset = if ($null -ne $hoursToReset) { [Math]::Round($hoursToReset, 1) } else { $null }
    reset_reading_source = $usage.reset_source
    target_reserve_percent_at_reset = [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))
    spendable_budget_before_reserve_percent = if ($null -ne $hoursToReset) { [Math]::Max(0, $remainingPercent - [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))) } else { $null }
    target_spend_percent_per_day_until_reset = if ($null -ne $hoursToReset -and $hoursToReset -gt 0) { [Math]::Round(([Math]::Max(0, $remainingPercent - [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))) / $hoursToReset * 24.0), 2) } else { $null }
    remaining_project_complexity_points = $remainingPoints
    has_useful_project_work = $hasUsefulProjectWork
    recommended_freework_rrule = $recommendedRRule
    recommended_interval_hours = $intervalHours
    recommended_chunk_mode = $chunkMode
    current_freework_rrule = $currentRRule
    should_update_freework_automation = [bool]$shouldUpdateAutomation
    reason = $reason
    safety_note = "Increase cadence instead of overlapping multiple implementation chunks; keep one bounded chunk per run unless explicitly approved."
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outputResolvedPath -Encoding UTF8

Write-Host "freework_cadence_recommendation_written=$outputResolvedPath"
Write-Host "recommended_freework_rrule=$recommendedRRule"
Write-Host "current_freework_rrule=$currentRRule"
Write-Host "should_update_freework_automation=$shouldUpdateAutomation"
