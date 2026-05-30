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
$nextSessionCommandHint = ""
$latestTrendRecommendation = "need_more_real_sessions"
$latestIdentityRiskWarning = "false"
$latestIdentityRiskWarningReason = "need_more_real_sessions"
$latestIdentityRiskWarningCodes = @()
$latestObservableEffectSignal = "false"
$latestObservableEffectReason = "need_more_real_sessions"
$latestGameplayAcceptanceStateCurrent = ""
$latestGameplayAcceptanceStatePrevious = ""
$latestGameplayAcceptanceStateChanged = ""
$latestGameplayAcceptanceReasonCurrent = ""
$latestGameplayAcceptanceReasonPrevious = ""
$latestGameplayAcceptanceReasonChanged = ""
$latestMpWarningCountCurrent = ""
$latestMpWarningCountDelta = ""
$latestMpWarningCodesChanged = ""
$latestMpManifestHashCurrent = ""
$latestMpManifestHashPrevious = ""
$latestMpManifestHashChanged = ""
$latestMpPackageOutputDirCurrent = ""
$latestMpPackageOutputDirPrevious = ""
$latestMpPackageOutputDirChanged = ""
$latestMpWarningCodesPrevious = @()
$latestMpWarningCodesCurrent = @()
$latestMpHostReadinessCurrent = ""
$latestMpClientReadinessGateCurrent = ""
$latestMpHostNextStepCurrent = ""
$latestMpClientNextStepCurrent = ""
$latestMpVerifyCommandCurrent = ""
$latestMpImportCommandCurrent = ""
$latestMpStrictVerifyCommandCurrent = ""
$latestMpStrictImportCommandCurrent = ""
$latestMpIdentityMismatchWarningCurrent = ""
$latestMpIdentityMismatchWarningPrevious = ""
$latestMpIdentityMismatchWarningChanged = ""
$latestMpIdentityMismatchWarningCodesChanged = ""
$latestMpIdentityMismatchWarningCodesPrevious = @()
$latestMpIdentityMismatchWarningCodesCurrent = @()
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
    $compareObservableEffectSignalLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_observable_effect_signal=*" } | Select-Object -First 1
    $compareObservableEffectReasonLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_observable_effect_reason=*" } | Select-Object -First 1
    $compareGameplayAcceptanceStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_state_current=*" } | Select-Object -First 1
    $compareGameplayAcceptanceStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_state_previous=*" } | Select-Object -First 1
    $compareGameplayAcceptanceStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_state_changed=*" } | Select-Object -First 1
    $compareGameplayAcceptanceReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_reason_current=*" } | Select-Object -First 1
    $compareGameplayAcceptanceReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_reason_previous=*" } | Select-Object -First 1
    $compareGameplayAcceptanceReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_gameplay_acceptance_reason_changed=*" } | Select-Object -First 1
    $compareMpWarningCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_count_current=*" } | Select-Object -First 1
    $compareMpWarningCountDeltaLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_count_delta=*" } | Select-Object -First 1
    $compareMpWarningCodesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_codes_changed=*" } | Select-Object -First 1
    $compareMpManifestHashCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_current=*" } | Select-Object -First 1
    $compareMpManifestHashPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_previous=*" } | Select-Object -First 1
    $compareMpManifestHashChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_changed=*" } | Select-Object -First 1
    $compareMpPackageOutputDirCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_current=*" } | Select-Object -First 1
    $compareMpPackageOutputDirPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_previous=*" } | Select-Object -First 1
    $compareMpPackageOutputDirChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_changed=*" } | Select-Object -First 1
    $latestMpWarningCodesCurrent = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_warning_code_current=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_warning_code_current=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    $latestMpWarningCodesPrevious = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_warning_code_previous=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_warning_code_previous=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    $compareMpHostReadinessCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_readiness_current=*" } | Select-Object -First 1
    $compareMpHostReadinessPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_readiness_previous=*" } | Select-Object -First 1
    $compareMpHostReadinessChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_readiness_changed=*" } | Select-Object -First 1
    $compareMpClientReadinessGateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_readiness_gate_current=*" } | Select-Object -First 1
    $compareMpClientReadinessGatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_readiness_gate_previous=*" } | Select-Object -First 1
    $compareMpClientReadinessGateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_readiness_gate_changed=*" } | Select-Object -First 1
    $compareMpHostNextStepCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_next_step_current=*" } | Select-Object -First 1
    $compareMpHostNextStepPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_next_step_previous=*" } | Select-Object -First 1
    $compareMpHostNextStepChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_host_next_step_changed=*" } | Select-Object -First 1
    $compareMpClientNextStepCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_next_step_current=*" } | Select-Object -First 1
    $compareMpClientNextStepPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_next_step_previous=*" } | Select-Object -First 1
    $compareMpClientNextStepChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_client_next_step_changed=*" } | Select-Object -First 1
    $compareMpVerifyCommandCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_verify_command_current=*" } | Select-Object -First 1
    $compareMpVerifyCommandPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_verify_command_previous=*" } | Select-Object -First 1
    $compareMpVerifyCommandChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_verify_command_changed=*" } | Select-Object -First 1
    $compareMpImportCommandCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_import_command_current=*" } | Select-Object -First 1
    $compareMpImportCommandPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_import_command_previous=*" } | Select-Object -First 1
    $compareMpImportCommandChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_import_command_changed=*" } | Select-Object -First 1
    $compareMpStrictVerifyCommandCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_verify_command_current=*" } | Select-Object -First 1
    $compareMpStrictVerifyCommandPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_verify_command_previous=*" } | Select-Object -First 1
    $compareMpStrictVerifyCommandChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_verify_command_changed=*" } | Select-Object -First 1
    $compareMpStrictImportCommandCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_import_command_current=*" } | Select-Object -First 1
    $compareMpStrictImportCommandPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_import_command_previous=*" } | Select-Object -First 1
    $compareMpStrictImportCommandChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_strict_import_command_changed=*" } | Select-Object -First 1
    $compareMpIdentityMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpIdentityMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpIdentityMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpIdentityMismatchWarningCodesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_codes_changed=*" } | Select-Object -First 1
    $latestMpIdentityMismatchWarningCodesPrevious = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_code_previous=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_identity_mismatch_warning_code_previous=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    $latestMpIdentityMismatchWarningCodesCurrent = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_identity_mismatch_warning_code_current=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_identity_mismatch_warning_code_current=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
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
    if (-not [string]::IsNullOrWhiteSpace($compareObservableEffectSignalLine)) {
        $latestObservableEffectSignal = $compareObservableEffectSignalLine.Substring("real_session_v0_compare_observable_effect_signal=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareObservableEffectReasonLine)) {
        $latestObservableEffectReason = $compareObservableEffectReasonLine.Substring("real_session_v0_compare_observable_effect_reason=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceStateCurrentLine)) {
        $latestGameplayAcceptanceStateCurrent = $compareGameplayAcceptanceStateCurrentLine.Substring("real_session_v0_compare_gameplay_acceptance_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceStatePreviousLine)) {
        $latestGameplayAcceptanceStatePrevious = $compareGameplayAcceptanceStatePreviousLine.Substring("real_session_v0_compare_gameplay_acceptance_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceStateChangedLine)) {
        $latestGameplayAcceptanceStateChanged = $compareGameplayAcceptanceStateChangedLine.Substring("real_session_v0_compare_gameplay_acceptance_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceReasonCurrentLine)) {
        $latestGameplayAcceptanceReasonCurrent = $compareGameplayAcceptanceReasonCurrentLine.Substring("real_session_v0_compare_gameplay_acceptance_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceReasonPreviousLine)) {
        $latestGameplayAcceptanceReasonPrevious = $compareGameplayAcceptanceReasonPreviousLine.Substring("real_session_v0_compare_gameplay_acceptance_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGameplayAcceptanceReasonChangedLine)) {
        $latestGameplayAcceptanceReasonChanged = $compareGameplayAcceptanceReasonChangedLine.Substring("real_session_v0_compare_gameplay_acceptance_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCountCurrentLine)) {
        $latestMpWarningCountCurrent = $compareMpWarningCountCurrentLine.Substring("real_session_v0_compare_mp_warning_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCountDeltaLine)) {
        $latestMpWarningCountDelta = $compareMpWarningCountDeltaLine.Substring("real_session_v0_compare_mp_warning_count_delta=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCodesChangedLine)) {
        $latestMpWarningCodesChanged = $compareMpWarningCodesChangedLine.Substring("real_session_v0_compare_mp_warning_codes_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashCurrentLine)) {
        $latestMpManifestHashCurrent = $compareMpManifestHashCurrentLine.Substring("real_session_v0_compare_mp_manifest_hash_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashPreviousLine)) {
        $latestMpManifestHashPrevious = $compareMpManifestHashPreviousLine.Substring("real_session_v0_compare_mp_manifest_hash_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashChangedLine)) {
        $latestMpManifestHashChanged = $compareMpManifestHashChangedLine.Substring("real_session_v0_compare_mp_manifest_hash_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirCurrentLine)) {
        $latestMpPackageOutputDirCurrent = $compareMpPackageOutputDirCurrentLine.Substring("real_session_v0_compare_mp_package_output_dir_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirPreviousLine)) {
        $latestMpPackageOutputDirPrevious = $compareMpPackageOutputDirPreviousLine.Substring("real_session_v0_compare_mp_package_output_dir_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirChangedLine)) {
        $latestMpPackageOutputDirChanged = $compareMpPackageOutputDirChangedLine.Substring("real_session_v0_compare_mp_package_output_dir_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessCurrentLine)) {
        $latestMpHostReadinessCurrent = $compareMpHostReadinessCurrentLine.Substring("real_session_v0_compare_mp_host_readiness_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessPreviousLine)) {
        $latestMpHostReadinessPrevious = $compareMpHostReadinessPreviousLine.Substring("real_session_v0_compare_mp_host_readiness_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessChangedLine)) {
        $latestMpHostReadinessChanged = $compareMpHostReadinessChangedLine.Substring("real_session_v0_compare_mp_host_readiness_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGateCurrentLine)) {
        $latestMpClientReadinessGateCurrent = $compareMpClientReadinessGateCurrentLine.Substring("real_session_v0_compare_mp_client_readiness_gate_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGatePreviousLine)) {
        $latestMpClientReadinessGatePrevious = $compareMpClientReadinessGatePreviousLine.Substring("real_session_v0_compare_mp_client_readiness_gate_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGateChangedLine)) {
        $latestMpClientReadinessGateChanged = $compareMpClientReadinessGateChangedLine.Substring("real_session_v0_compare_mp_client_readiness_gate_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepCurrentLine)) {
        $latestMpHostNextStepCurrent = $compareMpHostNextStepCurrentLine.Substring("real_session_v0_compare_mp_host_next_step_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepPreviousLine)) {
        $latestMpHostNextStepPrevious = $compareMpHostNextStepPreviousLine.Substring("real_session_v0_compare_mp_host_next_step_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepChangedLine)) {
        $latestMpHostNextStepChanged = $compareMpHostNextStepChangedLine.Substring("real_session_v0_compare_mp_host_next_step_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepCurrentLine)) {
        $latestMpClientNextStepCurrent = $compareMpClientNextStepCurrentLine.Substring("real_session_v0_compare_mp_client_next_step_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepPreviousLine)) {
        $latestMpClientNextStepPrevious = $compareMpClientNextStepPreviousLine.Substring("real_session_v0_compare_mp_client_next_step_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepChangedLine)) {
        $latestMpClientNextStepChanged = $compareMpClientNextStepChangedLine.Substring("real_session_v0_compare_mp_client_next_step_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandCurrentLine)) {
        $latestMpVerifyCommandCurrent = $compareMpVerifyCommandCurrentLine.Substring("real_session_v0_compare_mp_verify_command_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandPreviousLine)) {
        $latestMpVerifyCommandPrevious = $compareMpVerifyCommandPreviousLine.Substring("real_session_v0_compare_mp_verify_command_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandChangedLine)) {
        $latestMpVerifyCommandChanged = $compareMpVerifyCommandChangedLine.Substring("real_session_v0_compare_mp_verify_command_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandCurrentLine)) {
        $latestMpImportCommandCurrent = $compareMpImportCommandCurrentLine.Substring("real_session_v0_compare_mp_import_command_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandPreviousLine)) {
        $latestMpImportCommandPrevious = $compareMpImportCommandPreviousLine.Substring("real_session_v0_compare_mp_import_command_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandChangedLine)) {
        $latestMpImportCommandChanged = $compareMpImportCommandChangedLine.Substring("real_session_v0_compare_mp_import_command_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandCurrentLine)) {
        $latestMpStrictVerifyCommandCurrent = $compareMpStrictVerifyCommandCurrentLine.Substring("real_session_v0_compare_mp_strict_verify_command_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandPreviousLine)) {
        $latestMpStrictVerifyCommandPrevious = $compareMpStrictVerifyCommandPreviousLine.Substring("real_session_v0_compare_mp_strict_verify_command_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandChangedLine)) {
        $latestMpStrictVerifyCommandChanged = $compareMpStrictVerifyCommandChangedLine.Substring("real_session_v0_compare_mp_strict_verify_command_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandCurrentLine)) {
        $latestMpStrictImportCommandCurrent = $compareMpStrictImportCommandCurrentLine.Substring("real_session_v0_compare_mp_strict_import_command_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandPreviousLine)) {
        $latestMpStrictImportCommandPrevious = $compareMpStrictImportCommandPreviousLine.Substring("real_session_v0_compare_mp_strict_import_command_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandChangedLine)) {
        $latestMpStrictImportCommandChanged = $compareMpStrictImportCommandChangedLine.Substring("real_session_v0_compare_mp_strict_import_command_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningCurrentLine)) {
        $latestMpIdentityMismatchWarningCurrent = $compareMpIdentityMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_identity_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningPreviousLine)) {
        $latestMpIdentityMismatchWarningPrevious = $compareMpIdentityMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_identity_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningChangedLine)) {
        $latestMpIdentityMismatchWarningChanged = $compareMpIdentityMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_identity_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningCodesChangedLine)) {
        $latestMpIdentityMismatchWarningCodesChanged = $compareMpIdentityMismatchWarningCodesChangedLine.Substring("real_session_v0_compare_mp_identity_mismatch_warning_codes_changed=".Length)
    }

    if ($latestIdentityRiskWarning -eq "true") {
        $latestTrendRecommendation = "review_identity_risk_warning"
    }
    elseif (($latestDeltaOverlayChanged -eq "true") -or ([int]$latestDeltaArchiveSaveCountDelta -ne 0) -or ($latestDeltaGameplayChanged -eq "true")) {
        $latestTrendRecommendation = "review_observable_deltas"
    }
    else {
        $latestTrendRecommendation = "no_pipeline_delta_detected"
    }
}
if ($sessionCount -ge 1) {
    $nextSessionCommandHint = 'cmd /c tools\run_real_session_v0_loop.cmd "<save_root>" "<archive_root>" "<campaign_id>" "<empire_id>" "<dsl_input>" -PreviousSessionDirForCompare "' + $latest.session_dir + '"'
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
    latest_observable_effect_signal = [ordered]@{
        active = $latestObservableEffectSignal
        reason = $latestObservableEffectReason
    }
    latest_gameplay_acceptance_state = [ordered]@{
        current = $latestGameplayAcceptanceStateCurrent
        previous = $latestGameplayAcceptanceStatePrevious
        changed = $latestGameplayAcceptanceStateChanged
        reason_current = $latestGameplayAcceptanceReasonCurrent
        reason_previous = $latestGameplayAcceptanceReasonPrevious
        reason_changed = $latestGameplayAcceptanceReasonChanged
    }
    latest_mp_warning_count = [ordered]@{
        current = $latestMpWarningCountCurrent
        delta = $latestMpWarningCountDelta
        warning_codes_previous = $latestMpWarningCodesPrevious
        warning_codes_current = $latestMpWarningCodesCurrent
        warning_codes_changed = $latestMpWarningCodesChanged
    }
    latest_mp_manifest = [ordered]@{
        hash_previous = $latestMpManifestHashPrevious
        hash_current = $latestMpManifestHashCurrent
        hash_changed = $latestMpManifestHashChanged
        package_output_dir_previous = $latestMpPackageOutputDirPrevious
        package_output_dir_current = $latestMpPackageOutputDirCurrent
        package_output_dir_changed = $latestMpPackageOutputDirChanged
    }
    latest_mp_readiness = [ordered]@{
        host_readiness_previous = $latestMpHostReadinessPrevious
        host_readiness_current = $latestMpHostReadinessCurrent
        host_readiness_changed = $latestMpHostReadinessChanged
        client_readiness_gate_previous = $latestMpClientReadinessGatePrevious
        client_readiness_gate_current = $latestMpClientReadinessGateCurrent
        client_readiness_gate_changed = $latestMpClientReadinessGateChanged
        host_next_step_previous = $latestMpHostNextStepPrevious
        host_next_step_current = $latestMpHostNextStepCurrent
        host_next_step_changed = $latestMpHostNextStepChanged
        client_next_step_previous = $latestMpClientNextStepPrevious
        client_next_step_current = $latestMpClientNextStepCurrent
        client_next_step_changed = $latestMpClientNextStepChanged
        verify_command_previous = $latestMpVerifyCommandPrevious
        verify_command_current = $latestMpVerifyCommandCurrent
        verify_command_changed = $latestMpVerifyCommandChanged
        import_command_previous = $latestMpImportCommandPrevious
        import_command_current = $latestMpImportCommandCurrent
        import_command_changed = $latestMpImportCommandChanged
        strict_verify_command_previous = $latestMpStrictVerifyCommandPrevious
        strict_verify_command_current = $latestMpStrictVerifyCommandCurrent
        strict_verify_command_changed = $latestMpStrictVerifyCommandChanged
        strict_import_command_previous = $latestMpStrictImportCommandPrevious
        strict_import_command_current = $latestMpStrictImportCommandCurrent
        strict_import_command_changed = $latestMpStrictImportCommandChanged
        identity_mismatch_warning_previous = $latestMpIdentityMismatchWarningPrevious
        identity_mismatch_warning_current = $latestMpIdentityMismatchWarningCurrent
        identity_mismatch_warning_changed = $latestMpIdentityMismatchWarningChanged
        identity_mismatch_warning_codes_previous = $latestMpIdentityMismatchWarningCodesPrevious
        identity_mismatch_warning_codes_current = $latestMpIdentityMismatchWarningCodesCurrent
        identity_mismatch_warning_codes_changed = $latestMpIdentityMismatchWarningCodesChanged
    }
    next_session_command_hint = $nextSessionCommandHint
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
Write-Host ("real_session_v0_trend_observable_effect_signal=" + $latestObservableEffectSignal)
Write-Host ("real_session_v0_trend_observable_effect_reason=" + $latestObservableEffectReason)
Write-Host ("real_session_v0_trend_gameplay_acceptance_state_current=" + $latestGameplayAcceptanceStateCurrent)
Write-Host ("real_session_v0_trend_gameplay_acceptance_state_previous=" + $latestGameplayAcceptanceStatePrevious)
Write-Host ("real_session_v0_trend_gameplay_acceptance_state_changed=" + $latestGameplayAcceptanceStateChanged)
Write-Host ("real_session_v0_trend_gameplay_acceptance_reason_current=" + $latestGameplayAcceptanceReasonCurrent)
Write-Host ("real_session_v0_trend_gameplay_acceptance_reason_previous=" + $latestGameplayAcceptanceReasonPrevious)
Write-Host ("real_session_v0_trend_gameplay_acceptance_reason_changed=" + $latestGameplayAcceptanceReasonChanged)
if (-not [string]::IsNullOrWhiteSpace($latestMpWarningCountCurrent)) {
    Write-Host ("real_session_v0_trend_mp_warning_count_current=" + $latestMpWarningCountCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpWarningCountDelta)) {
    Write-Host ("real_session_v0_trend_mp_warning_count_delta=" + $latestMpWarningCountDelta)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashCurrent)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_current=" + $latestMpManifestHashCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashPrevious)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_previous=" + $latestMpManifestHashPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashChanged)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_changed=" + $latestMpManifestHashChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageOutputDirCurrent)) {
    Write-Host ("real_session_v0_trend_mp_package_output_dir_current=" + $latestMpPackageOutputDirCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageOutputDirPrevious)) {
    Write-Host ("real_session_v0_trend_mp_package_output_dir_previous=" + $latestMpPackageOutputDirPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageOutputDirChanged)) {
    Write-Host ("real_session_v0_trend_mp_package_output_dir_changed=" + $latestMpPackageOutputDirChanged)
}
foreach ($warningCode in $latestMpWarningCodesCurrent) {
    Write-Host ("real_session_v0_trend_mp_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $latestMpWarningCodesPrevious) {
    Write-Host ("real_session_v0_trend_mp_warning_code_previous=" + $warningCode)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpWarningCodesChanged)) {
    Write-Host ("real_session_v0_trend_mp_warning_codes_changed=" + $latestMpWarningCodesChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostReadinessCurrent)) {
    Write-Host ("real_session_v0_trend_mp_host_readiness_current=" + $latestMpHostReadinessCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostReadinessPrevious)) {
    Write-Host ("real_session_v0_trend_mp_host_readiness_previous=" + $latestMpHostReadinessPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostReadinessChanged)) {
    Write-Host ("real_session_v0_trend_mp_host_readiness_changed=" + $latestMpHostReadinessChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientReadinessGateCurrent)) {
    Write-Host ("real_session_v0_trend_mp_client_readiness_gate_current=" + $latestMpClientReadinessGateCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientReadinessGatePrevious)) {
    Write-Host ("real_session_v0_trend_mp_client_readiness_gate_previous=" + $latestMpClientReadinessGatePrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientReadinessGateChanged)) {
    Write-Host ("real_session_v0_trend_mp_client_readiness_gate_changed=" + $latestMpClientReadinessGateChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostNextStepCurrent)) {
    Write-Host ("real_session_v0_trend_mp_host_next_step_current=" + $latestMpHostNextStepCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostNextStepPrevious)) {
    Write-Host ("real_session_v0_trend_mp_host_next_step_previous=" + $latestMpHostNextStepPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpHostNextStepChanged)) {
    Write-Host ("real_session_v0_trend_mp_host_next_step_changed=" + $latestMpHostNextStepChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientNextStepCurrent)) {
    Write-Host ("real_session_v0_trend_mp_client_next_step_current=" + $latestMpClientNextStepCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientNextStepPrevious)) {
    Write-Host ("real_session_v0_trend_mp_client_next_step_previous=" + $latestMpClientNextStepPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpClientNextStepChanged)) {
    Write-Host ("real_session_v0_trend_mp_client_next_step_changed=" + $latestMpClientNextStepChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpVerifyCommandCurrent)) {
    Write-Host ("real_session_v0_trend_mp_verify_command_current=" + $latestMpVerifyCommandCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpVerifyCommandPrevious)) {
    Write-Host ("real_session_v0_trend_mp_verify_command_previous=" + $latestMpVerifyCommandPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpVerifyCommandChanged)) {
    Write-Host ("real_session_v0_trend_mp_verify_command_changed=" + $latestMpVerifyCommandChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpImportCommandCurrent)) {
    Write-Host ("real_session_v0_trend_mp_import_command_current=" + $latestMpImportCommandCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpImportCommandPrevious)) {
    Write-Host ("real_session_v0_trend_mp_import_command_previous=" + $latestMpImportCommandPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpImportCommandChanged)) {
    Write-Host ("real_session_v0_trend_mp_import_command_changed=" + $latestMpImportCommandChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictVerifyCommandCurrent)) {
    Write-Host ("real_session_v0_trend_mp_strict_verify_command_current=" + $latestMpStrictVerifyCommandCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictVerifyCommandPrevious)) {
    Write-Host ("real_session_v0_trend_mp_strict_verify_command_previous=" + $latestMpStrictVerifyCommandPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictVerifyCommandChanged)) {
    Write-Host ("real_session_v0_trend_mp_strict_verify_command_changed=" + $latestMpStrictVerifyCommandChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictImportCommandCurrent)) {
    Write-Host ("real_session_v0_trend_mp_strict_import_command_current=" + $latestMpStrictImportCommandCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictImportCommandPrevious)) {
    Write-Host ("real_session_v0_trend_mp_strict_import_command_previous=" + $latestMpStrictImportCommandPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpStrictImportCommandChanged)) {
    Write-Host ("real_session_v0_trend_mp_strict_import_command_changed=" + $latestMpStrictImportCommandChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpIdentityMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_current=" + $latestMpIdentityMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpIdentityMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_previous=" + $latestMpIdentityMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpIdentityMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_changed=" + $latestMpIdentityMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpIdentityMismatchWarningCodesChanged)) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_codes_changed=" + $latestMpIdentityMismatchWarningCodesChanged)
}
foreach ($warningCode in $latestMpIdentityMismatchWarningCodesPrevious) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $latestMpIdentityMismatchWarningCodesCurrent) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $latestIdentityRiskWarningCodes) {
    Write-Host ("real_session_v0_trend_identity_risk_warning_code=" + $warningCode)
}
if (-not [string]::IsNullOrWhiteSpace($latestCompareCommandHint)) {
    Write-Host ("real_session_v0_trend_latest_compare_command_hint=" + $latestCompareCommandHint)
}
if (-not [string]::IsNullOrWhiteSpace($nextSessionCommandHint)) {
    Write-Host ("real_session_v0_trend_next_session_command_hint=" + $nextSessionCommandHint)
}
exit 0
