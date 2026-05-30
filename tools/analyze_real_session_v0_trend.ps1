param(
    [string]$SessionsRoot,
    [string]$OutputJson
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SessionsRoot)) {
    $SessionsRoot = Join-Path $repoRoot "dist/real_session_v0_loop"
}
$sessionsRootFull = [System.IO.Path]::GetFullPath($SessionsRoot)
if (-not (Test-Path -LiteralPath $sessionsRootFull)) {
    throw "SessionsRoot does not exist: $sessionsRootFull"
}

function Read-JsonFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

function Get-OptionalString {
    param([Parameter(Mandatory = $true)]$Object, [Parameter(Mandatory = $true)][string]$Property)
    if ($null -eq $Object) { return "" }
    $value = $Object.PSObject.Properties[$Property]
    if ($null -eq $value) { return "" }
    if ($null -eq $value.Value) { return "" }
    return [string]$value.Value
}

$sessionDirs = Get-ChildItem -LiteralPath $sessionsRootFull -Directory | Sort-Object Name
$sessions = @()
foreach ($sessionDir in $sessionDirs) {
    $statusPath = Join-Path $sessionDir.FullName "snc_status_snapshot.json"
    $summaryPath = Join-Path $sessionDir.FullName "work/archive_summary.json"
    if (-not (Test-Path -LiteralPath $statusPath) -or -not (Test-Path -LiteralPath $summaryPath)) {
        continue
    }

    $status = Read-JsonFile -Path $statusPath
    $summary = Read-JsonFile -Path $summaryPath
    if ($null -eq $status -or $null -eq $summary) {
        continue
    }

    $overlayManifestHash = ""
    $gameplayState = ""
    if ($null -ne $status.generated_overlay_status) {
        $overlayManifestHash = Get-OptionalString -Object $status.generated_overlay_status -Property "manifest_hash"
    }
    if ($null -ne $status.gameplay_acceptance_status) {
        $gameplayState = Get-OptionalString -Object $status.gameplay_acceptance_status -Property "state"
    }

    $archiveSaveCount = 0
    if ($null -ne $summary.copied_save_count) {
        $archiveSaveCount = [int]$summary.copied_save_count
    }

    $sessions += [pscustomobject]@{
        session_id = $sessionDir.Name
        session_dir = $sessionDir.FullName
        overlay_manifest_hash = $overlayManifestHash
        archive_save_count = $archiveSaveCount
        gameplay_acceptance_state = $gameplayState
    }
}

