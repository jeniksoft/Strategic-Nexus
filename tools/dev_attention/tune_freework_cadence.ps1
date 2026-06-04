param(
    [string]$UsageBudgetStatePath = ".codex_local/usage_budget_state.md",
    [string]$UsageBudgetLogPath = ".codex_local/usage_budget_log.csv",
    [string]$BudgetRulesPath = "docs/FREE_WORK_AND_USAGE_BUDGET_RULES.md",
    [string]$ProjectProgressPath = "dist/private_reports/project_progress_estimate.json",
    [string]$FreeworkAutomationPath = "",
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

function Get-UsageBurnRate {
    param(
        [string]$Path,
        [DateTime]$ResetAnchor
    )

    $result = [pscustomobject]@{
        points_per_day = $null
        sample_count = 0
        window_hours = $null
        first_timestamp = $null
        last_timestamp = $null
        source = "missing"
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        return $result
    }

    try {
        $rows = Import-Csv -LiteralPath $Path
    } catch {
        $result.source = "unreadable"
        return $result
    }

    $samples = New-Object System.Collections.Generic.List[object]
    foreach ($row in $rows) {
        $timestampText = [string]$row.timestamp_local
        $usedText = [string]$row.used_percent
        if ([string]::IsNullOrWhiteSpace($timestampText) -or [string]::IsNullOrWhiteSpace($usedText)) {
            continue
        }
        $timestamp = [DateTime]::MinValue
        if (-not [DateTime]::TryParse($timestampText, [ref]$timestamp)) {
            continue
        }
        if ($timestamp -lt $ResetAnchor) {
            continue
        }
        $used = 0.0
        if (-not [double]::TryParse($usedText, [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$used)) {
            continue
        }
        $samples.Add([pscustomobject]@{
            timestamp = $timestamp
            used = $used
        }) | Out-Null
    }

    $ordered = @($samples | Sort-Object timestamp)
    if ($ordered.Count -lt 2) {
        $result.source = "insufficient-samples"
        $result.sample_count = $ordered.Count
        return $result
    }

    $deduped = New-Object System.Collections.Generic.List[object]
    $lastUsed = $null
    foreach ($sample in $ordered) {
        if ($null -eq $lastUsed -or [Math]::Abs($sample.used - $lastUsed) -gt 0.001) {
            $deduped.Add($sample) | Out-Null
            $lastUsed = $sample.used
        } elseif ($deduped.Count -gt 0) {
            $deduped[$deduped.Count - 1] = $sample
        }
    }

    if ($deduped.Count -lt 2) {
        $result.source = "insufficient-changing-samples"
        $result.sample_count = $deduped.Count
        return $result
    }

    $first = $deduped[0]
    $last = $deduped[$deduped.Count - 1]
    $windowHours = [Math]::Max(0.0, ($last.timestamp - $first.timestamp).TotalHours)
    $usedDelta = $last.used - $first.used
    if ($windowHours -le 0.1 -or $usedDelta -lt 0) {
        $result.source = "invalid-window"
        $result.sample_count = $deduped.Count
        return $result
    }

    $result.points_per_day = $usedDelta / $windowHours * 24.0
    $result.sample_count = $deduped.Count
    $result.window_hours = $windowHours
    $result.first_timestamp = $first.timestamp
    $result.last_timestamp = $last.timestamp
    $result.source = "usage-budget-log-changing-samples"
    return $result
}

function Get-RecentUsageBudgetPair {
    param(
        [string]$Path,
        [DateTime]$ResetAnchor,
        [int]$TargetReservePercent,
        $CurrentHoursToReset
    )

    $result = [pscustomobject]@{
        available = $false
        source = "missing"
        previous_timestamp = $null
        current_timestamp = $null
        previous_remaining_percent = $null
        current_remaining_percent = $null
        interval_hours = $null
        expected_spend_percent = $null
        actual_spend_percent = $null
        raw_scale = $null
        scale = $null
        scale_min = 0.5
        scale_max = 2.0
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        return $result
    }

    try {
        $rows = Import-Csv -LiteralPath $Path
    } catch {
        $result.source = "unreadable"
        return $result
    }

    $samples = New-Object System.Collections.Generic.List[object]
    foreach ($row in $rows) {
        $timestampText = [string]$row.timestamp_local
        $remainingText = [string]$row.remaining_percent
        if ([string]::IsNullOrWhiteSpace($timestampText) -or [string]::IsNullOrWhiteSpace($remainingText)) {
            continue
        }

        $timestamp = [DateTime]::MinValue
        if (-not [DateTime]::TryParse($timestampText, [ref]$timestamp)) {
            continue
        }
        if ($ResetAnchor -ne [DateTime]::MinValue -and $timestamp -lt $ResetAnchor) {
            continue
        }

        $remaining = 0.0
        if (-not [double]::TryParse($remainingText, [System.Globalization.NumberStyles]::Float, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$remaining)) {
            continue
        }
        if ($remaining -lt 0 -or $remaining -gt 100) {
            continue
        }

        $samples.Add([pscustomobject]@{
            timestamp = $timestamp
            remaining = $remaining
        }) | Out-Null
    }

    $ordered = @($samples | Sort-Object timestamp)
    if ($ordered.Count -lt 2) {
        $result.source = "insufficient-samples"
        return $result
    }

    $previous = $ordered[$ordered.Count - 2]
    $current = $ordered[$ordered.Count - 1]
    $intervalHours = ($current.timestamp - $previous.timestamp).TotalHours
    $actualSpend = $previous.remaining - $current.remaining

    $result.previous_timestamp = $previous.timestamp.ToString("o")
    $result.current_timestamp = $current.timestamp.ToString("o")
    $result.previous_remaining_percent = [Math]::Round($previous.remaining, 2)
    $result.current_remaining_percent = [Math]::Round($current.remaining, 2)
    $result.interval_hours = [Math]::Round($intervalHours, 3)
    $result.actual_spend_percent = [Math]::Round($actualSpend, 3)

    if ($intervalHours -le 0.01) {
        $result.source = "invalid-interval"
        return $result
    }
    if ($actualSpend -lt 0) {
        $result.source = "reset-or-correction-detected"
        return $result
    }
    if ($null -eq $CurrentHoursToReset -or $CurrentHoursToReset -le 0) {
        $result.source = "missing-reset-window"
        return $result
    }

    $targetReserve = [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))
    $previousHoursToReset = [Math]::Max(0.01, [double]$CurrentHoursToReset + $intervalHours)
    $spendableAtPreviousCheck = [Math]::Max(0.0, $previous.remaining - $targetReserve)
    $expectedSpend = $spendableAtPreviousCheck * $intervalHours / $previousHoursToReset
    $result.expected_spend_percent = [Math]::Round($expectedSpend, 3)

    if ($expectedSpend -le 0.001) {
        $result.source = "expected-spend-too-small"
        return $result
    }

    $rawScale = $actualSpend / $expectedSpend
    $scale = [Math]::Max($result.scale_min, [Math]::Min($result.scale_max, $rawScale))
    $result.raw_scale = [Math]::Round($rawScale, 3)
    $result.scale = [Math]::Round($scale, 3)
    $result.source = "latest-budget-check-pair"
    $result.available = $true
    return $result
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

function Get-TomlStringValue {
    param(
        [string]$Path,
        [string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    $pattern = "^\s*$([regex]::Escape($Name))\s*=\s*`"(?<value>[^`"]*)`""
    $match = Select-String -LiteralPath $Path -Pattern $pattern | Select-Object -First 1
    if ($match) {
        return [string]$match.Matches[0].Groups["value"].Value
    }
    return ""
}

function Get-TomlIntValue {
    param(
        [string]$Path,
        [string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    $pattern = "^\s*$([regex]::Escape($Name))\s*=\s*(?<value>[0-9]+)\s*$"
    $match = Select-String -LiteralPath $Path -Pattern $pattern | Select-Object -First 1
    if ($match) {
        return [int64]$match.Matches[0].Groups["value"].Value
    }
    return $null
}

function Resolve-FreeworkAutomationPath {
    param([string]$Path)

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        $explicitPath = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Resolve-ProjectPath $Path }
        if (Test-Path -LiteralPath $explicitPath) {
            return $explicitPath
        }
    }

    $automationRoot = Join-Path $env:USERPROFILE ".codex\automations"
    if (-not (Test-Path -LiteralPath $automationRoot)) {
        return ""
    }

    $candidates = New-Object System.Collections.Generic.List[object]
    foreach ($toml in Get-ChildItem -LiteralPath $automationRoot -Directory | ForEach-Object { Join-Path $_.FullName "automation.toml" }) {
        if (-not (Test-Path -LiteralPath $toml)) {
            continue
        }

        $id = Get-TomlStringValue -Path $toml -Name "id"
        $name = Get-TomlStringValue -Path $toml -Name "name"
        $status = Get-TomlStringValue -Path $toml -Name "status"
        if ($status -ne "ACTIVE") {
            continue
        }
        if ($id -notlike "sn-bounded-free-work-execution*" -and $name -ne "SN Bounded Free Work Execution") {
            continue
        }

        $item = Get-Item -LiteralPath $toml
        $candidates.Add([pscustomobject]@{
            path = $item.FullName
            id = $id
            name = $name
            last_write_time = $item.LastWriteTime
        }) | Out-Null
    }

    $selected = $candidates |
        Sort-Object `
            @{ Expression = { if ($_.id -ne "sn-bounded-free-work-execution") { 0 } else { 1 } }; Ascending = $true },
            @{ Expression = { $_.last_write_time }; Descending = $true } |
        Select-Object -First 1

    if ($selected) {
        return [string]$selected.path
    }

    return ""
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

function Get-RRuleForMinutes {
    param([int]$Minutes)

    if ($Minutes -lt 60 -or ($Minutes % 60) -ne 0) {
        return "FREQ=MINUTELY;INTERVAL=$Minutes"
    }
    $hours = [Math]::Max(1, [int]($Minutes / 60))
    return Get-RRuleForHours -Hours $hours
}

function Get-RRuleIntervalMinutes {
    param([string]$RRule)

    if ([string]::IsNullOrWhiteSpace($RRule)) {
        return $null
    }

    $interval = 1
    $intervalMatch = [regex]::Match($RRule, '(?:^|;)INTERVAL=(?<value>[0-9]+)(?:;|$)')
    if ($intervalMatch.Success) {
        $interval = [int]$intervalMatch.Groups["value"].Value
    }

    if ($RRule -match '(?:^|;)FREQ=MINUTELY(?:;|$)') {
        return [Math]::Max(1, $interval)
    }
    if ($RRule -match '(?:^|;)FREQ=HOURLY(?:;|$)') {
        return [Math]::Max(1, $interval) * 60
    }
    if ($RRule -match '(?:^|;)FREQ=DAILY(?:;|$)') {
        return [Math]::Max(1, $interval) * 1440
    }

    return $null
}

function Clamp-IntervalMinutes {
    param(
        [double]$Minutes,
        [int]$MinimumMinutes = 5,
        [int]$MaximumMinutes = 1440
    )

    $rounded = [int][Math]::Round($Minutes)
    return [Math]::Max($MinimumMinutes, [Math]::Min($MaximumMinutes, $rounded))
}

function Get-PreviousCadenceRecommendation {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    try {
        return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
    } catch {
        return $null
    }
}

$usagePath = Resolve-ProjectPath $UsageBudgetStatePath
$usageLogPath = Resolve-ProjectPath $UsageBudgetLogPath
$budgetRulesResolvedPath = Resolve-ProjectPath $BudgetRulesPath
$progressPath = Resolve-ProjectPath $ProjectProgressPath
$outputResolvedPath = Resolve-ProjectPath $OutputPath
$freeworkAutomationResolvedPath = Resolve-FreeworkAutomationPath -Path $FreeworkAutomationPath
$freeworkAutomationId = if ($freeworkAutomationResolvedPath) { Get-TomlStringValue -Path $freeworkAutomationResolvedPath -Name "id" } else { "" }
$freeworkAutomationName = if ($freeworkAutomationResolvedPath) { Get-TomlStringValue -Path $freeworkAutomationResolvedPath -Name "name" } else { "" }
$freeworkAutomationUpdatedAtUnixMs = if ($freeworkAutomationResolvedPath) { Get-TomlIntValue -Path $freeworkAutomationResolvedPath -Name "updated_at" } else { $null }
$freeworkAutomationUpdatedAt = if ($null -ne $freeworkAutomationUpdatedAtUnixMs) { [DateTimeOffset]::FromUnixTimeMilliseconds($freeworkAutomationUpdatedAtUnixMs).LocalDateTime } else { $null }
$previousRecommendation = Get-PreviousCadenceRecommendation -Path $outputResolvedPath

if (-not (Test-Path -LiteralPath $budgetRulesResolvedPath)) {
    throw "Budget rules file not found: $budgetRulesResolvedPath"
}

$usage = Get-UsageBudgetState -Path $usagePath
$remainingPercent = if ($null -ne $usage.remaining_percent) { [int]$usage.remaining_percent } else { 60 }
$now = Get-Date
$hoursToReset = $null
$resetAnchor = $null
if ($null -ne $usage.next_reset_local) {
    $hoursToReset = [Math]::Max(0, ($usage.next_reset_local - $now).TotalHours)
    $resetAnchor = $usage.next_reset_local.AddDays(-7)
}
$burnRate = if ($null -ne $resetAnchor) { Get-UsageBurnRate -Path $usageLogPath -ResetAnchor $resetAnchor } else { Get-UsageBurnRate -Path $usageLogPath -ResetAnchor ([DateTime]::MinValue) }

$remainingPoints = Get-ProjectRemainingPoints -Path $progressPath
$hasUsefulProjectWork = $true
if ($null -ne $remainingPoints -and $remainingPoints -le 1) {
    $hasUsefulProjectWork = $false
}

$intervalHours = 4
$chunkMode = "normal"
$reason = "default conservative cadence"
$targetReserve = [Math]::Max(0, [Math]::Min(50, $TargetReservePercent))
$spendableToReserve = if ($null -ne $hoursToReset) { [Math]::Max(0, $remainingPercent - $targetReserve) } else { $null }
$targetSpendPerDay = if ($null -ne $hoursToReset -and $hoursToReset -gt 0) {
    $spendableToReserve / $hoursToReset * 24.0
} else {
    $null
}
$burnRateDeltaPerDay = if ($null -ne $burnRate.points_per_day -and $null -ne $targetSpendPerDay) {
    $burnRate.points_per_day - $targetSpendPerDay
} else {
    $null
}
$burnRateInterpretation = if ($null -eq $burnRate.points_per_day -or $null -eq $targetSpendPerDay) {
    "insufficient-history"
} elseif ($burnRateDeltaPerDay -lt -4) {
    "behind-target-burn"
} elseif ($burnRateDeltaPerDay -gt 4) {
    "ahead-of-target-burn"
} else {
    "near-target-burn"
}

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

    if ($burnRateInterpretation -eq "ahead-of-target-burn" -and $intervalHours -lt 4) {
        $intervalHours = [Math]::Min(4, $intervalHours + 1)
        $chunkMode = "controlled-one-bounded-chunk"
        $reason = "$reason; historical burn is ahead of target, so cadence is softened"
    } elseif ($burnRateInterpretation -eq "behind-target-burn" -and $intervalHours -gt 1 -and $spendableToReserve -gt 20) {
        $intervalHours = [Math]::Max(1, $intervalHours - 1)
        $chunkMode = "catch-up-one-bounded-chunk"
        $reason = "$reason; historical burn is behind target, so cadence is tightened"
    }
}

$currentRRule = Get-CurrentFreeworkRRule -Path $freeworkAutomationResolvedPath
$currentIntervalMinutes = Get-RRuleIntervalMinutes -RRule $currentRRule
$intervalMinutes = $intervalHours * 60
$fallbackIntervalMinutes = $intervalMinutes
$adaptiveCadence = if ($null -ne $resetAnchor) {
    Get-RecentUsageBudgetPair -Path $usageLogPath -ResetAnchor $resetAnchor -TargetReservePercent $targetReserve -CurrentHoursToReset $hoursToReset
} else {
    Get-RecentUsageBudgetPair -Path $usageLogPath -ResetAnchor ([DateTime]::MinValue) -TargetReservePercent $targetReserve -CurrentHoursToReset $hoursToReset
}
$adaptiveInputIntervalMinutes = if ($null -ne $currentIntervalMinutes) { $currentIntervalMinutes } else { $fallbackIntervalMinutes }
$adaptiveUnclampedIntervalMinutes = $null
$adaptiveApplied = $false
$adaptiveAlreadyAppliedForPair = $false

if ($adaptiveCadence.available -and $null -ne $freeworkAutomationUpdatedAt) {
    $adaptiveCurrentTimestamp = [DateTime]::MinValue
    if ([DateTime]::TryParse([string]$adaptiveCadence.current_timestamp, [ref]$adaptiveCurrentTimestamp)) {
        if ($freeworkAutomationUpdatedAt -ge $adaptiveCurrentTimestamp) {
            $adaptiveAlreadyAppliedForPair = $true
        }
    }
}

if (
    -not $adaptiveAlreadyAppliedForPair -and
    $adaptiveCadence.available -and
    $null -ne $previousRecommendation -and
    $previousRecommendation.adaptive_previous_budget_check_at -eq $adaptiveCadence.previous_timestamp -and
    $previousRecommendation.adaptive_current_budget_check_at -eq $adaptiveCadence.current_timestamp -and
    $previousRecommendation.recommended_freework_rrule -eq $currentRRule
) {
    $adaptiveAlreadyAppliedForPair = $true
}

if (
    $hasUsefulProjectWork -and
    $remainingPercent -ge 40 -and
    $null -ne $spendableToReserve -and
    $spendableToReserve -gt 0 -and
    $adaptiveCadence.available -and
    -not $adaptiveAlreadyAppliedForPair
) {
    $adaptiveUnclampedIntervalMinutes = $adaptiveInputIntervalMinutes * [double]$adaptiveCadence.scale
    $intervalMinutes = Clamp-IntervalMinutes -Minutes $adaptiveUnclampedIntervalMinutes
    $adaptiveApplied = $true
    $chunkMode = if ([double]$adaptiveCadence.scale -lt 1.0) {
        "adaptive-catch-up-bounded"
    } elseif ([double]$adaptiveCadence.scale -gt 1.0) {
        "adaptive-controlled-bounded"
    } else {
        "adaptive-on-target-bounded"
    }
    $reason = "adaptive cadence: actual budget spend D=$($adaptiveCadence.actual_spend_percent)% versus expected K=$($adaptiveCadence.expected_spend_percent)% over I=$($adaptiveCadence.interval_hours)h; applying S=$($adaptiveCadence.scale) to current Free Work interval"
} elseif ($adaptiveAlreadyAppliedForPair -and $null -ne $currentIntervalMinutes) {
    $intervalMinutes = $currentIntervalMinutes
    if ($previousRecommendation.recommended_chunk_mode) {
        $chunkMode = [string]$previousRecommendation.recommended_chunk_mode
    }
    $reason = "adaptive cadence already applied for the latest budget-check pair; keeping current Free Work interval until a new budget reading arrives"
}

$nearResetCapMinutes = $null
$nearResetCapMode = $null
$nearResetCapReason = $null
if (
    $hasUsefulProjectWork -and
    $null -ne $hoursToReset -and
    $hoursToReset -le 12 -and
    $spendableToReserve -ge 30 -and
    $remainingPercent -ge 40
) {
    $nearResetCapMinutes = 15
    $nearResetCapMode = "rapid-near-reset-bounded"
    $nearResetCapReason = "owner-approved rapid near-reset cadence cap"

    if ($hoursToReset -le 3 -and $spendableToReserve -ge 20) {
        $nearResetCapMinutes = 10
        $nearResetCapMode = "very-rapid-near-reset-bounded"
        $nearResetCapReason = "owner-approved final-window cadence cap"
    }

    if ($hoursToReset -le 8 -and $spendableToReserve -ge 40 -and $remainingPercent -ge 50) {
        $nearResetCapMinutes = 5
        $nearResetCapMode = "urgent-near-reset-bounded"
        $nearResetCapReason = "owner-approved urgent near-reset cadence cap"
    }
}

if ($null -ne $nearResetCapMinutes -and $intervalMinutes -gt $nearResetCapMinutes) {
    $intervalMinutes = $nearResetCapMinutes
    $chunkMode = $nearResetCapMode
    $reason = "$reason; $nearResetCapReason lowers maximum interval to $nearResetCapMinutes minutes"
}

$recommendedRRule = Get-RRuleForMinutes -Minutes $intervalMinutes
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
    budget_rules_path = $budgetRulesResolvedPath
    budget_rules_found = $true
    hours_to_estimated_reset = if ($null -ne $hoursToReset) { [Math]::Round($hoursToReset, 1) } else { $null }
    reset_reading_source = $usage.reset_source
    target_reserve_percent_at_reset = $targetReserve
    spendable_budget_before_reserve_percent = $spendableToReserve
    target_spend_percent_per_day_until_reset = if ($null -ne $targetSpendPerDay) { [Math]::Round($targetSpendPerDay, 2) } else { $null }
    historical_burn_percent_per_day = if ($null -ne $burnRate.points_per_day) { [Math]::Round($burnRate.points_per_day, 2) } else { $null }
    historical_burn_window_hours = if ($null -ne $burnRate.window_hours) { [Math]::Round($burnRate.window_hours, 1) } else { $null }
    historical_burn_sample_count = $burnRate.sample_count
    historical_burn_source = $burnRate.source
    burn_rate_vs_target = $burnRateInterpretation
    adaptive_cadence_available = [bool]$adaptiveCadence.available
    adaptive_cadence_source = $adaptiveCadence.source
    adaptive_previous_budget_check_at = $adaptiveCadence.previous_timestamp
    adaptive_current_budget_check_at = $adaptiveCadence.current_timestamp
    adaptive_budget_check_interval_hours_I = $adaptiveCadence.interval_hours
    adaptive_expected_spend_percent_K = $adaptiveCadence.expected_spend_percent
    adaptive_actual_spend_percent_D = $adaptiveCadence.actual_spend_percent
    adaptive_raw_scale_D_over_K = $adaptiveCadence.raw_scale
    adaptive_applied_scale_S = $adaptiveCadence.scale
    adaptive_current_interval_minutes_T = $adaptiveInputIntervalMinutes
    adaptive_unclamped_interval_minutes_T_times_S = if ($null -ne $adaptiveUnclampedIntervalMinutes) { [Math]::Round($adaptiveUnclampedIntervalMinutes, 2) } else { $null }
    adaptive_applied = [bool]$adaptiveApplied
    remaining_project_complexity_points = $remainingPoints
    has_useful_project_work = $hasUsefulProjectWork
    freework_automation_id = $freeworkAutomationId
    freework_automation_name = $freeworkAutomationName
    freework_automation_path = $freeworkAutomationResolvedPath
    recommended_freework_rrule = $recommendedRRule
    recommended_interval_hours = [Math]::Round($intervalMinutes / 60.0, 2)
    recommended_interval_minutes = $intervalMinutes
    recommended_chunk_mode = $chunkMode
    current_freework_rrule = $currentRRule
    should_update_freework_automation = [bool]$shouldUpdateAutomation
    reason = $reason
    safety_note = "Rapid cadence must not overlap heavy implementation in the same worktree. Parallel heavy runs are allowed only with isolated worktrees/branches or a durable claim system for disjoint work lanes."
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $outputResolvedPath -Encoding UTF8

Write-Host "freework_cadence_recommendation_written=$outputResolvedPath"
Write-Host "budget_rules_path=$budgetRulesResolvedPath"
Write-Host "freework_automation_id=$freeworkAutomationId"
Write-Host "freework_automation_path=$freeworkAutomationResolvedPath"
Write-Host "recommended_freework_rrule=$recommendedRRule"
Write-Host "current_freework_rrule=$currentRRule"
Write-Host "should_update_freework_automation=$shouldUpdateAutomation"
