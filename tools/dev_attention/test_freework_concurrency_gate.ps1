param(
    [string]$RepoRoot = "",
    [string]$AutomationId = "sn-bounded-free-work-execution-2",
    [string]$RunId = "",
    [int]$ForegroundLockMaxAgeMinutes = 120,
    [int]$ActiveRunMaxAgeMinutes = 45,
    [string]$RunLogPath = ".codex_local/automation_run_log.csv",
    [string]$OutputPath = "dist/private_reports/freework_concurrency_gate.json"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
} else {
    $RepoRoot = Resolve-Path $RepoRoot
}

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $RepoRoot $Path
}

function Ensure-ParentDirectory {
    param([string]$Path)

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function Get-SafeAutomationId {
    param([string]$Value)

    $safe = ($Value -replace '[^A-Za-z0-9._-]', '-').Trim('-')
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "unknown"
    }

    return $safe
}

function Read-TextFile {
    param([string]$Path)

    try {
        if (-not (Test-Path -LiteralPath $Path)) {
            return ""
        }

        return (Get-Content -LiteralPath $Path -Raw -ErrorAction Stop).Trim()
    } catch {
        return ""
    }
}

function Get-ObjectProperty {
    param([object]$Object, [string]$Name)

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function ConvertTo-DateTimeOffsetOrNull {
    param([object]$Value)

    if ($null -eq $Value) {
        return $null
    }

    $text = ([string]$Value).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $null
    }

    $parsed = [DateTimeOffset]::MinValue
    if ([DateTimeOffset]::TryParse($text, [ref]$parsed)) {
        return $parsed
    }

    return $null
}

function Get-FileAgeMinutes {
    param([string]$Path)

    try {
        $item = Get-Item -LiteralPath $Path -ErrorAction Stop
        return ([DateTimeOffset]::Now - [DateTimeOffset]$item.LastWriteTime).TotalMinutes
    } catch {
        return $null
    }
}

function Get-LatestFinalRunResult {
    param([string]$AutomationIdValue, [string]$RunIdValue)

    if ([string]::IsNullOrWhiteSpace($RunIdValue)) {
        return ""
    }

    $resolvedLogPath = Resolve-ProjectPath -Path $RunLogPath
    if (-not (Test-Path -LiteralPath $resolvedLogPath)) {
        return ""
    }

    try {
        $header = "timestamp_local,timezone,automation_id,run_id,trigger,result,summary,details"
        $recentLines = Get-Content -LiteralPath $resolvedLogPath -Tail 500 -ErrorAction Stop |
            Where-Object { $_ -and -not $_.StartsWith("timestamp_local") }
        if (-not $recentLines -or $recentLines.Count -eq 0) {
            return ""
        }

        $records = ($header + "`n" + ($recentLines -join "`n")) | ConvertFrom-Csv
        $match = $records |
            Where-Object {
                $_.automation_id -eq $AutomationIdValue -and
                $_.run_id -eq $RunIdValue -and
                $_.result -in @("implemented", "blocked", "quiet", "no_safe_task", "failed")
            } |
            Select-Object -Last 1
        if ($null -eq $match) {
            return ""
        }

        return [string]$match.result
    } catch {
        return ""
    }
}

function New-MarkerInfo {
    param(
        [string]$Role,
        [string]$Path,
        [bool]$Blocks
    )

    $resolved = Resolve-ProjectPath -Path $Path
    $exists = Test-Path -LiteralPath $resolved
    $value = if ($exists) { Read-TextFile -Path $resolved } else { "" }
    $ageMinutes = if ($exists) { Get-FileAgeMinutes -Path $resolved } else { $null }
    $sameRun = (-not [string]::IsNullOrWhiteSpace($RunId)) -and ($value -eq $RunId)
    $finalResult = if ($exists) { Get-LatestFinalRunResult -AutomationIdValue $AutomationId -RunIdValue $value } else { "" }
    $hasFinalResult = -not [string]::IsNullOrWhiteSpace($finalResult)
    $stale = ($null -ne $ageMinutes) -and ($ageMinutes -gt $ActiveRunMaxAgeMinutes)
    $active = $exists -and (-not [string]::IsNullOrWhiteSpace($value)) -and (-not $sameRun) -and (-not $stale) -and (-not $hasFinalResult)

    return [pscustomobject]@{
        role = $Role
        path = $Path
        exists = $exists
        value = $value
        age_minutes = if ($null -eq $ageMinutes) { $null } else { [Math]::Round($ageMinutes, 2) }
        same_run = $sameRun
        stale = $stale
        final_result = $finalResult
        final_result_seen = $hasFinalResult
        blocks = ($Blocks -and $active)
    }
}