$sessionCount = $sessions.Count
$manifestHashes = @($sessions | ForEach-Object { $_.overlay_manifest_hash } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$gameplayStates = @($sessions | ForEach-Object { $_.gameplay_acceptance_state } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$archiveSaveCounts = @($sessions | ForEach-Object { [int]$_.archive_save_count } | Sort-Object -Unique)

$latestSessionId = ""
$latestSessionDir = ""
$previousSessionId = ""
$latestOverlayManifestHash = ""
$previousOverlayManifestHash = ""
$latestArchiveSaveCount = ""
$previousArchiveSaveCount = ""
$latestGameplayState = ""
$previousGameplayState = ""
$latestDeltaOverlayChanged = ""
$latestDeltaArchiveSaveCountDelta = ""
$latestDeltaGameplayChanged = ""
$latestCompareCommandHint = ""
$latestTrendRecommendation = "need_more_real_sessions"
$latestIdentityRiskWarning = "false"
$latestIdentityRiskWarningReason = "need_more_real_sessions"
$latestIdentityRiskWarningCodes = @()
if ($sessionCount -ge 1) {
    $latest = $sessions[$sessionCount - 1]
    $latestSessionId = $latest.session_id
    $latestSessionDir = $latest.session_dir
    $latestOverlayManifestHash = $latest.overlay_manifest_hash
    $latestArchiveSaveCount = [string]$latest.archive_save_count
    $latestGameplayState = $latest.gameplay_acceptance_state
}
if ($sessionCount -ge 2) {
    $previous = $sessions[$sessionCount - 2]
    $previousSessionId = $previous.session_id
    $previousOverlayManifestHash = $previous.overlay_manifest_hash
    $previousArchiveSaveCount = [string]$previous.archive_save_count
    $previousGameplayState = $previous.gameplay_acceptance_state
    $latestDeltaOverlayChanged = if ($latestOverlayManifestHash -ne $previousOverlayManifestHash) { "true" } else { "false" }
    $latestDeltaArchiveSaveCountDelta = [string]($latest.archive_save_count - $previous.archive_save_count)
    $latestDeltaGameplayChanged = if ($latestGameplayState -ne $previousGameplayState) { "true" } else { "false" }
    $latestCompareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "' + $previous.session_dir + '" "' + $latest.session_dir + '" "dist\private_reports\real_session_v0_compare_' + $previous.session_id + '_to_' + $latest.session_id + '.json"'

    $compareScriptPath = Join-Path $PSScriptRoot "compare_real_session_v0_outputs.ps1"
    if (-not (Test-Path -LiteralPath $compareScriptPath)) {
        throw "Missing compare script: $compareScriptPath"
    }
    $compareOutputJson = Join-Path $repoRoot "dist/private_reports/real_session_v0_trend_latest_compare.json"
    $compareLines = & powershell -NoProfile -ExecutionPolicy Bypass -File $compareScriptPath $previous.session_dir $latest.session_dir $compareOutputJson
    if ($LASTEXITCODE -ne 0) {
        throw "compare_real_session_v0_outputs.ps1 failed during trend analysis (exit code $LASTEXITCODE)."
    }
    if ($null -eq $compareLines) {
        throw "Compare script returned no output during trend analysis."
    }

    $compareIdentityRiskWarningLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_identity_risk_warning=*" } | Select-Object -First 1
    $compareIdentityRiskWarningReasonLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_identity_risk_warning_reason=*" } | Select-Object -First 1
    $latestIdentityRiskWarningCodes = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_identity_risk_warning_code=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_identity_risk_warning_code=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    if (-not [string]::IsNullOrWhiteSpace($compareIdentityRiskWarningLine)) {
        $latestIdentityRiskWarning = $compareIdentityRiskWarningLine.Substring("real_session_v0_compare_identity_risk_warning=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareIdentityRiskWarningReasonLine)) {
        $latestIdentityRiskWarningReason = $compareIdentityRiskWarningReasonLine.Substring("real_session_v0_compare_identity_risk_warning_reason=".Length)
    }

    if (($latestDeltaOverlayChanged -eq "true") -or ([int]$latestDeltaArchiveSaveCountDelta -ne 0) -or ($latestDeltaGameplayChanged -eq "true")) {
        $latestTrendRecommendation = "review_observable_deltas"
    }
    elseif ($latestIdentityRiskWarning -eq "true") {
        $latestTrendRecommendation = "review_identity_risk_warning"
    }
    else {
        $latestTrendRecommendation = "no_pipeline_delta_detected"
    }
}

$result = [ordered]@{
    schema_version = 1
    sessions_root = $sessionsRootFull
    session_count = $sessionCount
    unique_overlay_manifest_hash_count = $manifestHashes.Count
    unique_gameplay_acceptance_state_count = $gameplayStates.Count
    unique_archive_save_count_values = $archiveSaveCounts
    latest_session = [ordered]@{
        session_id = $latestSessionId
        session_dir = $latestSessionDir
        overlay_manifest_hash = $latestOverlayManifestHash
        archive_save_count = $latestArchiveSaveCount
        gameplay_acceptance_state = $latestGameplayState
    }
    previous_session = [ordered]@{
        session_id = $previousSessionId
        overlay_manifest_hash = $previousOverlayManifestHash
        archive_save_count = $previousArchiveSaveCount
        gameplay_acceptance_state = $previousGameplayState
    }
    latest_delta = [ordered]@{
        overlay_manifest_hash_changed = $latestDeltaOverlayChanged
        archive_save_count_delta = $latestDeltaArchiveSaveCountDelta
        gameplay_acceptance_state_changed = $latestDeltaGameplayChanged
    }
    latest_identity_risk_warning = [ordered]@{
        active = $latestIdentityRiskWarning
        reason = $latestIdentityRiskWarningReason
        warning_codes = $latestIdentityRiskWarningCodes
    }
    next_step_recommendation = $latestTrendRecommendation
}

if ([string]::IsNullOrWhiteSpace($OutputJson)) {
    $OutputJson = Join-Path $repoRoot "dist/private_reports/real_session_v0_trend.json"
}
$outputJsonFull = [System.IO.Path]::GetFullPath($OutputJson)
New-Item -ItemType Directory -Path (Split-Path -Parent $outputJsonFull) -Force | Out-Null
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputJsonFull -Encoding UTF8

Write-Host "real_session_v0_trend_ok=true"
Write-Host ("real_session_v0_trend_output_json=" + $outputJsonFull)
Write-Host ("real_session_v0_trend_session_count=" + $sessionCount)
Write-Host ("real_session_v0_trend_unique_overlay_manifest_hash_count=" + $manifestHashes.Count)
Write-Host ("real_session_v0_trend_unique_gameplay_acceptance_state_count=" + $gameplayStates.Count)
Write-Host ("real_session_v0_trend_recommendation=" + $latestTrendRecommendation)
Write-Host ("real_session_v0_trend_identity_risk_warning=" + $latestIdentityRiskWarning)
Write-Host ("real_session_v0_trend_identity_risk_warning_reason=" + $latestIdentityRiskWarningReason)
foreach ($warningCode in $latestIdentityRiskWarningCodes) {
    Write-Host ("real_session_v0_trend_identity_risk_warning_code=" + $warningCode)
}
if (-not [string]::IsNullOrWhiteSpace($latestCompareCommandHint)) {
    Write-Host ("real_session_v0_trend_latest_compare_command_hint=" + $latestCompareCommandHint)
}
exit 0
