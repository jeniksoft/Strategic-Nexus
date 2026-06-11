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
$latestStatusCenterStateCurrent = ""
$latestStatusCenterStatePrevious = ""
$latestStatusCenterStateChanged = ""
$latestStatusCenterReasonCurrent = ""
$latestStatusCenterReasonPrevious = ""
$latestStatusCenterReasonChanged = ""
$latestStatusCenterSummaryTextCurrent = ""
$latestStatusCenterSummaryTextPrevious = ""
$latestStatusCenterSummaryTextChanged = ""
$latestGeneratedOverlayPublishGateStateCurrent = ""
$latestGeneratedOverlayPublishGateStatePrevious = ""
$latestGeneratedOverlayPublishGateStateChanged = ""
$latestGeneratedOverlayPublishGateReasonCurrent = ""
$latestGeneratedOverlayPublishGateReasonPrevious = ""
$latestGeneratedOverlayPublishGateReasonChanged = ""
$latestGeneratedOverlayPublishGateCanPublishCurrent = ""
$latestGeneratedOverlayPublishGateCanPublishPrevious = ""
$latestGeneratedOverlayPublishGateCanPublishChanged = ""
$latestGeneratedOverlayPublishGatePublishCommandCurrent = ""
$latestGeneratedOverlayPublishGatePublishCommandPrevious = ""
$latestGeneratedOverlayPublishGatePublishCommandChanged = ""
$latestNextActionCurrent = ""
$latestNextActionPrevious = ""
$latestNextActionChanged = ""
$latestNextActionReasonCurrent = ""
$latestNextActionReasonPrevious = ""
$latestNextActionReasonChanged = ""
$latestNextActionCommandHintCurrent = ""
$latestNextActionCommandHintPrevious = ""
$latestNextActionCommandHintChanged = ""
$latestNextActionCommandHintSourceCurrent = ""
$latestNextActionCommandHintSourcePrevious = ""
$latestNextActionCommandHintSourceChanged = ""
$latestNextActionPathCurrent = ""
$latestNextActionPathPrevious = ""
$latestNextActionPathChanged = ""
$latestEntryPointAnalysisPathCurrent = ""
$latestEntryPointAnalysisPathPrevious = ""
$latestEntryPointAnalysisPathChanged = ""
$latestEntryPointReadinessCurrent = ""
$latestEntryPointReadinessPrevious = ""
$latestEntryPointReadinessChanged = ""
$latestEntryPointReasonCurrent = ""
$latestEntryPointReasonPrevious = ""
$latestEntryPointReasonChanged = ""
$latestEntryPointCountCurrent = ""
$latestEntryPointCountPrevious = ""
$latestEntryPointCountChanged = ""
$latestEntryPointBranchAmbiguityCurrent = ""
$latestEntryPointBranchAmbiguityPrevious = ""
$latestEntryPointBranchAmbiguityChanged = ""
$latestMemoryRecoveryAnchorEntryPointIdCurrent = ""
$latestMemoryRecoveryAnchorEntryPointIdPrevious = ""
$latestMemoryRecoveryAnchorEntryPointIdChanged = ""
$latestCampaignLibraryPlanPresentCurrent = ""
$latestCampaignLibraryPlanPresentPrevious = ""
$latestCampaignLibraryPlanPresentChanged = ""
$latestCampaignLibraryPlanPathCurrent = ""
$latestCampaignLibraryPlanPathPrevious = ""
$latestCampaignLibraryPlanPathChanged = ""
$latestCampaignLibraryPlanSourceCurrent = ""
$latestCampaignLibraryPlanSourcePrevious = ""
$latestCampaignLibraryPlanSourceChanged = ""
$latestCampaignLibraryPlanReadinessCurrent = ""
$latestCampaignLibraryPlanReadinessPrevious = ""
$latestCampaignLibraryPlanReadinessChanged = ""
$latestCampaignLibraryPlanReasonCurrent = ""
$latestCampaignLibraryPlanReasonPrevious = ""
$latestCampaignLibraryPlanReasonChanged = ""
$latestCampaignLibraryLimitReachedCurrent = ""
$latestCampaignLibraryLimitReachedPrevious = ""
$latestCampaignLibraryLimitReachedChanged = ""
$latestCampaignLibrarySkippedDueToLimitCountCurrent = ""
$latestCampaignLibrarySkippedDueToLimitCountPrevious = ""
$latestCampaignLibrarySkippedDueToLimitCountChanged = ""
$latestCampaignLibraryFollowUpActive = "false"
$latestCampaignLibraryFollowUpReason = "none"
$latestMpWarningCountCurrent = ""
$latestMpWarningCountDelta = ""
$latestMpWarningCodesChanged = ""
$latestMpManifestHashCurrent = ""
$latestMpManifestHashPrevious = ""
$latestMpManifestHashChanged = ""
$latestMpPackageOutputDirCurrent = ""
$latestMpPackageOutputDirPrevious = ""
$latestMpPackageOutputDirChanged = ""
$latestMpPackageZipStateCurrent = ""
$latestMpPackageZipStatePrevious = ""
$latestMpPackageZipStateChanged = ""
$latestMpPackageZipReasonCurrent = ""
$latestMpPackageZipReasonPrevious = ""
$latestMpPackageZipReasonChanged = ""
$latestMpPackageZipSha256Current = ""
$latestMpPackageZipSha256Previous = ""
$latestMpPackageZipSha256Changed = ""
$latestMpPackageZipPathCurrent = ""
$latestMpPackageZipPathPrevious = ""
$latestMpPackageZipPathChanged = ""
$latestMpPackageZipBytesCurrent = ""
$latestMpPackageZipBytesPrevious = ""
$latestMpPackageZipBytesChanged = ""
$latestMpProvenanceStateCurrent = ""
$latestMpProvenanceStatePrevious = ""
$latestMpProvenanceStateChanged = ""
$latestMpSourceQualityCountCurrent = ""
$latestMpSourceQualityCountPrevious = ""
$latestMpSourceQualityCountChanged = ""
$latestMpSourceQualitiesCurrent = @()
$latestMpSourceQualitiesPrevious = @()
$latestMpSourceQualitiesChanged = ""
$latestMpBootstrapCampaignCountCurrent = ""
$latestMpBootstrapCampaignCountPrevious = ""
$latestMpBootstrapCampaignCountChanged = ""
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
$latestMpGameVersionMismatchWarningCurrent = ""
$latestMpGameVersionMismatchWarningPrevious = ""
$latestMpGameVersionMismatchWarningChanged = ""
$latestMpModVersionMismatchWarningCurrent = ""
$latestMpModVersionMismatchWarningPrevious = ""
$latestMpModVersionMismatchWarningChanged = ""
$latestMpManifestHashMismatchWarningCurrent = ""
$latestMpManifestHashMismatchWarningPrevious = ""
$latestMpManifestHashMismatchWarningChanged = ""
$latestMpMismatchWarningStateCurrent = ""
$latestMpMismatchWarningStatePrevious = ""
$latestMpMismatchWarningStateChanged = ""
$latestMpMismatchWarningReasonCurrent = ""
$latestMpMismatchWarningReasonPrevious = ""
$latestMpMismatchWarningReasonChanged = ""
$latestMpMismatchWarningCodesChanged = ""
$latestMpMismatchWarningCodesPrevious = @()
$latestMpMismatchWarningCodesCurrent = @()
$latestMpHandoffFollowUpActive = "false"
$latestMpHandoffFollowUpReason = "none"
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
    $compareStatusCenterStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_state_current=*" } | Select-Object -First 1
    $compareStatusCenterStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_state_previous=*" } | Select-Object -First 1
    $compareStatusCenterStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_state_changed=*" } | Select-Object -First 1
    $compareStatusCenterReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_reason_current=*" } | Select-Object -First 1
    $compareStatusCenterReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_reason_previous=*" } | Select-Object -First 1
    $compareStatusCenterReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_reason_changed=*" } | Select-Object -First 1
    $compareStatusCenterSummaryTextCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_summary_text_current=*" } | Select-Object -First 1
    $compareStatusCenterSummaryTextPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_summary_text_previous=*" } | Select-Object -First 1
    $compareStatusCenterSummaryTextChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_status_center_summary_text_changed=*" } | Select-Object -First 1
    $compareSaveRootResolutionCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_resolution_current=*" } | Select-Object -First 1
    $compareSaveRootResolutionPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_resolution_previous=*" } | Select-Object -First 1
    $compareSaveRootResolutionChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_resolution_changed=*" } | Select-Object -First 1
    $compareSaveRootSourceCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_source_current=*" } | Select-Object -First 1
    $compareSaveRootSourcePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_source_previous=*" } | Select-Object -First 1
    $compareSaveRootSourceChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_source_changed=*" } | Select-Object -First 1
    $compareSaveRootPathCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_path_current=*" } | Select-Object -First 1
    $compareSaveRootPathPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_path_previous=*" } | Select-Object -First 1
    $compareSaveRootPathChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_path_changed=*" } | Select-Object -First 1
    $compareSaveRootCampaignCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_campaign_count_current=*" } | Select-Object -First 1
    $compareSaveRootCampaignCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_campaign_count_previous=*" } | Select-Object -First 1
    $compareSaveRootCampaignCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_campaign_count_changed=*" } | Select-Object -First 1
    $compareSaveRootSaveFileCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_save_file_count_current=*" } | Select-Object -First 1
    $compareSaveRootSaveFileCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_save_file_count_previous=*" } | Select-Object -First 1
    $compareSaveRootSaveFileCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_save_file_count_changed=*" } | Select-Object -First 1
    $compareSaveRootAutosaveAnchorCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_autosave_anchor_count_current=*" } | Select-Object -First 1
    $compareSaveRootAutosaveAnchorCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_autosave_anchor_count_previous=*" } | Select-Object -First 1
    $compareSaveRootAutosaveAnchorCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_save_root_autosave_anchor_count_changed=*" } | Select-Object -First 1
    $compareNextActionCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_current=*" } | Select-Object -First 1
    $compareNextActionPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_previous=*" } | Select-Object -First 1
    $compareNextActionChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_changed=*" } | Select-Object -First 1
    $compareNextActionReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_reason_current=*" } | Select-Object -First 1
    $compareNextActionReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_reason_previous=*" } | Select-Object -First 1
    $compareNextActionReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_reason_changed=*" } | Select-Object -First 1
    $compareNextActionCommandHintCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_current=*" } | Select-Object -First 1
    $compareNextActionCommandHintPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_previous=*" } | Select-Object -First 1
    $compareNextActionCommandHintChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_changed=*" } | Select-Object -First 1
    $compareNextActionCommandHintSourceCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_source_current=*" } | Select-Object -First 1
    $compareNextActionCommandHintSourcePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_source_previous=*" } | Select-Object -First 1
    $compareNextActionCommandHintSourceChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_command_hint_source_changed=*" } | Select-Object -First 1
    $compareNextActionPathCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_path_current=*" } | Select-Object -First 1
    $compareNextActionPathPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_path_previous=*" } | Select-Object -First 1
    $compareNextActionPathChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_next_action_path_changed=*" } | Select-Object -First 1
    $compareEntryPointAnalysisPathCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_analysis_path_current=*" } | Select-Object -First 1
    $compareEntryPointAnalysisPathPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_analysis_path_previous=*" } | Select-Object -First 1
    $compareEntryPointAnalysisPathChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_analysis_path_changed=*" } | Select-Object -First 1
    $compareEntryPointReadinessCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_readiness_current=*" } | Select-Object -First 1
    $compareEntryPointReadinessPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_readiness_previous=*" } | Select-Object -First 1
    $compareEntryPointReadinessChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_readiness_changed=*" } | Select-Object -First 1
    $compareEntryPointReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_reason_current=*" } | Select-Object -First 1
    $compareEntryPointReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_reason_previous=*" } | Select-Object -First 1
    $compareEntryPointReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_reason_changed=*" } | Select-Object -First 1
    $compareEntryPointCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_count_current=*" } | Select-Object -First 1
    $compareEntryPointCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_count_previous=*" } | Select-Object -First 1
    $compareEntryPointCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_count_changed=*" } | Select-Object -First 1
    $compareEntryPointBranchAmbiguityCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_branch_ambiguity_current=*" } | Select-Object -First 1
    $compareEntryPointBranchAmbiguityPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_branch_ambiguity_previous=*" } | Select-Object -First 1
    $compareEntryPointBranchAmbiguityChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_entry_point_branch_ambiguity_changed=*" } | Select-Object -First 1
    $comparePostPlayPackageCampaignIdentityStateSummaryCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_post_play_package_campaign_identity_state_summary_current=*" } | Select-Object -First 1
    $comparePostPlayPackageCampaignIdentityStateSummaryPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_post_play_package_campaign_identity_state_summary_previous=*" } | Select-Object -First 1
    $comparePostPlayPackageCampaignIdentityStateSummaryChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_post_play_package_campaign_identity_state_summary_changed=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishAllowedCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_allowed_current=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishAllowedPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_allowed_previous=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishAllowedChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_allowed_changed=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_state_current=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_state_previous=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_state_changed=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_reason_current=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_reason_previous=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_reason_changed=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateCanPublishCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_can_publish_current=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateCanPublishPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_can_publish_previous=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGateCanPublishChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_can_publish_changed=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGatePublishCommandCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_publish_command_current=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGatePublishCommandPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_publish_command_previous=*" } | Select-Object -First 1
    $compareGeneratedOverlayPublishGatePublishCommandChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_generated_overlay_publish_gate_publish_command_changed=*" } | Select-Object -First 1
    $compareMemoryRecoveryAnchorEntryPointIdCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_memory_recovery_anchor_entry_point_id_current=*" } | Select-Object -First 1
    $compareMemoryRecoveryAnchorEntryPointIdPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_memory_recovery_anchor_entry_point_id_previous=*" } | Select-Object -First 1
    $compareMemoryRecoveryAnchorEntryPointIdChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_memory_recovery_anchor_entry_point_id_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPresentCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_present_current=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPresentPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_present_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPresentChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_present_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPathCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_path_current=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPathPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_path_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanPathChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_path_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanSourceCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_source_current=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanSourcePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_source_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanSourceChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_source_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReadinessCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_readiness_current=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReadinessPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_readiness_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReadinessChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_readiness_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_reason_current=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_reason_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryPlanReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_plan_reason_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryLimitReachedCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_limit_reached_current=*" } | Select-Object -First 1
    $compareCampaignLibraryLimitReachedPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_limit_reached_previous=*" } | Select-Object -First 1
    $compareCampaignLibraryLimitReachedChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_limit_reached_changed=*" } | Select-Object -First 1
    $compareCampaignLibrarySkippedDueToLimitCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_skipped_due_to_limit_count_current=*" } | Select-Object -First 1
    $compareCampaignLibrarySkippedDueToLimitCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_skipped_due_to_limit_count_previous=*" } | Select-Object -First 1
    $compareCampaignLibrarySkippedDueToLimitCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_skipped_due_to_limit_count_changed=*" } | Select-Object -First 1
    $compareCampaignLibraryFollowUpActiveLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_follow_up_active=*" } | Select-Object -First 1
    $compareCampaignLibraryFollowUpReasonLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_campaign_library_follow_up_reason=*" } | Select-Object -First 1
    $compareMpWarningCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_count_current=*" } | Select-Object -First 1
    $compareMpWarningCountDeltaLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_count_delta=*" } | Select-Object -First 1
    $compareMpWarningCodesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_warning_codes_changed=*" } | Select-Object -First 1
    $compareMpManifestHashCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_current=*" } | Select-Object -First 1
    $compareMpManifestHashPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_previous=*" } | Select-Object -First 1
    $compareMpManifestHashChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_changed=*" } | Select-Object -First 1
    $compareMpPackageOutputDirCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_current=*" } | Select-Object -First 1
    $compareMpPackageOutputDirPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_previous=*" } | Select-Object -First 1
    $compareMpPackageOutputDirChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_output_dir_changed=*" } | Select-Object -First 1
    $compareMpPackageZipStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_state_current=*" } | Select-Object -First 1
    $compareMpPackageZipStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_state_previous=*" } | Select-Object -First 1
    $compareMpPackageZipStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_state_changed=*" } | Select-Object -First 1
    $compareMpPackageZipReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_reason_current=*" } | Select-Object -First 1
    $compareMpPackageZipReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_reason_previous=*" } | Select-Object -First 1
    $compareMpPackageZipReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_reason_changed=*" } | Select-Object -First 1
    $compareMpPackageZipSha256CurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_sha256_current=*" } | Select-Object -First 1
    $compareMpPackageZipSha256PreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_sha256_previous=*" } | Select-Object -First 1
    $compareMpPackageZipSha256ChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_sha256_changed=*" } | Select-Object -First 1
    $compareMpPackageZipPathCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_path_current=*" } | Select-Object -First 1
    $compareMpPackageZipPathPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_path_previous=*" } | Select-Object -First 1
    $compareMpPackageZipPathChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_path_changed=*" } | Select-Object -First 1
    $compareMpPackageZipBytesCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_bytes_current=*" } | Select-Object -First 1
    $compareMpPackageZipBytesPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_bytes_previous=*" } | Select-Object -First 1
    $compareMpPackageZipBytesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_package_zip_bytes_changed=*" } | Select-Object -First 1
    $compareMpProvenanceStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_provenance_state_current=*" } | Select-Object -First 1
    $compareMpProvenanceStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_provenance_state_previous=*" } | Select-Object -First 1
    $compareMpProvenanceStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_provenance_state_changed=*" } | Select-Object -First 1
    $compareMpSourceQualityCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_quality_count_current=*" } | Select-Object -First 1
    $compareMpSourceQualityCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_quality_count_previous=*" } | Select-Object -First 1
    $compareMpSourceQualityCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_quality_count_changed=*" } | Select-Object -First 1
    $compareMpSourceQualityPreviousLines = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_quality_previous=*" }
    $compareMpSourceQualityCurrentLines = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_quality_current=*" }
    $compareMpSourceQualitiesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_source_qualities_changed=*" } | Select-Object -First 1
    $compareMpBootstrapCampaignCountCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_bootstrap_campaign_count_current=*" } | Select-Object -First 1
    $compareMpBootstrapCampaignCountPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_bootstrap_campaign_count_previous=*" } | Select-Object -First 1
    $compareMpBootstrapCampaignCountChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_bootstrap_campaign_count_changed=*" } | Select-Object -First 1
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
    $compareMpHandoffStatusCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_handoff_status_current=*" } | Select-Object -First 1
    $compareMpHandoffStatusPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_handoff_status_previous=*" } | Select-Object -First 1
    $compareMpHandoffStatusChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_handoff_status_changed=*" } | Select-Object -First 1
    $compareMpPreviousHostAvailableCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_previous_host_available_current=*" } | Select-Object -First 1
    $compareMpPreviousHostAvailablePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_previous_host_available_previous=*" } | Select-Object -First 1
    $compareMpPreviousHostAvailableChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_previous_host_available_changed=*" } | Select-Object -First 1
    $compareMpHandoffFollowUpActiveLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_handoff_follow_up_active=*" } | Select-Object -First 1
    $compareMpHandoffFollowUpReasonLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_handoff_follow_up_reason=*" } | Select-Object -First 1
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
    $compareMpGameVersionMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_game_version_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpGameVersionMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_game_version_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpGameVersionMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_game_version_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpCampaignIdMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_campaign_id_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpCampaignIdMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_campaign_id_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpCampaignIdMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_campaign_id_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpOverlayVersionMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_overlay_version_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpOverlayVersionMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_overlay_version_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpOverlayVersionMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_overlay_version_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpModVersionMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mod_version_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpModVersionMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mod_version_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpModVersionMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mod_version_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpManifestHashMismatchWarningCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_mismatch_warning_current=*" } | Select-Object -First 1
    $compareMpManifestHashMismatchWarningPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_mismatch_warning_previous=*" } | Select-Object -First 1
    $compareMpManifestHashMismatchWarningChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_manifest_hash_mismatch_warning_changed=*" } | Select-Object -First 1
    $compareMpMismatchWarningStateCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_state_current=*" } | Select-Object -First 1
    $compareMpMismatchWarningStatePreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_state_previous=*" } | Select-Object -First 1
    $compareMpMismatchWarningStateChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_state_changed=*" } | Select-Object -First 1
    $compareMpMismatchWarningReasonCurrentLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_reason_current=*" } | Select-Object -First 1
    $compareMpMismatchWarningReasonPreviousLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_reason_previous=*" } | Select-Object -First 1
    $compareMpMismatchWarningReasonChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_reason_changed=*" } | Select-Object -First 1
    $compareMpMismatchWarningCodesChangedLine = $compareLines | Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_codes_changed=*" } | Select-Object -First 1
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
    $latestMpMismatchWarningCodesPrevious = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_code_previous=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_mismatch_warning_code_previous=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Sort-Object -Unique
    )
    $latestMpMismatchWarningCodesCurrent = @(
        $compareLines |
            Where-Object { $_ -like "real_session_v0_compare_mp_mismatch_warning_code_current=*" } |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_mismatch_warning_code_current=".Length) } |
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
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterStateCurrentLine)) {
        $latestStatusCenterStateCurrent = $compareStatusCenterStateCurrentLine.Substring("real_session_v0_compare_status_center_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterStatePreviousLine)) {
        $latestStatusCenterStatePrevious = $compareStatusCenterStatePreviousLine.Substring("real_session_v0_compare_status_center_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterStateChangedLine)) {
        $latestStatusCenterStateChanged = $compareStatusCenterStateChangedLine.Substring("real_session_v0_compare_status_center_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterReasonCurrentLine)) {
        $latestStatusCenterReasonCurrent = $compareStatusCenterReasonCurrentLine.Substring("real_session_v0_compare_status_center_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterReasonPreviousLine)) {
        $latestStatusCenterReasonPrevious = $compareStatusCenterReasonPreviousLine.Substring("real_session_v0_compare_status_center_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterReasonChangedLine)) {
        $latestStatusCenterReasonChanged = $compareStatusCenterReasonChangedLine.Substring("real_session_v0_compare_status_center_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterSummaryTextCurrentLine)) {
        $latestStatusCenterSummaryTextCurrent = $compareStatusCenterSummaryTextCurrentLine.Substring("real_session_v0_compare_status_center_summary_text_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterSummaryTextPreviousLine)) {
        $latestStatusCenterSummaryTextPrevious = $compareStatusCenterSummaryTextPreviousLine.Substring("real_session_v0_compare_status_center_summary_text_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareStatusCenterSummaryTextChangedLine)) {
        $latestStatusCenterSummaryTextChanged = $compareStatusCenterSummaryTextChangedLine.Substring("real_session_v0_compare_status_center_summary_text_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootResolutionCurrentLine)) {
        $latestSaveRootResolutionCurrent = $compareSaveRootResolutionCurrentLine.Substring("real_session_v0_compare_save_root_resolution_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootResolutionPreviousLine)) {
        $latestSaveRootResolutionPrevious = $compareSaveRootResolutionPreviousLine.Substring("real_session_v0_compare_save_root_resolution_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootResolutionChangedLine)) {
        $latestSaveRootResolutionChanged = $compareSaveRootResolutionChangedLine.Substring("real_session_v0_compare_save_root_resolution_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSourceCurrentLine)) {
        $latestSaveRootSourceCurrent = $compareSaveRootSourceCurrentLine.Substring("real_session_v0_compare_save_root_source_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSourcePreviousLine)) {
        $latestSaveRootSourcePrevious = $compareSaveRootSourcePreviousLine.Substring("real_session_v0_compare_save_root_source_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSourceChangedLine)) {
        $latestSaveRootSourceChanged = $compareSaveRootSourceChangedLine.Substring("real_session_v0_compare_save_root_source_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootPathCurrentLine)) {
        $latestSaveRootPathCurrent = $compareSaveRootPathCurrentLine.Substring("real_session_v0_compare_save_root_path_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootPathPreviousLine)) {
        $latestSaveRootPathPrevious = $compareSaveRootPathPreviousLine.Substring("real_session_v0_compare_save_root_path_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootPathChangedLine)) {
        $latestSaveRootPathChanged = $compareSaveRootPathChangedLine.Substring("real_session_v0_compare_save_root_path_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootCampaignCountCurrentLine)) {
        $latestSaveRootCampaignCountCurrent = $compareSaveRootCampaignCountCurrentLine.Substring("real_session_v0_compare_save_root_campaign_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootCampaignCountPreviousLine)) {
        $latestSaveRootCampaignCountPrevious = $compareSaveRootCampaignCountPreviousLine.Substring("real_session_v0_compare_save_root_campaign_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootCampaignCountChangedLine)) {
        $latestSaveRootCampaignCountChanged = $compareSaveRootCampaignCountChangedLine.Substring("real_session_v0_compare_save_root_campaign_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSaveFileCountCurrentLine)) {
        $latestSaveRootSaveFileCountCurrent = $compareSaveRootSaveFileCountCurrentLine.Substring("real_session_v0_compare_save_root_save_file_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSaveFileCountPreviousLine)) {
        $latestSaveRootSaveFileCountPrevious = $compareSaveRootSaveFileCountPreviousLine.Substring("real_session_v0_compare_save_root_save_file_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootSaveFileCountChangedLine)) {
        $latestSaveRootSaveFileCountChanged = $compareSaveRootSaveFileCountChangedLine.Substring("real_session_v0_compare_save_root_save_file_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootAutosaveAnchorCountCurrentLine)) {
        $latestSaveRootAutosaveAnchorCountCurrent = $compareSaveRootAutosaveAnchorCountCurrentLine.Substring("real_session_v0_compare_save_root_autosave_anchor_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootAutosaveAnchorCountPreviousLine)) {
        $latestSaveRootAutosaveAnchorCountPrevious = $compareSaveRootAutosaveAnchorCountPreviousLine.Substring("real_session_v0_compare_save_root_autosave_anchor_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareSaveRootAutosaveAnchorCountChangedLine)) {
        $latestSaveRootAutosaveAnchorCountChanged = $compareSaveRootAutosaveAnchorCountChangedLine.Substring("real_session_v0_compare_save_root_autosave_anchor_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCurrentLine)) {
        $latestNextActionCurrent = $compareNextActionCurrentLine.Substring("real_session_v0_compare_next_action_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionPreviousLine)) {
        $latestNextActionPrevious = $compareNextActionPreviousLine.Substring("real_session_v0_compare_next_action_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionChangedLine)) {
        $latestNextActionChanged = $compareNextActionChangedLine.Substring("real_session_v0_compare_next_action_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionReasonCurrentLine)) {
        $latestNextActionReasonCurrent = $compareNextActionReasonCurrentLine.Substring("real_session_v0_compare_next_action_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionReasonPreviousLine)) {
        $latestNextActionReasonPrevious = $compareNextActionReasonPreviousLine.Substring("real_session_v0_compare_next_action_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionReasonChangedLine)) {
        $latestNextActionReasonChanged = $compareNextActionReasonChangedLine.Substring("real_session_v0_compare_next_action_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintCurrentLine)) {
        $latestNextActionCommandHintCurrent = $compareNextActionCommandHintCurrentLine.Substring("real_session_v0_compare_next_action_command_hint_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintPreviousLine)) {
        $latestNextActionCommandHintPrevious = $compareNextActionCommandHintPreviousLine.Substring("real_session_v0_compare_next_action_command_hint_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintChangedLine)) {
        $latestNextActionCommandHintChanged = $compareNextActionCommandHintChangedLine.Substring("real_session_v0_compare_next_action_command_hint_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintSourceCurrentLine)) {
        $latestNextActionCommandHintSourceCurrent = $compareNextActionCommandHintSourceCurrentLine.Substring("real_session_v0_compare_next_action_command_hint_source_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintSourcePreviousLine)) {
        $latestNextActionCommandHintSourcePrevious = $compareNextActionCommandHintSourcePreviousLine.Substring("real_session_v0_compare_next_action_command_hint_source_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionCommandHintSourceChangedLine)) {
        $latestNextActionCommandHintSourceChanged = $compareNextActionCommandHintSourceChangedLine.Substring("real_session_v0_compare_next_action_command_hint_source_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionPathCurrentLine)) {
        $latestNextActionPathCurrent = $compareNextActionPathCurrentLine.Substring("real_session_v0_compare_next_action_path_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionPathPreviousLine)) {
        $latestNextActionPathPrevious = $compareNextActionPathPreviousLine.Substring("real_session_v0_compare_next_action_path_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareNextActionPathChangedLine)) {
        $latestNextActionPathChanged = $compareNextActionPathChangedLine.Substring("real_session_v0_compare_next_action_path_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointAnalysisPathCurrentLine)) {
        $latestEntryPointAnalysisPathCurrent = $compareEntryPointAnalysisPathCurrentLine.Substring("real_session_v0_compare_entry_point_analysis_path_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointAnalysisPathPreviousLine)) {
        $latestEntryPointAnalysisPathPrevious = $compareEntryPointAnalysisPathPreviousLine.Substring("real_session_v0_compare_entry_point_analysis_path_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointAnalysisPathChangedLine)) {
        $latestEntryPointAnalysisPathChanged = $compareEntryPointAnalysisPathChangedLine.Substring("real_session_v0_compare_entry_point_analysis_path_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReadinessCurrentLine)) {
        $latestEntryPointReadinessCurrent = $compareEntryPointReadinessCurrentLine.Substring("real_session_v0_compare_entry_point_readiness_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReadinessPreviousLine)) {
        $latestEntryPointReadinessPrevious = $compareEntryPointReadinessPreviousLine.Substring("real_session_v0_compare_entry_point_readiness_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReadinessChangedLine)) {
        $latestEntryPointReadinessChanged = $compareEntryPointReadinessChangedLine.Substring("real_session_v0_compare_entry_point_readiness_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReasonCurrentLine)) {
        $latestEntryPointReasonCurrent = $compareEntryPointReasonCurrentLine.Substring("real_session_v0_compare_entry_point_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReasonPreviousLine)) {
        $latestEntryPointReasonPrevious = $compareEntryPointReasonPreviousLine.Substring("real_session_v0_compare_entry_point_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointReasonChangedLine)) {
        $latestEntryPointReasonChanged = $compareEntryPointReasonChangedLine.Substring("real_session_v0_compare_entry_point_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointCountCurrentLine)) {
        $latestEntryPointCountCurrent = $compareEntryPointCountCurrentLine.Substring("real_session_v0_compare_entry_point_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointCountPreviousLine)) {
        $latestEntryPointCountPrevious = $compareEntryPointCountPreviousLine.Substring("real_session_v0_compare_entry_point_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointCountChangedLine)) {
        $latestEntryPointCountChanged = $compareEntryPointCountChangedLine.Substring("real_session_v0_compare_entry_point_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointBranchAmbiguityCurrentLine)) {
        $latestEntryPointBranchAmbiguityCurrent = $compareEntryPointBranchAmbiguityCurrentLine.Substring("real_session_v0_compare_entry_point_branch_ambiguity_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointBranchAmbiguityPreviousLine)) {
        $latestEntryPointBranchAmbiguityPrevious = $compareEntryPointBranchAmbiguityPreviousLine.Substring("real_session_v0_compare_entry_point_branch_ambiguity_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareEntryPointBranchAmbiguityChangedLine)) {
        $latestEntryPointBranchAmbiguityChanged = $compareEntryPointBranchAmbiguityChangedLine.Substring("real_session_v0_compare_entry_point_branch_ambiguity_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($comparePostPlayPackageCampaignIdentityStateSummaryCurrentLine)) {
        $latestPostPlayPackageCampaignIdentityStateSummaryCurrent = $comparePostPlayPackageCampaignIdentityStateSummaryCurrentLine.Substring("real_session_v0_compare_post_play_package_campaign_identity_state_summary_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($comparePostPlayPackageCampaignIdentityStateSummaryPreviousLine)) {
        $latestPostPlayPackageCampaignIdentityStateSummaryPrevious = $comparePostPlayPackageCampaignIdentityStateSummaryPreviousLine.Substring("real_session_v0_compare_post_play_package_campaign_identity_state_summary_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($comparePostPlayPackageCampaignIdentityStateSummaryChangedLine)) {
        $latestPostPlayPackageCampaignIdentityStateSummaryChanged = $comparePostPlayPackageCampaignIdentityStateSummaryChangedLine.Substring("real_session_v0_compare_post_play_package_campaign_identity_state_summary_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishAllowedCurrentLine)) {
        $latestGeneratedOverlayPublishAllowedCurrent = $compareGeneratedOverlayPublishAllowedCurrentLine.Substring("real_session_v0_compare_generated_overlay_publish_allowed_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishAllowedPreviousLine)) {
        $latestGeneratedOverlayPublishAllowedPrevious = $compareGeneratedOverlayPublishAllowedPreviousLine.Substring("real_session_v0_compare_generated_overlay_publish_allowed_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishAllowedChangedLine)) {
        $latestGeneratedOverlayPublishAllowedChanged = $compareGeneratedOverlayPublishAllowedChangedLine.Substring("real_session_v0_compare_generated_overlay_publish_allowed_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateStateCurrentLine)) {
        $latestGeneratedOverlayPublishGateStateCurrent = $compareGeneratedOverlayPublishGateStateCurrentLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateStatePreviousLine)) {
        $latestGeneratedOverlayPublishGateStatePrevious = $compareGeneratedOverlayPublishGateStatePreviousLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateStateChangedLine)) {
        $latestGeneratedOverlayPublishGateStateChanged = $compareGeneratedOverlayPublishGateStateChangedLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateReasonCurrentLine)) {
        $latestGeneratedOverlayPublishGateReasonCurrent = $compareGeneratedOverlayPublishGateReasonCurrentLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateReasonPreviousLine)) {
        $latestGeneratedOverlayPublishGateReasonPrevious = $compareGeneratedOverlayPublishGateReasonPreviousLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateReasonChangedLine)) {
        $latestGeneratedOverlayPublishGateReasonChanged = $compareGeneratedOverlayPublishGateReasonChangedLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateCanPublishCurrentLine)) {
        $latestGeneratedOverlayPublishGateCanPublishCurrent = $compareGeneratedOverlayPublishGateCanPublishCurrentLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_can_publish_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateCanPublishPreviousLine)) {
        $latestGeneratedOverlayPublishGateCanPublishPrevious = $compareGeneratedOverlayPublishGateCanPublishPreviousLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_can_publish_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGateCanPublishChangedLine)) {
        $latestGeneratedOverlayPublishGateCanPublishChanged = $compareGeneratedOverlayPublishGateCanPublishChangedLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_can_publish_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGatePublishCommandCurrentLine)) {
        $latestGeneratedOverlayPublishGatePublishCommandCurrent = $compareGeneratedOverlayPublishGatePublishCommandCurrentLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_publish_command_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGatePublishCommandPreviousLine)) {
        $latestGeneratedOverlayPublishGatePublishCommandPrevious = $compareGeneratedOverlayPublishGatePublishCommandPreviousLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_publish_command_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareGeneratedOverlayPublishGatePublishCommandChangedLine)) {
        $latestGeneratedOverlayPublishGatePublishCommandChanged = $compareGeneratedOverlayPublishGatePublishCommandChangedLine.Substring("real_session_v0_compare_generated_overlay_publish_gate_publish_command_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMemoryRecoveryAnchorEntryPointIdCurrentLine)) {
        $latestMemoryRecoveryAnchorEntryPointIdCurrent = $compareMemoryRecoveryAnchorEntryPointIdCurrentLine.Substring("real_session_v0_compare_memory_recovery_anchor_entry_point_id_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMemoryRecoveryAnchorEntryPointIdPreviousLine)) {
        $latestMemoryRecoveryAnchorEntryPointIdPrevious = $compareMemoryRecoveryAnchorEntryPointIdPreviousLine.Substring("real_session_v0_compare_memory_recovery_anchor_entry_point_id_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMemoryRecoveryAnchorEntryPointIdChangedLine)) {
        $latestMemoryRecoveryAnchorEntryPointIdChanged = $compareMemoryRecoveryAnchorEntryPointIdChangedLine.Substring("real_session_v0_compare_memory_recovery_anchor_entry_point_id_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPresentCurrentLine)) {
        $latestCampaignLibraryPlanPresentCurrent = $compareCampaignLibraryPlanPresentCurrentLine.Substring("real_session_v0_compare_campaign_library_plan_present_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPresentPreviousLine)) {
        $latestCampaignLibraryPlanPresentPrevious = $compareCampaignLibraryPlanPresentPreviousLine.Substring("real_session_v0_compare_campaign_library_plan_present_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPresentChangedLine)) {
        $latestCampaignLibraryPlanPresentChanged = $compareCampaignLibraryPlanPresentChangedLine.Substring("real_session_v0_compare_campaign_library_plan_present_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPathCurrentLine)) {
        $latestCampaignLibraryPlanPathCurrent = $compareCampaignLibraryPlanPathCurrentLine.Substring("real_session_v0_compare_campaign_library_plan_path_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPathPreviousLine)) {
        $latestCampaignLibraryPlanPathPrevious = $compareCampaignLibraryPlanPathPreviousLine.Substring("real_session_v0_compare_campaign_library_plan_path_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanPathChangedLine)) {
        $latestCampaignLibraryPlanPathChanged = $compareCampaignLibraryPlanPathChangedLine.Substring("real_session_v0_compare_campaign_library_plan_path_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanSourceCurrentLine)) {
        $latestCampaignLibraryPlanSourceCurrent = $compareCampaignLibraryPlanSourceCurrentLine.Substring("real_session_v0_compare_campaign_library_plan_source_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanSourcePreviousLine)) {
        $latestCampaignLibraryPlanSourcePrevious = $compareCampaignLibraryPlanSourcePreviousLine.Substring("real_session_v0_compare_campaign_library_plan_source_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanSourceChangedLine)) {
        $latestCampaignLibraryPlanSourceChanged = $compareCampaignLibraryPlanSourceChangedLine.Substring("real_session_v0_compare_campaign_library_plan_source_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReadinessCurrentLine)) {
        $latestCampaignLibraryPlanReadinessCurrent = $compareCampaignLibraryPlanReadinessCurrentLine.Substring("real_session_v0_compare_campaign_library_plan_readiness_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReadinessPreviousLine)) {
        $latestCampaignLibraryPlanReadinessPrevious = $compareCampaignLibraryPlanReadinessPreviousLine.Substring("real_session_v0_compare_campaign_library_plan_readiness_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReadinessChangedLine)) {
        $latestCampaignLibraryPlanReadinessChanged = $compareCampaignLibraryPlanReadinessChangedLine.Substring("real_session_v0_compare_campaign_library_plan_readiness_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReasonCurrentLine)) {
        $latestCampaignLibraryPlanReasonCurrent = $compareCampaignLibraryPlanReasonCurrentLine.Substring("real_session_v0_compare_campaign_library_plan_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReasonPreviousLine)) {
        $latestCampaignLibraryPlanReasonPrevious = $compareCampaignLibraryPlanReasonPreviousLine.Substring("real_session_v0_compare_campaign_library_plan_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryPlanReasonChangedLine)) {
        $latestCampaignLibraryPlanReasonChanged = $compareCampaignLibraryPlanReasonChangedLine.Substring("real_session_v0_compare_campaign_library_plan_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryLimitReachedCurrentLine)) {
        $latestCampaignLibraryLimitReachedCurrent = $compareCampaignLibraryLimitReachedCurrentLine.Substring("real_session_v0_compare_campaign_library_limit_reached_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryLimitReachedPreviousLine)) {
        $latestCampaignLibraryLimitReachedPrevious = $compareCampaignLibraryLimitReachedPreviousLine.Substring("real_session_v0_compare_campaign_library_limit_reached_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryLimitReachedChangedLine)) {
        $latestCampaignLibraryLimitReachedChanged = $compareCampaignLibraryLimitReachedChangedLine.Substring("real_session_v0_compare_campaign_library_limit_reached_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibrarySkippedDueToLimitCountCurrentLine)) {
        $latestCampaignLibrarySkippedDueToLimitCountCurrent = $compareCampaignLibrarySkippedDueToLimitCountCurrentLine.Substring("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibrarySkippedDueToLimitCountPreviousLine)) {
        $latestCampaignLibrarySkippedDueToLimitCountPrevious = $compareCampaignLibrarySkippedDueToLimitCountPreviousLine.Substring("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibrarySkippedDueToLimitCountChangedLine)) {
        $latestCampaignLibrarySkippedDueToLimitCountChanged = $compareCampaignLibrarySkippedDueToLimitCountChangedLine.Substring("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryFollowUpActiveLine)) {
        $latestCampaignLibraryFollowUpActive = $compareCampaignLibraryFollowUpActiveLine.Substring("real_session_v0_compare_campaign_library_follow_up_active=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareCampaignLibraryFollowUpReasonLine)) {
        $latestCampaignLibraryFollowUpReason = $compareCampaignLibraryFollowUpReasonLine.Substring("real_session_v0_compare_campaign_library_follow_up_reason=".Length)
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
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipStateCurrentLine)) {
        $latestMpPackageZipStateCurrent = $compareMpPackageZipStateCurrentLine.Substring("real_session_v0_compare_mp_package_zip_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipStatePreviousLine)) {
        $latestMpPackageZipStatePrevious = $compareMpPackageZipStatePreviousLine.Substring("real_session_v0_compare_mp_package_zip_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipStateChangedLine)) {
        $latestMpPackageZipStateChanged = $compareMpPackageZipStateChangedLine.Substring("real_session_v0_compare_mp_package_zip_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipReasonCurrentLine)) {
        $latestMpPackageZipReasonCurrent = $compareMpPackageZipReasonCurrentLine.Substring("real_session_v0_compare_mp_package_zip_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipReasonPreviousLine)) {
        $latestMpPackageZipReasonPrevious = $compareMpPackageZipReasonPreviousLine.Substring("real_session_v0_compare_mp_package_zip_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipReasonChangedLine)) {
        $latestMpPackageZipReasonChanged = $compareMpPackageZipReasonChangedLine.Substring("real_session_v0_compare_mp_package_zip_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipSha256CurrentLine)) {
        $latestMpPackageZipSha256Current = $compareMpPackageZipSha256CurrentLine.Substring("real_session_v0_compare_mp_package_zip_sha256_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipSha256PreviousLine)) {
        $latestMpPackageZipSha256Previous = $compareMpPackageZipSha256PreviousLine.Substring("real_session_v0_compare_mp_package_zip_sha256_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipSha256ChangedLine)) {
        $latestMpPackageZipSha256Changed = $compareMpPackageZipSha256ChangedLine.Substring("real_session_v0_compare_mp_package_zip_sha256_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipPathCurrentLine)) {
        $latestMpPackageZipPathCurrent = $compareMpPackageZipPathCurrentLine.Substring("real_session_v0_compare_mp_package_zip_path_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipPathPreviousLine)) {
        $latestMpPackageZipPathPrevious = $compareMpPackageZipPathPreviousLine.Substring("real_session_v0_compare_mp_package_zip_path_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipPathChangedLine)) {
        $latestMpPackageZipPathChanged = $compareMpPackageZipPathChangedLine.Substring("real_session_v0_compare_mp_package_zip_path_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipBytesCurrentLine)) {
        $latestMpPackageZipBytesCurrent = $compareMpPackageZipBytesCurrentLine.Substring("real_session_v0_compare_mp_package_zip_bytes_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipBytesPreviousLine)) {
        $latestMpPackageZipBytesPrevious = $compareMpPackageZipBytesPreviousLine.Substring("real_session_v0_compare_mp_package_zip_bytes_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageZipBytesChangedLine)) {
        $latestMpPackageZipBytesChanged = $compareMpPackageZipBytesChangedLine.Substring("real_session_v0_compare_mp_package_zip_bytes_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpProvenanceStateCurrentLine)) {
        $latestMpProvenanceStateCurrent = $compareMpProvenanceStateCurrentLine.Substring("real_session_v0_compare_mp_provenance_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpProvenanceStatePreviousLine)) {
        $latestMpProvenanceStatePrevious = $compareMpProvenanceStatePreviousLine.Substring("real_session_v0_compare_mp_provenance_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpProvenanceStateChangedLine)) {
        $latestMpProvenanceStateChanged = $compareMpProvenanceStateChangedLine.Substring("real_session_v0_compare_mp_provenance_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpSourceQualityCountCurrentLine)) {
        $latestMpSourceQualityCountCurrent = $compareMpSourceQualityCountCurrentLine.Substring("real_session_v0_compare_mp_source_quality_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpSourceQualityCountPreviousLine)) {
        $latestMpSourceQualityCountPrevious = $compareMpSourceQualityCountPreviousLine.Substring("real_session_v0_compare_mp_source_quality_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpSourceQualityCountChangedLine)) {
        $latestMpSourceQualityCountChanged = $compareMpSourceQualityCountChangedLine.Substring("real_session_v0_compare_mp_source_quality_count_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpSourceQualitiesChangedLine)) {
        $latestMpSourceQualitiesChanged = $compareMpSourceQualitiesChangedLine.Substring("real_session_v0_compare_mp_source_qualities_changed=".Length)
    }
    $latestMpSourceQualitiesPrevious = @(
        $compareMpSourceQualityPreviousLines |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_source_quality_previous=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    $latestMpSourceQualitiesCurrent = @(
        $compareMpSourceQualityCurrentLines |
            ForEach-Object { $_.Substring("real_session_v0_compare_mp_source_quality_current=".Length) } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if (-not [string]::IsNullOrWhiteSpace($compareMpBootstrapCampaignCountCurrentLine)) {
        $latestMpBootstrapCampaignCountCurrent = $compareMpBootstrapCampaignCountCurrentLine.Substring("real_session_v0_compare_mp_bootstrap_campaign_count_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpBootstrapCampaignCountPreviousLine)) {
        $latestMpBootstrapCampaignCountPrevious = $compareMpBootstrapCampaignCountPreviousLine.Substring("real_session_v0_compare_mp_bootstrap_campaign_count_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpBootstrapCampaignCountChangedLine)) {
        $latestMpBootstrapCampaignCountChanged = $compareMpBootstrapCampaignCountChangedLine.Substring("real_session_v0_compare_mp_bootstrap_campaign_count_changed=".Length)
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
    if (-not [string]::IsNullOrWhiteSpace($compareMpHandoffStatusCurrentLine)) {
        $latestMpHandoffStatusCurrent = $compareMpHandoffStatusCurrentLine.Substring("real_session_v0_compare_mp_handoff_status_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHandoffStatusPreviousLine)) {
        $latestMpHandoffStatusPrevious = $compareMpHandoffStatusPreviousLine.Substring("real_session_v0_compare_mp_handoff_status_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHandoffStatusChangedLine)) {
        $latestMpHandoffStatusChanged = $compareMpHandoffStatusChangedLine.Substring("real_session_v0_compare_mp_handoff_status_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPreviousHostAvailableCurrentLine)) {
        $latestMpPreviousHostAvailableCurrent = $compareMpPreviousHostAvailableCurrentLine.Substring("real_session_v0_compare_mp_previous_host_available_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPreviousHostAvailablePreviousLine)) {
        $latestMpPreviousHostAvailablePrevious = $compareMpPreviousHostAvailablePreviousLine.Substring("real_session_v0_compare_mp_previous_host_available_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPreviousHostAvailableChangedLine)) {
        $latestMpPreviousHostAvailableChanged = $compareMpPreviousHostAvailableChangedLine.Substring("real_session_v0_compare_mp_previous_host_available_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHandoffFollowUpActiveLine)) {
        $latestMpHandoffFollowUpActive = $compareMpHandoffFollowUpActiveLine.Substring("real_session_v0_compare_mp_handoff_follow_up_active=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHandoffFollowUpReasonLine)) {
        $latestMpHandoffFollowUpReason = $compareMpHandoffFollowUpReasonLine.Substring("real_session_v0_compare_mp_handoff_follow_up_reason=".Length)
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
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningCurrentLine)) {
        $latestMpGameVersionMismatchWarningCurrent = $compareMpGameVersionMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_game_version_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningPreviousLine)) {
        $latestMpGameVersionMismatchWarningPrevious = $compareMpGameVersionMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_game_version_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningChangedLine)) {
        $latestMpGameVersionMismatchWarningChanged = $compareMpGameVersionMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_game_version_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningCurrentLine)) {
        $latestMpCampaignIdMismatchWarningCurrent = $compareMpCampaignIdMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_campaign_id_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningPreviousLine)) {
        $latestMpCampaignIdMismatchWarningPrevious = $compareMpCampaignIdMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_campaign_id_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningChangedLine)) {
        $latestMpCampaignIdMismatchWarningChanged = $compareMpCampaignIdMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_campaign_id_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningCurrentLine)) {
        $latestMpOverlayVersionMismatchWarningCurrent = $compareMpOverlayVersionMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_overlay_version_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningPreviousLine)) {
        $latestMpOverlayVersionMismatchWarningPrevious = $compareMpOverlayVersionMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_overlay_version_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningChangedLine)) {
        $latestMpOverlayVersionMismatchWarningChanged = $compareMpOverlayVersionMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_overlay_version_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningCurrentLine)) {
        $latestMpModVersionMismatchWarningCurrent = $compareMpModVersionMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_mod_version_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningPreviousLine)) {
        $latestMpModVersionMismatchWarningPrevious = $compareMpModVersionMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_mod_version_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningChangedLine)) {
        $latestMpModVersionMismatchWarningChanged = $compareMpModVersionMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_mod_version_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningCurrentLine)) {
        $latestMpManifestHashMismatchWarningCurrent = $compareMpManifestHashMismatchWarningCurrentLine.Substring("real_session_v0_compare_mp_manifest_hash_mismatch_warning_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningPreviousLine)) {
        $latestMpManifestHashMismatchWarningPrevious = $compareMpManifestHashMismatchWarningPreviousLine.Substring("real_session_v0_compare_mp_manifest_hash_mismatch_warning_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningChangedLine)) {
        $latestMpManifestHashMismatchWarningChanged = $compareMpManifestHashMismatchWarningChangedLine.Substring("real_session_v0_compare_mp_manifest_hash_mismatch_warning_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStateCurrentLine)) {
        $latestMpMismatchWarningStateCurrent = $compareMpMismatchWarningStateCurrentLine.Substring("real_session_v0_compare_mp_mismatch_warning_state_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStatePreviousLine)) {
        $latestMpMismatchWarningStatePrevious = $compareMpMismatchWarningStatePreviousLine.Substring("real_session_v0_compare_mp_mismatch_warning_state_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStateChangedLine)) {
        $latestMpMismatchWarningStateChanged = $compareMpMismatchWarningStateChangedLine.Substring("real_session_v0_compare_mp_mismatch_warning_state_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonCurrentLine)) {
        $latestMpMismatchWarningReasonCurrent = $compareMpMismatchWarningReasonCurrentLine.Substring("real_session_v0_compare_mp_mismatch_warning_reason_current=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonPreviousLine)) {
        $latestMpMismatchWarningReasonPrevious = $compareMpMismatchWarningReasonPreviousLine.Substring("real_session_v0_compare_mp_mismatch_warning_reason_previous=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonChangedLine)) {
        $latestMpMismatchWarningReasonChanged = $compareMpMismatchWarningReasonChangedLine.Substring("real_session_v0_compare_mp_mismatch_warning_reason_changed=".Length)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningCodesChangedLine)) {
        $latestMpMismatchWarningCodesChanged = $compareMpMismatchWarningCodesChangedLine.Substring("real_session_v0_compare_mp_mismatch_warning_codes_changed=".Length)
    }

    if ($latestIdentityRiskWarning -eq "true") {
        $latestTrendRecommendation = "review_identity_risk_warning"
    }
    elseif ($latestMpHandoffFollowUpActive -eq "true") {
        $latestTrendRecommendation = "review_mp_handoff_continuity"
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
    latest_post_play_package_campaign_identity_state_summary = [ordered]@{
        current = $latestPostPlayPackageCampaignIdentityStateSummaryCurrent
        previous = $latestPostPlayPackageCampaignIdentityStateSummaryPrevious
        changed = $latestPostPlayPackageCampaignIdentityStateSummaryChanged
    }
    latest_generated_overlay_publish_allowed = [ordered]@{
        current = $latestGeneratedOverlayPublishAllowedCurrent
        previous = $latestGeneratedOverlayPublishAllowedPrevious
        changed = $latestGeneratedOverlayPublishAllowedChanged
    }
    latest_generated_overlay_publish_gate = [ordered]@{
        state_current = $latestGeneratedOverlayPublishGateStateCurrent
        state_previous = $latestGeneratedOverlayPublishGateStatePrevious
        state_changed = $latestGeneratedOverlayPublishGateStateChanged
        reason_current = $latestGeneratedOverlayPublishGateReasonCurrent
        reason_previous = $latestGeneratedOverlayPublishGateReasonPrevious
        reason_changed = $latestGeneratedOverlayPublishGateReasonChanged
        can_publish_current = $latestGeneratedOverlayPublishGateCanPublishCurrent
        can_publish_previous = $latestGeneratedOverlayPublishGateCanPublishPrevious
        can_publish_changed = $latestGeneratedOverlayPublishGateCanPublishChanged
        publish_command_current = $latestGeneratedOverlayPublishGatePublishCommandCurrent
        publish_command_previous = $latestGeneratedOverlayPublishGatePublishCommandPrevious
        publish_command_changed = $latestGeneratedOverlayPublishGatePublishCommandChanged
    }
    latest_status_center = [ordered]@{
        state_current = $latestStatusCenterStateCurrent
        state_previous = $latestStatusCenterStatePrevious
        state_changed = $latestStatusCenterStateChanged
        reason_current = $latestStatusCenterReasonCurrent
        reason_previous = $latestStatusCenterReasonPrevious
        reason_changed = $latestStatusCenterReasonChanged
        summary_text_current = $latestStatusCenterSummaryTextCurrent
        summary_text_previous = $latestStatusCenterSummaryTextPrevious
        summary_text_changed = $latestStatusCenterSummaryTextChanged
    }
    latest_save_root = [ordered]@{
        resolution_current = $latestSaveRootResolutionCurrent
        resolution_previous = $latestSaveRootResolutionPrevious
        resolution_changed = $latestSaveRootResolutionChanged
        source_current = $latestSaveRootSourceCurrent
        source_previous = $latestSaveRootSourcePrevious
        source_changed = $latestSaveRootSourceChanged
        path_current = $latestSaveRootPathCurrent
        path_previous = $latestSaveRootPathPrevious
        path_changed = $latestSaveRootPathChanged
        campaign_count_current = $latestSaveRootCampaignCountCurrent
        campaign_count_previous = $latestSaveRootCampaignCountPrevious
        campaign_count_changed = $latestSaveRootCampaignCountChanged
        save_file_count_current = $latestSaveRootSaveFileCountCurrent
        save_file_count_previous = $latestSaveRootSaveFileCountPrevious
        save_file_count_changed = $latestSaveRootSaveFileCountChanged
        autosave_anchor_count_current = $latestSaveRootAutosaveAnchorCountCurrent
        autosave_anchor_count_previous = $latestSaveRootAutosaveAnchorCountPrevious
        autosave_anchor_count_changed = $latestSaveRootAutosaveAnchorCountChanged
    }
    latest_next_action = [ordered]@{
        action_current = $latestNextActionCurrent
        action_previous = $latestNextActionPrevious
        action_changed = $latestNextActionChanged
        reason_current = $latestNextActionReasonCurrent
        reason_previous = $latestNextActionReasonPrevious
        reason_changed = $latestNextActionReasonChanged
        command_hint_current = $latestNextActionCommandHintCurrent
        command_hint_previous = $latestNextActionCommandHintPrevious
        command_hint_changed = $latestNextActionCommandHintChanged
        command_hint_source_current = $latestNextActionCommandHintSourceCurrent
        command_hint_source_previous = $latestNextActionCommandHintSourcePrevious
        command_hint_source_changed = $latestNextActionCommandHintSourceChanged
        path_current = $latestNextActionPathCurrent
        path_previous = $latestNextActionPathPrevious
        path_changed = $latestNextActionPathChanged
    }
    latest_campaign_library = [ordered]@{
        plan_present_current = $latestCampaignLibraryPlanPresentCurrent
        plan_present_previous = $latestCampaignLibraryPlanPresentPrevious
        plan_present_changed = $latestCampaignLibraryPlanPresentChanged
        plan_path_current = $latestCampaignLibraryPlanPathCurrent
        plan_path_previous = $latestCampaignLibraryPlanPathPrevious
        plan_path_changed = $latestCampaignLibraryPlanPathChanged
        plan_source_current = $latestCampaignLibraryPlanSourceCurrent
        plan_source_previous = $latestCampaignLibraryPlanSourcePrevious
        plan_source_changed = $latestCampaignLibraryPlanSourceChanged
        plan_readiness_current = $latestCampaignLibraryPlanReadinessCurrent
        plan_readiness_previous = $latestCampaignLibraryPlanReadinessPrevious
        plan_readiness_changed = $latestCampaignLibraryPlanReadinessChanged
        plan_reason_current = $latestCampaignLibraryPlanReasonCurrent
        plan_reason_previous = $latestCampaignLibraryPlanReasonPrevious
        plan_reason_changed = $latestCampaignLibraryPlanReasonChanged
        limit_reached_current = $latestCampaignLibraryLimitReachedCurrent
        limit_reached_previous = $latestCampaignLibraryLimitReachedPrevious
        limit_reached_changed = $latestCampaignLibraryLimitReachedChanged
        skipped_due_to_limit_count_current = $latestCampaignLibrarySkippedDueToLimitCountCurrent
        skipped_due_to_limit_count_previous = $latestCampaignLibrarySkippedDueToLimitCountPrevious
        skipped_due_to_limit_count_changed = $latestCampaignLibrarySkippedDueToLimitCountChanged
        follow_up_active = $latestCampaignLibraryFollowUpActive
        follow_up_reason = $latestCampaignLibraryFollowUpReason
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
        package_zip_state_previous = $latestMpPackageZipStatePrevious
        package_zip_state_current = $latestMpPackageZipStateCurrent
        package_zip_state_changed = $latestMpPackageZipStateChanged
        package_zip_reason_previous = $latestMpPackageZipReasonPrevious
        package_zip_reason_current = $latestMpPackageZipReasonCurrent
        package_zip_reason_changed = $latestMpPackageZipReasonChanged
        package_zip_sha256_previous = $latestMpPackageZipSha256Previous
        package_zip_sha256_current = $latestMpPackageZipSha256Current
        package_zip_sha256_changed = $latestMpPackageZipSha256Changed
        package_zip_path_previous = $latestMpPackageZipPathPrevious
        package_zip_path_current = $latestMpPackageZipPathCurrent
        package_zip_path_changed = $latestMpPackageZipPathChanged
        package_zip_bytes_previous = $latestMpPackageZipBytesPrevious
        package_zip_bytes_current = $latestMpPackageZipBytesCurrent
        package_zip_bytes_changed = $latestMpPackageZipBytesChanged
    }
    latest_mp_provenance = [ordered]@{
        provenance_state_previous = $latestMpProvenanceStatePrevious
        provenance_state_current = $latestMpProvenanceStateCurrent
        provenance_state_changed = $latestMpProvenanceStateChanged
        source_quality_count_previous = $latestMpSourceQualityCountPrevious
        source_quality_count_current = $latestMpSourceQualityCountCurrent
        source_quality_count_changed = $latestMpSourceQualityCountChanged
        source_qualities_previous = $latestMpSourceQualitiesPrevious
        source_qualities_current = $latestMpSourceQualitiesCurrent
        source_qualities_changed = $latestMpSourceQualitiesChanged
        bootstrap_campaign_count_previous = $latestMpBootstrapCampaignCountPrevious
        bootstrap_campaign_count_current = $latestMpBootstrapCampaignCountCurrent
        bootstrap_campaign_count_changed = $latestMpBootstrapCampaignCountChanged
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
        handoff_status_previous = $latestMpHandoffStatusPrevious
        handoff_status_current = $latestMpHandoffStatusCurrent
        handoff_status_changed = $latestMpHandoffStatusChanged
        previous_host_available_previous = $latestMpPreviousHostAvailablePrevious
        previous_host_available_current = $latestMpPreviousHostAvailableCurrent
        previous_host_available_changed = $latestMpPreviousHostAvailableChanged
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
        game_version_mismatch_warning_previous = $latestMpGameVersionMismatchWarningPrevious
        game_version_mismatch_warning_current = $latestMpGameVersionMismatchWarningCurrent
        game_version_mismatch_warning_changed = $latestMpGameVersionMismatchWarningChanged
        campaign_id_mismatch_warning_previous = $latestMpCampaignIdMismatchWarningPrevious
        campaign_id_mismatch_warning_current = $latestMpCampaignIdMismatchWarningCurrent
        campaign_id_mismatch_warning_changed = $latestMpCampaignIdMismatchWarningChanged
        overlay_version_mismatch_warning_previous = $latestMpOverlayVersionMismatchWarningPrevious
        overlay_version_mismatch_warning_current = $latestMpOverlayVersionMismatchWarningCurrent
        overlay_version_mismatch_warning_changed = $latestMpOverlayVersionMismatchWarningChanged
        mod_version_mismatch_warning_previous = $latestMpModVersionMismatchWarningPrevious
        mod_version_mismatch_warning_current = $latestMpModVersionMismatchWarningCurrent
        mod_version_mismatch_warning_changed = $latestMpModVersionMismatchWarningChanged
        manifest_hash_mismatch_warning_previous = $latestMpManifestHashMismatchWarningPrevious
        manifest_hash_mismatch_warning_current = $latestMpManifestHashMismatchWarningCurrent
        manifest_hash_mismatch_warning_changed = $latestMpManifestHashMismatchWarningChanged
        mismatch_warning_state_previous = $latestMpMismatchWarningStatePrevious
        mismatch_warning_state_current = $latestMpMismatchWarningStateCurrent
        mismatch_warning_state_changed = $latestMpMismatchWarningStateChanged
        mismatch_warning_reason_previous = $latestMpMismatchWarningReasonPrevious
        mismatch_warning_reason_current = $latestMpMismatchWarningReasonCurrent
        mismatch_warning_reason_changed = $latestMpMismatchWarningReasonChanged
        mismatch_warning_codes_previous = $latestMpMismatchWarningCodesPrevious
        mismatch_warning_codes_current = $latestMpMismatchWarningCodesCurrent
        mismatch_warning_codes_changed = $latestMpMismatchWarningCodesChanged
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
Write-Host ("real_session_v0_trend_status_center_state_current=" + $latestStatusCenterStateCurrent)
Write-Host ("real_session_v0_trend_status_center_state_previous=" + $latestStatusCenterStatePrevious)
Write-Host ("real_session_v0_trend_status_center_state_changed=" + $latestStatusCenterStateChanged)
Write-Host ("real_session_v0_trend_status_center_reason_current=" + $latestStatusCenterReasonCurrent)
Write-Host ("real_session_v0_trend_status_center_reason_previous=" + $latestStatusCenterReasonPrevious)
Write-Host ("real_session_v0_trend_status_center_reason_changed=" + $latestStatusCenterReasonChanged)
Write-Host ("real_session_v0_trend_status_center_summary_text_current=" + $latestStatusCenterSummaryTextCurrent)
Write-Host ("real_session_v0_trend_status_center_summary_text_previous=" + $latestStatusCenterSummaryTextPrevious)
Write-Host ("real_session_v0_trend_status_center_summary_text_changed=" + $latestStatusCenterSummaryTextChanged)
Write-Host ("real_session_v0_trend_save_root_resolution_current=" + $latestSaveRootResolutionCurrent)
Write-Host ("real_session_v0_trend_save_root_resolution_previous=" + $latestSaveRootResolutionPrevious)
Write-Host ("real_session_v0_trend_save_root_resolution_changed=" + $latestSaveRootResolutionChanged)
Write-Host ("real_session_v0_trend_save_root_source_current=" + $latestSaveRootSourceCurrent)
Write-Host ("real_session_v0_trend_save_root_source_previous=" + $latestSaveRootSourcePrevious)
Write-Host ("real_session_v0_trend_save_root_source_changed=" + $latestSaveRootSourceChanged)
Write-Host ("real_session_v0_trend_save_root_path_current=" + $latestSaveRootPathCurrent)
Write-Host ("real_session_v0_trend_save_root_path_previous=" + $latestSaveRootPathPrevious)
Write-Host ("real_session_v0_trend_save_root_path_changed=" + $latestSaveRootPathChanged)
Write-Host ("real_session_v0_trend_save_root_campaign_count_current=" + $latestSaveRootCampaignCountCurrent)
Write-Host ("real_session_v0_trend_save_root_campaign_count_previous=" + $latestSaveRootCampaignCountPrevious)
Write-Host ("real_session_v0_trend_save_root_campaign_count_changed=" + $latestSaveRootCampaignCountChanged)
Write-Host ("real_session_v0_trend_save_root_save_file_count_current=" + $latestSaveRootSaveFileCountCurrent)
Write-Host ("real_session_v0_trend_save_root_save_file_count_previous=" + $latestSaveRootSaveFileCountPrevious)
Write-Host ("real_session_v0_trend_save_root_save_file_count_changed=" + $latestSaveRootSaveFileCountChanged)
Write-Host ("real_session_v0_trend_save_root_autosave_anchor_count_current=" + $latestSaveRootAutosaveAnchorCountCurrent)
Write-Host ("real_session_v0_trend_save_root_autosave_anchor_count_previous=" + $latestSaveRootAutosaveAnchorCountPrevious)
Write-Host ("real_session_v0_trend_save_root_autosave_anchor_count_changed=" + $latestSaveRootAutosaveAnchorCountChanged)
Write-Host ("real_session_v0_trend_next_action_current=" + $latestNextActionCurrent)
Write-Host ("real_session_v0_trend_next_action_previous=" + $latestNextActionPrevious)
Write-Host ("real_session_v0_trend_next_action_changed=" + $latestNextActionChanged)
Write-Host ("real_session_v0_trend_next_action_reason_current=" + $latestNextActionReasonCurrent)
Write-Host ("real_session_v0_trend_next_action_reason_previous=" + $latestNextActionReasonPrevious)
Write-Host ("real_session_v0_trend_next_action_reason_changed=" + $latestNextActionReasonChanged)
Write-Host ("real_session_v0_trend_next_action_command_hint_current=" + $latestNextActionCommandHintCurrent)
Write-Host ("real_session_v0_trend_next_action_command_hint_previous=" + $latestNextActionCommandHintPrevious)
Write-Host ("real_session_v0_trend_next_action_command_hint_changed=" + $latestNextActionCommandHintChanged)
Write-Host ("real_session_v0_trend_next_action_command_hint_source_current=" + $latestNextActionCommandHintSourceCurrent)
Write-Host ("real_session_v0_trend_next_action_command_hint_source_previous=" + $latestNextActionCommandHintSourcePrevious)
Write-Host ("real_session_v0_trend_next_action_command_hint_source_changed=" + $latestNextActionCommandHintSourceChanged)
Write-Host ("real_session_v0_trend_next_action_path_current=" + $latestNextActionPathCurrent)
Write-Host ("real_session_v0_trend_next_action_path_previous=" + $latestNextActionPathPrevious)
Write-Host ("real_session_v0_trend_next_action_path_changed=" + $latestNextActionPathChanged)
Write-Host ("real_session_v0_trend_entry_point_analysis_path_current=" + $latestEntryPointAnalysisPathCurrent)
Write-Host ("real_session_v0_trend_entry_point_analysis_path_previous=" + $latestEntryPointAnalysisPathPrevious)
Write-Host ("real_session_v0_trend_entry_point_analysis_path_changed=" + $latestEntryPointAnalysisPathChanged)
Write-Host ("real_session_v0_trend_entry_point_readiness_current=" + $latestEntryPointReadinessCurrent)
Write-Host ("real_session_v0_trend_entry_point_readiness_previous=" + $latestEntryPointReadinessPrevious)
Write-Host ("real_session_v0_trend_entry_point_readiness_changed=" + $latestEntryPointReadinessChanged)
Write-Host ("real_session_v0_trend_entry_point_reason_current=" + $latestEntryPointReasonCurrent)
Write-Host ("real_session_v0_trend_entry_point_reason_previous=" + $latestEntryPointReasonPrevious)
Write-Host ("real_session_v0_trend_entry_point_reason_changed=" + $latestEntryPointReasonChanged)
Write-Host ("real_session_v0_trend_entry_point_count_current=" + $latestEntryPointCountCurrent)
Write-Host ("real_session_v0_trend_entry_point_count_previous=" + $latestEntryPointCountPrevious)
Write-Host ("real_session_v0_trend_entry_point_count_changed=" + $latestEntryPointCountChanged)
Write-Host ("real_session_v0_trend_entry_point_branch_ambiguity_current=" + $latestEntryPointBranchAmbiguityCurrent)
Write-Host ("real_session_v0_trend_entry_point_branch_ambiguity_previous=" + $latestEntryPointBranchAmbiguityPrevious)
Write-Host ("real_session_v0_trend_entry_point_branch_ambiguity_changed=" + $latestEntryPointBranchAmbiguityChanged)
Write-Host ("real_session_v0_trend_post_play_package_campaign_identity_state_summary_current=" + $latestPostPlayPackageCampaignIdentityStateSummaryCurrent)
Write-Host ("real_session_v0_trend_post_play_package_campaign_identity_state_summary_previous=" + $latestPostPlayPackageCampaignIdentityStateSummaryPrevious)
Write-Host ("real_session_v0_trend_post_play_package_campaign_identity_state_summary_changed=" + $latestPostPlayPackageCampaignIdentityStateSummaryChanged)
Write-Host ("real_session_v0_trend_generated_overlay_publish_allowed_current=" + $latestGeneratedOverlayPublishAllowedCurrent)
Write-Host ("real_session_v0_trend_generated_overlay_publish_allowed_previous=" + $latestGeneratedOverlayPublishAllowedPrevious)
Write-Host ("real_session_v0_trend_generated_overlay_publish_allowed_changed=" + $latestGeneratedOverlayPublishAllowedChanged)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_state_current=" + $latestGeneratedOverlayPublishGateStateCurrent)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_state_previous=" + $latestGeneratedOverlayPublishGateStatePrevious)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_state_changed=" + $latestGeneratedOverlayPublishGateStateChanged)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_reason_current=" + $latestGeneratedOverlayPublishGateReasonCurrent)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_reason_previous=" + $latestGeneratedOverlayPublishGateReasonPrevious)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_reason_changed=" + $latestGeneratedOverlayPublishGateReasonChanged)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_can_publish_current=" + $latestGeneratedOverlayPublishGateCanPublishCurrent)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_can_publish_previous=" + $latestGeneratedOverlayPublishGateCanPublishPrevious)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_can_publish_changed=" + $latestGeneratedOverlayPublishGateCanPublishChanged)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_publish_command_current=" + $latestGeneratedOverlayPublishGatePublishCommandCurrent)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_publish_command_previous=" + $latestGeneratedOverlayPublishGatePublishCommandPrevious)
Write-Host ("real_session_v0_trend_generated_overlay_publish_gate_publish_command_changed=" + $latestGeneratedOverlayPublishGatePublishCommandChanged)
Write-Host ("real_session_v0_trend_memory_recovery_anchor_entry_point_id_current=" + $latestMemoryRecoveryAnchorEntryPointIdCurrent)
Write-Host ("real_session_v0_trend_memory_recovery_anchor_entry_point_id_previous=" + $latestMemoryRecoveryAnchorEntryPointIdPrevious)
Write-Host ("real_session_v0_trend_memory_recovery_anchor_entry_point_id_changed=" + $latestMemoryRecoveryAnchorEntryPointIdChanged)
Write-Host ("real_session_v0_trend_campaign_library_plan_present_current=" + $latestCampaignLibraryPlanPresentCurrent)
Write-Host ("real_session_v0_trend_campaign_library_plan_present_previous=" + $latestCampaignLibraryPlanPresentPrevious)
Write-Host ("real_session_v0_trend_campaign_library_plan_present_changed=" + $latestCampaignLibraryPlanPresentChanged)
Write-Host ("real_session_v0_trend_campaign_library_plan_path_current=" + $latestCampaignLibraryPlanPathCurrent)
Write-Host ("real_session_v0_trend_campaign_library_plan_path_previous=" + $latestCampaignLibraryPlanPathPrevious)
Write-Host ("real_session_v0_trend_campaign_library_plan_path_changed=" + $latestCampaignLibraryPlanPathChanged)
Write-Host ("real_session_v0_trend_campaign_library_plan_source_current=" + $latestCampaignLibraryPlanSourceCurrent)
Write-Host ("real_session_v0_trend_campaign_library_plan_source_previous=" + $latestCampaignLibraryPlanSourcePrevious)
Write-Host ("real_session_v0_trend_campaign_library_plan_source_changed=" + $latestCampaignLibraryPlanSourceChanged)
Write-Host ("real_session_v0_trend_campaign_library_plan_readiness_current=" + $latestCampaignLibraryPlanReadinessCurrent)
Write-Host ("real_session_v0_trend_campaign_library_plan_readiness_previous=" + $latestCampaignLibraryPlanReadinessPrevious)
Write-Host ("real_session_v0_trend_campaign_library_plan_readiness_changed=" + $latestCampaignLibraryPlanReadinessChanged)
Write-Host ("real_session_v0_trend_campaign_library_plan_reason_current=" + $latestCampaignLibraryPlanReasonCurrent)
Write-Host ("real_session_v0_trend_campaign_library_plan_reason_previous=" + $latestCampaignLibraryPlanReasonPrevious)
Write-Host ("real_session_v0_trend_campaign_library_plan_reason_changed=" + $latestCampaignLibraryPlanReasonChanged)
Write-Host ("real_session_v0_trend_campaign_library_limit_reached_current=" + $latestCampaignLibraryLimitReachedCurrent)
Write-Host ("real_session_v0_trend_campaign_library_limit_reached_previous=" + $latestCampaignLibraryLimitReachedPrevious)
Write-Host ("real_session_v0_trend_campaign_library_limit_reached_changed=" + $latestCampaignLibraryLimitReachedChanged)
Write-Host ("real_session_v0_trend_campaign_library_skipped_due_to_limit_count_current=" + $latestCampaignLibrarySkippedDueToLimitCountCurrent)
Write-Host ("real_session_v0_trend_campaign_library_skipped_due_to_limit_count_previous=" + $latestCampaignLibrarySkippedDueToLimitCountPrevious)
Write-Host ("real_session_v0_trend_campaign_library_skipped_due_to_limit_count_changed=" + $latestCampaignLibrarySkippedDueToLimitCountChanged)
Write-Host ("real_session_v0_trend_campaign_library_follow_up_active=" + $latestCampaignLibraryFollowUpActive)
Write-Host ("real_session_v0_trend_campaign_library_follow_up_reason=" + $latestCampaignLibraryFollowUpReason)
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
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipStateCurrent)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_state_current=" + $latestMpPackageZipStateCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipStatePrevious)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_state_previous=" + $latestMpPackageZipStatePrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipStateChanged)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_state_changed=" + $latestMpPackageZipStateChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipReasonCurrent)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_reason_current=" + $latestMpPackageZipReasonCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipReasonPrevious)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_reason_previous=" + $latestMpPackageZipReasonPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipReasonChanged)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_reason_changed=" + $latestMpPackageZipReasonChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipSha256Current)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_sha256_current=" + $latestMpPackageZipSha256Current)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipSha256Previous)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_sha256_previous=" + $latestMpPackageZipSha256Previous)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipSha256Changed)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_sha256_changed=" + $latestMpPackageZipSha256Changed)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipPathCurrent)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_path_current=" + $latestMpPackageZipPathCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipPathPrevious)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_path_previous=" + $latestMpPackageZipPathPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipPathChanged)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_path_changed=" + $latestMpPackageZipPathChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipBytesCurrent)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_bytes_current=" + $latestMpPackageZipBytesCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipBytesPrevious)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_bytes_previous=" + $latestMpPackageZipBytesPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpPackageZipBytesChanged)) {
    Write-Host ("real_session_v0_trend_mp_package_zip_bytes_changed=" + $latestMpPackageZipBytesChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpProvenanceStateCurrent)) {
    Write-Host ("real_session_v0_trend_mp_provenance_state_current=" + $latestMpProvenanceStateCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpProvenanceStatePrevious)) {
    Write-Host ("real_session_v0_trend_mp_provenance_state_previous=" + $latestMpProvenanceStatePrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpProvenanceStateChanged)) {
    Write-Host ("real_session_v0_trend_mp_provenance_state_changed=" + $latestMpProvenanceStateChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpSourceQualityCountCurrent)) {
    Write-Host ("real_session_v0_trend_mp_source_quality_count_current=" + $latestMpSourceQualityCountCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpSourceQualityCountPrevious)) {
    Write-Host ("real_session_v0_trend_mp_source_quality_count_previous=" + $latestMpSourceQualityCountPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpSourceQualityCountChanged)) {
    Write-Host ("real_session_v0_trend_mp_source_quality_count_changed=" + $latestMpSourceQualityCountChanged)
}
foreach ($sourceQuality in $latestMpSourceQualitiesPrevious) {
    Write-Host ("real_session_v0_trend_mp_source_quality_previous=" + $sourceQuality)
}
foreach ($sourceQuality in $latestMpSourceQualitiesCurrent) {
    Write-Host ("real_session_v0_trend_mp_source_quality_current=" + $sourceQuality)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpSourceQualitiesChanged)) {
    Write-Host ("real_session_v0_trend_mp_source_qualities_changed=" + $latestMpSourceQualitiesChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpBootstrapCampaignCountCurrent)) {
    Write-Host ("real_session_v0_trend_mp_bootstrap_campaign_count_current=" + $latestMpBootstrapCampaignCountCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpBootstrapCampaignCountPrevious)) {
    Write-Host ("real_session_v0_trend_mp_bootstrap_campaign_count_previous=" + $latestMpBootstrapCampaignCountPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpBootstrapCampaignCountChanged)) {
    Write-Host ("real_session_v0_trend_mp_bootstrap_campaign_count_changed=" + $latestMpBootstrapCampaignCountChanged)
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
Write-Host ("real_session_v0_trend_mp_handoff_status_current=" + $latestMpHandoffStatusCurrent)
Write-Host ("real_session_v0_trend_mp_handoff_status_previous=" + $latestMpHandoffStatusPrevious)
Write-Host ("real_session_v0_trend_mp_handoff_status_changed=" + $latestMpHandoffStatusChanged)
Write-Host ("real_session_v0_trend_mp_previous_host_available_current=" + $latestMpPreviousHostAvailableCurrent)
Write-Host ("real_session_v0_trend_mp_previous_host_available_previous=" + $latestMpPreviousHostAvailablePrevious)
Write-Host ("real_session_v0_trend_mp_previous_host_available_changed=" + $latestMpPreviousHostAvailableChanged)
Write-Host ("real_session_v0_trend_mp_handoff_follow_up_active=" + $latestMpHandoffFollowUpActive)
Write-Host ("real_session_v0_trend_mp_handoff_follow_up_reason=" + $latestMpHandoffFollowUpReason)
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
if (-not [string]::IsNullOrWhiteSpace($latestMpGameVersionMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_game_version_mismatch_warning_current=" + $latestMpGameVersionMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpGameVersionMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_game_version_mismatch_warning_previous=" + $latestMpGameVersionMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpGameVersionMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_game_version_mismatch_warning_changed=" + $latestMpGameVersionMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpCampaignIdMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_campaign_id_mismatch_warning_current=" + $latestMpCampaignIdMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpCampaignIdMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_campaign_id_mismatch_warning_previous=" + $latestMpCampaignIdMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpCampaignIdMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_campaign_id_mismatch_warning_changed=" + $latestMpCampaignIdMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpOverlayVersionMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_overlay_version_mismatch_warning_current=" + $latestMpOverlayVersionMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpOverlayVersionMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_overlay_version_mismatch_warning_previous=" + $latestMpOverlayVersionMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpOverlayVersionMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_overlay_version_mismatch_warning_changed=" + $latestMpOverlayVersionMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpModVersionMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_mod_version_mismatch_warning_current=" + $latestMpModVersionMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpModVersionMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_mod_version_mismatch_warning_previous=" + $latestMpModVersionMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpModVersionMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_mod_version_mismatch_warning_changed=" + $latestMpModVersionMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashMismatchWarningCurrent)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_mismatch_warning_current=" + $latestMpManifestHashMismatchWarningCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashMismatchWarningPrevious)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_mismatch_warning_previous=" + $latestMpManifestHashMismatchWarningPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpManifestHashMismatchWarningChanged)) {
    Write-Host ("real_session_v0_trend_mp_manifest_hash_mismatch_warning_changed=" + $latestMpManifestHashMismatchWarningChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningStateCurrent)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_state_current=" + $latestMpMismatchWarningStateCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningStatePrevious)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_state_previous=" + $latestMpMismatchWarningStatePrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningStateChanged)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_state_changed=" + $latestMpMismatchWarningStateChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningReasonCurrent)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_reason_current=" + $latestMpMismatchWarningReasonCurrent)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningReasonPrevious)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_reason_previous=" + $latestMpMismatchWarningReasonPrevious)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningReasonChanged)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_reason_changed=" + $latestMpMismatchWarningReasonChanged)
}
if (-not [string]::IsNullOrWhiteSpace($latestMpMismatchWarningCodesChanged)) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_codes_changed=" + $latestMpMismatchWarningCodesChanged)
}
foreach ($warningCode in $latestMpIdentityMismatchWarningCodesPrevious) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $latestMpIdentityMismatchWarningCodesCurrent) {
    Write-Host ("real_session_v0_trend_mp_identity_mismatch_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $latestMpMismatchWarningCodesPrevious) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $latestMpMismatchWarningCodesCurrent) {
    Write-Host ("real_session_v0_trend_mp_mismatch_warning_code_current=" + $warningCode)
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