function New-ForegroundLockInfo {
    param(
        [string]$Chat,
        [string]$Path
    )

    $resolved = Resolve-ProjectPath -Path $Path
    $exists = Test-Path -LiteralPath $resolved
    $content = if ($exists) { Read-TextFile -Path $resolved } else { "" }
    $json = $null
    $parseError = ""

    if (-not [string]::IsNullOrWhiteSpace($content)) {
        try {
            $json = $content | ConvertFrom-Json -ErrorAction Stop
        } catch {
            $parseError = $_.Exception.Message
        }
    }

    $activeProperty = Get-ObjectProperty -Object $json -Name "active"
    $active = $exists
    if ($null -ne $activeProperty) {
        $active = [bool]$activeProperty
    }

    $reason = Get-ObjectProperty -Object $json -Name "reason"
    $run = Get-ObjectProperty -Object $json -Name "run_id"
    $declaredChat = Get-ObjectProperty -Object $json -Name "chat"
    if (-not [string]::IsNullOrWhiteSpace([string]$declaredChat)) {
        $Chat = [string]$declaredChat
    }

    $updatedAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "updated_at")
    if ($null -eq $updatedAt) {
        $updatedAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "updated_at_utc")
    }
    if ($null -eq $updatedAt) {
        $updatedAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "started_at")
    }
    if ($null -eq $updatedAt) {
        $updatedAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "started_at_utc")
    }

    $fileAgeMinutes = if ($exists) { Get-FileAgeMinutes -Path $resolved } else { $null }
    if (($null -eq $updatedAt) -and ($exists)) {
        $updatedAt = [DateTimeOffset](Get-Item -LiteralPath $resolved).LastWriteTime
    }

    $ageMinutes = if ($null -eq $updatedAt) { $fileAgeMinutes } else { ([DateTimeOffset]::Now - $updatedAt).TotalMinutes }

    $expiresAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "expires_at")
    if ($null -eq $expiresAt) {
        $expiresAt = ConvertTo-DateTimeOffsetOrNull (Get-ObjectProperty -Object $json -Name "expires_at_utc")
    }

    $expiredByTimestamp = ($null -ne $expiresAt) -and ([DateTimeOffset]::Now -gt $expiresAt)
    $expiredByAge = ($null -ne $ageMinutes) -and ($ageMinutes -gt $ForegroundLockMaxAgeMinutes)
    $stale = $expiredByTimestamp -or $expiredByAge
    $blocks = $exists -and $active -and (-not $stale)

    return [pscustomobject]@{
        chat = $Chat
        path = $Path
        exists = $exists
        active = $active
        blocks = $blocks
        stale = $stale
        age_minutes = if ($null -eq $ageMinutes) { $null } else { [Math]::Round($ageMinutes, 2) }
        expires_at = if ($null -eq $expiresAt) { $null } else { $expiresAt.ToString("o") }
        reason = if ($null -eq $reason) { "" } else { [string]$reason }
        run_id = if ($null -eq $run) { "" } else { [string]$run }
        parse_error = $parseError
    }
}

$safeAutomationId = Get-SafeAutomationId -Value $AutomationId
$markerSpecs = @(
    @{ role = "freework_active"; path = ".codex_local/freework_active_run_id.txt"; blocks = $true },
    @{ role = "current_run_this_automation"; path = ".codex_local/current_run_id_$safeAutomationId.txt"; blocks = $true },
    @{ role = "current_run_primary"; path = ".codex_local/current_run_id_sn-bounded-free-work-execution-2.txt"; blocks = $true },
    @{ role = "current_run_legacy"; path = ".codex_local/current_run_id_sn-bounded-free-work-execution.txt"; blocks = $true },
    @{ role = "current_automation_observation"; path = ".codex_local/current_automation_run_id.txt"; blocks = $false }
)

$seenMarkerPaths = @{}
$markers = @()
foreach ($spec in $markerSpecs) {
    if ($seenMarkerPaths.ContainsKey($spec.path)) {
        continue
    }
    $seenMarkerPaths[$spec.path] = $true
    $markers += New-MarkerInfo -Role $spec.role -Path $spec.path -Blocks ([bool]$spec.blocks)
}

$lockSpecs = @(
    @{ chat = "desktop"; path = ".codex_local/foreground_desktop_active.json" },
    @{ chat = "mobile"; path = ".codex_local/foreground_mobile_active.json" },
    @{ chat = "foreground"; path = ".codex_local/foreground_work_active.json" },
    @{ chat = "desktop"; path = ".codex_local/main_chat_desktop_active.json" },
    @{ chat = "mobile"; path = ".codex_local/main_chat_mobile_active.json" }
)

$foregroundLocks = @()
foreach ($spec in $lockSpecs) {
    $foregroundLocks += New-ForegroundLockInfo -Chat $spec.chat -Path $spec.path
}

$blockingMarker = $markers | Where-Object { $_.blocks } | Select-Object -First 1
$blockingLock = $foregroundLocks | Where-Object { $_.blocks } | Select-Object -First 1

$shouldSkip = $false
$skipKind = "none"
$skipReason = ""
$blockingPath = ""

if ($null -ne $blockingMarker) {
    $shouldSkip = $true
    $skipKind = "other_freework"
    $skipReason = "another Free Work run appears active via $($blockingMarker.role)"
    $blockingPath = $blockingMarker.path
} elseif ($null -ne $blockingLock) {
    $shouldSkip = $true
    $skipKind = "foreground_main_chat"
    $skipReason = "foreground $($blockingLock.chat) chat lock is active"
    $blockingPath = $blockingLock.path
}

$result = [ordered]@{
    schema_version = 1
    checked_at = (Get-Date).ToString("o")
    automation_id = $AutomationId
    run_id = $RunId
    should_skip = $shouldSkip
    skip_kind = $skipKind
    skip_reason = $skipReason
    blocking_path = $blockingPath
    active_run_max_age_minutes = $ActiveRunMaxAgeMinutes
    foreground_lock_max_age_minutes = $ForegroundLockMaxAgeMinutes
    run_markers = $markers
    foreground_locks = $foregroundLocks
}

$resolvedOutputPath = Resolve-ProjectPath -Path $OutputPath
Ensure-ParentDirectory -Path $resolvedOutputPath
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutputPath -Encoding UTF8

Write-Host ("freework_should_skip=" + $shouldSkip.ToString().ToLowerInvariant())
Write-Host ("freework_skip_kind=" + $skipKind)
Write-Host ("freework_skip_reason=" + $skipReason)
Write-Host ("freework_gate_output=" + $resolvedOutputPath)
