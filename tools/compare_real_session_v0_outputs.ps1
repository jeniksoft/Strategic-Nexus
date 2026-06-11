param(
    [Parameter(Mandatory = $true)]
    [string]$PreviousSessionDir,
    [Parameter(Mandatory = $true)]
    [string]$CurrentSessionDir,
    [string]$OutputJson
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-SessionIdFromDir {
    param([Parameter(Mandatory = $true)][string]$SessionDir)
    return Split-Path -Leaf ([System.IO.Path]::GetFullPath($SessionDir))
}

function Read-JsonFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required JSON file: $Path"
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

function Get-OptionalStringArray {
    param([Parameter(Mandatory = $true)]$Object, [Parameter(Mandatory = $true)][string]$Property)
    if ($null -eq $Object) { return @() }
    $value = $Object.PSObject.Properties[$Property]
    if ($null -eq $value) { return @() }
    if ($null -eq $value.Value) { return @() }

    $rawValue = $value.Value
    if ($rawValue -is [string]) {
        if ([string]::IsNullOrWhiteSpace($rawValue)) { return @() }
        return @([string]$rawValue)
    }

    $result = @()
    foreach ($item in $rawValue) {
        if ($null -eq $item) { continue }
        $text = [string]$item
        if ([string]::IsNullOrWhiteSpace($text)) { continue }
        $result += $text
    }
    return @($result)
}

function Parse-OptionalInt {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }
    $parsed = 0
    if ([int]::TryParse($Value, [ref]$parsed)) {
        return $parsed
    }
    return $null
}

function Contains-Value {
    param(
        [string[]]$Values,
        [string]$Needle
    )
    if ([string]::IsNullOrWhiteSpace($Needle)) {
        return $false
    }
    foreach ($value in $Values) {
        if ($value -eq $Needle) {
            return $true
        }
    }
    return $false
}

$previousSessionDirFull = [System.IO.Path]::GetFullPath($PreviousSessionDir)
$currentSessionDirFull = [System.IO.Path]::GetFullPath($CurrentSessionDir)
$previousSessionId = Get-SessionIdFromDir -SessionDir $previousSessionDirFull
$currentSessionId = Get-SessionIdFromDir -SessionDir $currentSessionDirFull
$compareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "' + $previousSessionDirFull + '" "' + $currentSessionDirFull + '" "dist\private_reports\real_session_v0_compare_' + $previousSessionId + '_to_' + $currentSessionId + '.json"'

$previousStatusPath = Join-Path $previousSessionDirFull "snc_status_snapshot.json"
$currentStatusPath = Join-Path $currentSessionDirFull "snc_status_snapshot.json"
$previousEvidencePath = Join-Path $previousSessionDirFull "real_session_v0_loop_evidence.json"
$currentEvidencePath = Join-Path $currentSessionDirFull "real_session_v0_loop_evidence.json"
$previousSummaryPath = Join-Path $previousSessionDirFull "work/archive_summary.json"
$currentSummaryPath = Join-Path $currentSessionDirFull "work/archive_summary.json"
$previousStatusWithMpPath = Join-Path $previousSessionDirFull "snc_status_snapshot_with_mp.json"
$currentStatusWithMpPath = Join-Path $currentSessionDirFull "snc_status_snapshot_with_mp.json"
$previousMpStatusSnapshotPresent = $false
$currentMpStatusSnapshotPresent = $false

$previousStatus = Read-JsonFile -Path $previousStatusPath
$currentStatus = Read-JsonFile -Path $currentStatusPath
$previousSummary = Read-JsonFile -Path $previousSummaryPath
$currentSummary = Read-JsonFile -Path $currentSummaryPath

$previousOverlayManifestHash = ""
$currentOverlayManifestHash = ""
if ($null -ne $previousStatus.generated_overlay_status) {
    $previousOverlayManifestHash = Get-OptionalString -Object $previousStatus.generated_overlay_status -Property "manifest_hash"
}
if ($null -ne $currentStatus.generated_overlay_status) {
    $currentOverlayManifestHash = Get-OptionalString -Object $currentStatus.generated_overlay_status -Property "manifest_hash"
}
$overlayChanged = ($previousOverlayManifestHash -ne $currentOverlayManifestHash)

$previousArchiveCount = 0
$currentArchiveCount = 0
if ($null -ne $previousSummary.copied_save_count) { $previousArchiveCount = [int]$previousSummary.copied_save_count }
if ($null -ne $currentSummary.copied_save_count) { $currentArchiveCount = [int]$currentSummary.copied_save_count }

$previousGameplayStatus = ""
$currentGameplayStatus = ""
$previousGameplayReason = ""
$currentGameplayReason = ""
$previousStatusCenterState = ""
$currentStatusCenterState = ""
$previousStatusCenterReason = ""
$currentStatusCenterReason = ""
$previousStatusCenterSummaryText = ""
$currentStatusCenterSummaryText = ""
$previousGeneratedOverlayPublishGateState = ""
$currentGeneratedOverlayPublishGateState = ""
$previousGeneratedOverlayPublishGateReason = ""
$currentGeneratedOverlayPublishGateReason = ""
$previousGeneratedOverlayPublishGateCanPublish = ""
$currentGeneratedOverlayPublishGateCanPublish = ""
$previousGeneratedOverlayPublishGatePublishCommand = ""
$currentGeneratedOverlayPublishGatePublishCommand = ""
if ($null -ne $previousStatus.gameplay_acceptance_status -and $null -ne $previousStatus.gameplay_acceptance_status.state) {
    $previousGameplayStatus = [string]$previousStatus.gameplay_acceptance_status.state
}
if ($null -ne $currentStatus.gameplay_acceptance_status -and $null -ne $currentStatus.gameplay_acceptance_status.state) {
    $currentGameplayStatus = [string]$currentStatus.gameplay_acceptance_status.state
}
if ($null -ne $previousStatus.gameplay_acceptance_status -and $null -ne $previousStatus.gameplay_acceptance_status.reason) {
    $previousGameplayReason = [string]$previousStatus.gameplay_acceptance_status.reason
}
if ($null -ne $currentStatus.gameplay_acceptance_status -and $null -ne $currentStatus.gameplay_acceptance_status.reason) {
    $currentGameplayReason = [string]$currentStatus.gameplay_acceptance_status.reason
}
if ($null -ne $previousStatus.status_center -and $null -ne $previousStatus.status_center.state) {
    $previousStatusCenterState = [string]$previousStatus.status_center.state
}
if ($null -ne $currentStatus.status_center -and $null -ne $currentStatus.status_center.state) {
    $currentStatusCenterState = [string]$currentStatus.status_center.state
}
if ($null -ne $previousStatus.status_center -and $null -ne $previousStatus.status_center.reason) {
    $previousStatusCenterReason = [string]$previousStatus.status_center.reason
}
if ($null -ne $currentStatus.status_center -and $null -ne $currentStatus.status_center.reason) {
    $currentStatusCenterReason = [string]$currentStatus.status_center.reason
}
if ($null -ne $previousStatus.status_center_summary_text) {
    $previousStatusCenterSummaryText = [string]$previousStatus.status_center_summary_text
}
if ($null -ne $currentStatus.status_center_summary_text) {
    $currentStatusCenterSummaryText = [string]$currentStatus.status_center_summary_text
}
if ($null -ne $previousStatus.generated_overlay_publish_gate_status) {
    $previousGeneratedOverlayPublishGateState = Get-OptionalString -Object $previousStatus.generated_overlay_publish_gate_status -Property "state"
    $previousGeneratedOverlayPublishGateReason = Get-OptionalString -Object $previousStatus.generated_overlay_publish_gate_status -Property "reason"
    $previousGeneratedOverlayPublishGateCanPublish = Get-OptionalString -Object $previousStatus.generated_overlay_publish_gate_status -Property "can_publish"
    $previousGeneratedOverlayPublishGatePublishCommand =
        Get-OptionalString -Object $previousStatus.generated_overlay_publish_gate_status -Property "publish_command"
}
if ($null -ne $currentStatus.generated_overlay_publish_gate_status) {
    $currentGeneratedOverlayPublishGateState = Get-OptionalString -Object $currentStatus.generated_overlay_publish_gate_status -Property "state"
    $currentGeneratedOverlayPublishGateReason = Get-OptionalString -Object $currentStatus.generated_overlay_publish_gate_status -Property "reason"
    $currentGeneratedOverlayPublishGateCanPublish = Get-OptionalString -Object $currentStatus.generated_overlay_publish_gate_status -Property "can_publish"
    $currentGeneratedOverlayPublishGatePublishCommand =
        Get-OptionalString -Object $currentStatus.generated_overlay_publish_gate_status -Property "publish_command"
}
$previousNextAction = ""
$currentNextAction = ""
$previousNextActionReason = ""
$currentNextActionReason = ""
$previousNextActionCommandHintSource = ""
$currentNextActionCommandHintSource = ""
$previousNextActionCommandHint = ""
$currentNextActionCommandHint = ""
$previousNextActionPath = ""
$currentNextActionPath = ""
$previousFriendTrustStoreState = ""
$currentFriendTrustStoreState = ""
$previousFriendTrustStoreReason = ""
$currentFriendTrustStoreReason = ""
$previousFriendTrustStorePath = ""
$currentFriendTrustStorePath = ""
$previousFriendTrustStoreControlsState = ""
$currentFriendTrustStoreControlsState = ""
$previousFriendTrustStoreControlsReason = ""
$currentFriendTrustStoreControlsReason = ""
$previousFriendTrustStoreControlsNextStep = ""
$currentFriendTrustStoreControlsNextStep = ""
$previousFriendMpSyncTransportAdapterState = ""
$currentFriendMpSyncTransportAdapterState = ""
$previousFriendMpSyncTransportAdapterReason = ""
$currentFriendMpSyncTransportAdapterReason = ""
$previousFriendMpSyncTransportAdapterNextStep = ""
$currentFriendMpSyncTransportAdapterNextStep = ""
$previousFriendMpSyncTransportAdapterKind = ""
$currentFriendMpSyncTransportAdapterKind = ""
$previousFriendMpSyncTransportAdapterPath = ""
$currentFriendMpSyncTransportAdapterPath = ""
$previousEntryPointAnalysisPath = ""
$currentEntryPointAnalysisPath = ""
$previousEntryPointReadiness = ""
$currentEntryPointReadiness = ""
$previousEntryPointReason = ""
$currentEntryPointReason = ""
$previousEntryPointCount = ""
$currentEntryPointCount = ""
$previousEntryPointBranchAmbiguity = ""
$currentEntryPointBranchAmbiguity = ""
$previousMemoryRecoveryAnchorEntryPointId = ""
$currentMemoryRecoveryAnchorEntryPointId = ""
$previousPostPlayPersonalityProfileApplied = "false"
$currentPostPlayPersonalityProfileApplied = "false"
$previousPostPlayPersonalityProfileSourceSchemaVersion = "0"
$currentPostPlayPersonalityProfileSourceSchemaVersion = "0"
$previousPostPlayPersonalityProfileSchemaCompatibilityState = ""
$currentPostPlayPersonalityProfileSchemaCompatibilityState = ""
$previousPostPlayPersonalityProfileSchemaCompatibilityNote = ""
$currentPostPlayPersonalityProfileSchemaCompatibilityNote = ""
$previousPostPlayPersonalityProfileValidatedUpdateSummary = ""
$currentPostPlayPersonalityProfileValidatedUpdateSummary = ""
$previousPostPlayPersonalityProfilePromptOutputNote = ""
$currentPostPlayPersonalityProfilePromptOutputNote = ""
$previousPostPlayPersonalityProfileSourceSaveDate = ""
$currentPostPlayPersonalityProfileSourceSaveDate = ""
$previousPostPlayPersonalityProfileZeroHistoryBootstrap = "false"
$currentPostPlayPersonalityProfileZeroHistoryBootstrap = "false"
$previousCampaignLibraryPlanPresent = "false"
$currentCampaignLibraryPlanPresent = "false"
$previousCampaignLibraryPlanPath = ""
$currentCampaignLibraryPlanPath = ""
$previousCampaignLibraryPlanSource = ""
$currentCampaignLibraryPlanSource = ""
$previousCampaignLibraryPlanReadiness = ""
$currentCampaignLibraryPlanReadiness = ""
$previousCampaignLibraryPlanReason = ""
$currentCampaignLibraryPlanReason = ""
$previousCampaignLibraryLimitReached = "false"
$currentCampaignLibraryLimitReached = "false"
$previousCampaignLibrarySkippedDueToLimitCount = ""
$currentCampaignLibrarySkippedDueToLimitCount = ""
$previousMpPackageZipState = ""
$currentMpPackageZipState = ""
$previousMpPackageZipReason = ""
$currentMpPackageZipReason = ""
$previousMpPackageZipSha256 = ""
$currentMpPackageZipSha256 = ""
$previousMpPackageZipPath = ""
$currentMpPackageZipPath = ""
$previousMpPackageZipBytes = ""
$currentMpPackageZipBytes = ""
$previousMpProvenanceState = "not_exported"
$currentMpProvenanceState = "not_exported"
$previousMpSourceQualityCount = "0"
$currentMpSourceQualityCount = "0"
$previousMpSourceQualities = @()
$currentMpSourceQualities = @()
$previousMpBootstrapCampaignCount = "0"
$currentMpBootstrapCampaignCount = "0"
$previousSaveRootResolution = ""
$currentSaveRootResolution = ""
$previousSaveRootSource = ""
$currentSaveRootSource = ""
$previousSaveRootPath = ""
$currentSaveRootPath = ""
$previousSaveRootCampaignCount = ""
$currentSaveRootCampaignCount = ""
$previousSaveRootSaveFileCount = ""
$currentSaveRootSaveFileCount = ""
$previousSaveRootAutosaveAnchorCount = ""
$currentSaveRootAutosaveAnchorCount = ""
if (Test-Path -LiteralPath $previousEvidencePath) {
    $previousEvidence = Read-JsonFile -Path $previousEvidencePath
    if ($null -ne $previousEvidence.save_root) {
        $previousSaveRootResolution = Get-OptionalString -Object $previousEvidence.save_root -Property "resolution"
        $previousSaveRootSource = Get-OptionalString -Object $previousEvidence.save_root -Property "source"
        $previousSaveRootPath = Get-OptionalString -Object $previousEvidence.save_root -Property "path"
        $previousSaveRootCampaignCount = Get-OptionalString -Object $previousEvidence.save_root -Property "campaign_count"
        $previousSaveRootSaveFileCount = Get-OptionalString -Object $previousEvidence.save_root -Property "save_file_count"
        $previousSaveRootAutosaveAnchorCount = Get-OptionalString -Object $previousEvidence.save_root -Property "autosave_anchor_count"
    }
    if ($null -ne $previousEvidence.next_action) {
        $previousNextAction = Get-OptionalString -Object $previousEvidence.next_action -Property "action"
        $previousNextActionReason = Get-OptionalString -Object $previousEvidence.next_action -Property "reason"
        $previousNextActionCommandHintSource = Get-OptionalString -Object $previousEvidence.next_action -Property "command_hint_source"
        $previousNextActionCommandHint = Get-OptionalString -Object $previousEvidence.next_action -Property "command_hint"
        $previousNextActionPath = Get-OptionalString -Object $previousEvidence.next_action -Property "path"
    }
    if ($null -ne $previousEvidence.friend_trust_store) {
        $previousFriendTrustStoreState = Get-OptionalString -Object $previousEvidence.friend_trust_store -Property "state"
        $previousFriendTrustStoreReason = Get-OptionalString -Object $previousEvidence.friend_trust_store -Property "reason"
        $previousFriendTrustStorePath = Get-OptionalString -Object $previousEvidence.friend_trust_store -Property "path"
    }
    if ($null -ne $previousEvidence.friend_trust_store_controls) {
        $previousFriendTrustStoreControlsState = Get-OptionalString -Object $previousEvidence.friend_trust_store_controls -Property "state"
        $previousFriendTrustStoreControlsReason = Get-OptionalString -Object $previousEvidence.friend_trust_store_controls -Property "reason"
        $previousFriendTrustStoreControlsNextStep = Get-OptionalString -Object $previousEvidence.friend_trust_store_controls -Property "next_step"
    }
    if ($null -ne $previousEvidence.friend_mesh_update) {
        $previousFriendMeshUpdateState = Get-OptionalString -Object $previousEvidence.friend_mesh_update -Property "state"
        $previousFriendMeshUpdateReason = Get-OptionalString -Object $previousEvidence.friend_mesh_update -Property "reason"
        $previousFriendMeshUpdateNextStep = Get-OptionalString -Object $previousEvidence.friend_mesh_update -Property "next_step"
    }
    if ($null -ne $previousEvidence.friend_mp_sync_transport_adapter) {
        $previousFriendMpSyncTransportAdapterState = Get-OptionalString -Object $previousEvidence.friend_mp_sync_transport_adapter -Property "state"
        $previousFriendMpSyncTransportAdapterReason = Get-OptionalString -Object $previousEvidence.friend_mp_sync_transport_adapter -Property "reason"
        $previousFriendMpSyncTransportAdapterNextStep = Get-OptionalString -Object $previousEvidence.friend_mp_sync_transport_adapter -Property "next_step"
        $previousFriendMpSyncTransportAdapterKind = Get-OptionalString -Object $previousEvidence.friend_mp_sync_transport_adapter -Property "kind"
        $previousFriendMpSyncTransportAdapterPath = Get-OptionalString -Object $previousEvidence.friend_mp_sync_transport_adapter -Property "path"
    }
    $previousEntryPointAnalysisPath = Get-OptionalString -Object $previousEvidence -Property "entry_point_analysis_path"
    $previousEntryPointReadiness = Get-OptionalString -Object $previousEvidence -Property "entry_point_readiness"
    $previousEntryPointReason = Get-OptionalString -Object $previousEvidence -Property "entry_point_reason"
    $previousEntryPointCount = Get-OptionalString -Object $previousEvidence -Property "entry_point_count"
    $previousEntryPointBranchAmbiguity = Get-OptionalString -Object $previousEvidence -Property "entry_point_branch_ambiguity"
    $previousMemoryRecoveryAnchorEntryPointId = Get-OptionalString -Object $previousEvidence -Property "memory_recovery_anchor_entry_point_id"
    $previousPostPlayPackageCampaignIdentityStateSummary = ""
    if ($null -ne $previousEvidence.entry_point_post_play) {
        $previousPostPlayPackageCampaignIdentityStateSummary = Get-OptionalString -Object $previousEvidence.entry_point_post_play -Property "post_play_package_campaign_identity_state_summary"
        if ($null -ne $previousEvidence.entry_point_post_play.personality_profile) {
            $previousPostPlayPersonalityProfileApplied = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "applied"
            if ([string]::IsNullOrWhiteSpace($previousPostPlayPersonalityProfileApplied)) { $previousPostPlayPersonalityProfileApplied = "false" }
            $previousPostPlayPersonalityProfileSourceSchemaVersion = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "source_schema_version"
            if ([string]::IsNullOrWhiteSpace($previousPostPlayPersonalityProfileSourceSchemaVersion)) { $previousPostPlayPersonalityProfileSourceSchemaVersion = "0" }
            $previousPostPlayPersonalityProfileSchemaCompatibilityState = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "schema_compatibility_state"
            $previousPostPlayPersonalityProfileSchemaCompatibilityNote = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "schema_compatibility_note"
            $previousPostPlayPersonalityProfileValidatedUpdateSummary = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "validated_update_summary"
            $previousPostPlayPersonalityProfilePromptOutputNote = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "prompt_output_note"
            $previousPostPlayPersonalityProfileSourceSaveDate = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "source_save_date"
            $previousPostPlayPersonalityProfileZeroHistoryBootstrap = Get-OptionalString -Object $previousEvidence.entry_point_post_play.personality_profile -Property "zero_history_bootstrap"
            if ([string]::IsNullOrWhiteSpace($previousPostPlayPersonalityProfileZeroHistoryBootstrap)) { $previousPostPlayPersonalityProfileZeroHistoryBootstrap = "false" }
        }
    }
    $previousGeneratedOverlayPublishAllowed = Get-OptionalString -Object $previousEvidence -Property "generated_overlay_publish_allowed"
    if ($null -ne $previousEvidence.campaign_library) {
        $previousCampaignLibraryPlanPresent = Get-OptionalString -Object $previousEvidence.campaign_library -Property "plan_present"
        $previousCampaignLibraryPlanPath = Get-OptionalString -Object $previousEvidence.campaign_library -Property "plan_path"
        $previousCampaignLibraryPlanSource = Get-OptionalString -Object $previousEvidence.campaign_library -Property "plan_source"
        $previousCampaignLibraryPlanReadiness = Get-OptionalString -Object $previousEvidence.campaign_library -Property "plan_readiness"
        $previousCampaignLibraryPlanReason = Get-OptionalString -Object $previousEvidence.campaign_library -Property "plan_reason"
        $previousCampaignLibraryLimitReached = Get-OptionalString -Object $previousEvidence.campaign_library -Property "limit_reached"
        $previousCampaignLibrarySkippedDueToLimitCount = Get-OptionalString -Object $previousEvidence.campaign_library -Property "skipped_due_to_limit_count"
    }
    if ($null -ne $previousEvidence.mp_export) {
        $previousMpProvenanceState = Get-OptionalString -Object $previousEvidence.mp_export -Property "provenance_state"
        if ([string]::IsNullOrWhiteSpace($previousMpProvenanceState)) { $previousMpProvenanceState = "not_exported" }
        $previousMpSourceQualityCount = Get-OptionalString -Object $previousEvidence.mp_export -Property "source_quality_count"
        if ([string]::IsNullOrWhiteSpace($previousMpSourceQualityCount)) { $previousMpSourceQualityCount = "0" }
        $previousMpSourceQualities = Get-OptionalStringArray -Object $previousEvidence.mp_export -Property "source_qualities"
        $previousMpBootstrapCampaignCount = Get-OptionalString -Object $previousEvidence.mp_export -Property "bootstrap_campaign_count"
        if ([string]::IsNullOrWhiteSpace($previousMpBootstrapCampaignCount)) { $previousMpBootstrapCampaignCount = "0" }
        $previousMpPackageZipState = Get-OptionalString -Object $previousEvidence.mp_export -Property "package_zip_state"
        $previousMpPackageZipReason = Get-OptionalString -Object $previousEvidence.mp_export -Property "package_zip_reason"
        $previousMpPackageZipSha256 = Get-OptionalString -Object $previousEvidence.mp_export -Property "package_zip_sha256"
        $previousMpPackageZipPath = Get-OptionalString -Object $previousEvidence.mp_export -Property "package_zip_path"
        $previousMpPackageZipBytes = Get-OptionalString -Object $previousEvidence.mp_export -Property "package_zip_bytes"
    }
}
if (Test-Path -LiteralPath $currentEvidencePath) {
    $currentEvidence = Read-JsonFile -Path $currentEvidencePath
    if ($null -ne $currentEvidence.save_root) {
        $currentSaveRootResolution = Get-OptionalString -Object $currentEvidence.save_root -Property "resolution"
        $currentSaveRootSource = Get-OptionalString -Object $currentEvidence.save_root -Property "source"
        $currentSaveRootPath = Get-OptionalString -Object $currentEvidence.save_root -Property "path"
        $currentSaveRootCampaignCount = Get-OptionalString -Object $currentEvidence.save_root -Property "campaign_count"
        $currentSaveRootSaveFileCount = Get-OptionalString -Object $currentEvidence.save_root -Property "save_file_count"
        $currentSaveRootAutosaveAnchorCount = Get-OptionalString -Object $currentEvidence.save_root -Property "autosave_anchor_count"
    }
    if ($null -ne $currentEvidence.next_action) {
        $currentNextAction = Get-OptionalString -Object $currentEvidence.next_action -Property "action"
        $currentNextActionReason = Get-OptionalString -Object $currentEvidence.next_action -Property "reason"
        $currentNextActionCommandHintSource = Get-OptionalString -Object $currentEvidence.next_action -Property "command_hint_source"
        $currentNextActionCommandHint = Get-OptionalString -Object $currentEvidence.next_action -Property "command_hint"
        $currentNextActionPath = Get-OptionalString -Object $currentEvidence.next_action -Property "path"
    }
    if ($null -ne $currentEvidence.friend_trust_store) {
        $currentFriendTrustStoreState = Get-OptionalString -Object $currentEvidence.friend_trust_store -Property "state"
        $currentFriendTrustStoreReason = Get-OptionalString -Object $currentEvidence.friend_trust_store -Property "reason"
        $currentFriendTrustStorePath = Get-OptionalString -Object $currentEvidence.friend_trust_store -Property "path"
    }
    if ($null -ne $currentEvidence.friend_trust_store_controls) {
        $currentFriendTrustStoreControlsState = Get-OptionalString -Object $currentEvidence.friend_trust_store_controls -Property "state"
        $currentFriendTrustStoreControlsReason = Get-OptionalString -Object $currentEvidence.friend_trust_store_controls -Property "reason"
        $currentFriendTrustStoreControlsNextStep = Get-OptionalString -Object $currentEvidence.friend_trust_store_controls -Property "next_step"
    }
    if ($null -ne $currentEvidence.friend_mesh_update) {
        $currentFriendMeshUpdateState = Get-OptionalString -Object $currentEvidence.friend_mesh_update -Property "state"
        $currentFriendMeshUpdateReason = Get-OptionalString -Object $currentEvidence.friend_mesh_update -Property "reason"
        $currentFriendMeshUpdateNextStep = Get-OptionalString -Object $currentEvidence.friend_mesh_update -Property "next_step"
    }
    if ($null -ne $currentEvidence.friend_mp_sync_transport_adapter) {
        $currentFriendMpSyncTransportAdapterState = Get-OptionalString -Object $currentEvidence.friend_mp_sync_transport_adapter -Property "state"
        $currentFriendMpSyncTransportAdapterReason = Get-OptionalString -Object $currentEvidence.friend_mp_sync_transport_adapter -Property "reason"
        $currentFriendMpSyncTransportAdapterNextStep = Get-OptionalString -Object $currentEvidence.friend_mp_sync_transport_adapter -Property "next_step"
        $currentFriendMpSyncTransportAdapterKind = Get-OptionalString -Object $currentEvidence.friend_mp_sync_transport_adapter -Property "kind"
        $currentFriendMpSyncTransportAdapterPath = Get-OptionalString -Object $currentEvidence.friend_mp_sync_transport_adapter -Property "path"
    }
    $currentEntryPointAnalysisPath = Get-OptionalString -Object $currentEvidence -Property "entry_point_analysis_path"
    $currentEntryPointReadiness = Get-OptionalString -Object $currentEvidence -Property "entry_point_readiness"
    $currentEntryPointReason = Get-OptionalString -Object $currentEvidence -Property "entry_point_reason"
    $currentEntryPointCount = Get-OptionalString -Object $currentEvidence -Property "entry_point_count"
    $currentEntryPointBranchAmbiguity = Get-OptionalString -Object $currentEvidence -Property "entry_point_branch_ambiguity"
    $currentMemoryRecoveryAnchorEntryPointId = Get-OptionalString -Object $currentEvidence -Property "memory_recovery_anchor_entry_point_id"
    $currentPostPlayPackageCampaignIdentityStateSummary = ""
    if ($null -ne $currentEvidence.entry_point_post_play) {
        $currentPostPlayPackageCampaignIdentityStateSummary = Get-OptionalString -Object $currentEvidence.entry_point_post_play -Property "post_play_package_campaign_identity_state_summary"
        if ($null -ne $currentEvidence.entry_point_post_play.personality_profile) {
            $currentPostPlayPersonalityProfileApplied = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "applied"
            if ([string]::IsNullOrWhiteSpace($currentPostPlayPersonalityProfileApplied)) { $currentPostPlayPersonalityProfileApplied = "false" }
            $currentPostPlayPersonalityProfileSourceSchemaVersion = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "source_schema_version"
            if ([string]::IsNullOrWhiteSpace($currentPostPlayPersonalityProfileSourceSchemaVersion)) { $currentPostPlayPersonalityProfileSourceSchemaVersion = "0" }
            $currentPostPlayPersonalityProfileSchemaCompatibilityState = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "schema_compatibility_state"
            $currentPostPlayPersonalityProfileSchemaCompatibilityNote = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "schema_compatibility_note"
            $currentPostPlayPersonalityProfileValidatedUpdateSummary = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "validated_update_summary"
            $currentPostPlayPersonalityProfilePromptOutputNote = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "prompt_output_note"
            $currentPostPlayPersonalityProfileSourceSaveDate = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "source_save_date"
            $currentPostPlayPersonalityProfileZeroHistoryBootstrap = Get-OptionalString -Object $currentEvidence.entry_point_post_play.personality_profile -Property "zero_history_bootstrap"
            if ([string]::IsNullOrWhiteSpace($currentPostPlayPersonalityProfileZeroHistoryBootstrap)) { $currentPostPlayPersonalityProfileZeroHistoryBootstrap = "false" }
        }
    }
    $currentGeneratedOverlayPublishAllowed = Get-OptionalString -Object $currentEvidence -Property "generated_overlay_publish_allowed"
    if ($null -ne $currentEvidence.campaign_library) {
        $currentCampaignLibraryPlanPresent = Get-OptionalString -Object $currentEvidence.campaign_library -Property "plan_present"
        $currentCampaignLibraryPlanPath = Get-OptionalString -Object $currentEvidence.campaign_library -Property "plan_path"
        $currentCampaignLibraryPlanSource = Get-OptionalString -Object $currentEvidence.campaign_library -Property "plan_source"
        $currentCampaignLibraryPlanReadiness = Get-OptionalString -Object $currentEvidence.campaign_library -Property "plan_readiness"
        $currentCampaignLibraryPlanReason = Get-OptionalString -Object $currentEvidence.campaign_library -Property "plan_reason"
        $currentCampaignLibraryLimitReached = Get-OptionalString -Object $currentEvidence.campaign_library -Property "limit_reached"
        $currentCampaignLibrarySkippedDueToLimitCount = Get-OptionalString -Object $currentEvidence.campaign_library -Property "skipped_due_to_limit_count"
    }
    if ($null -ne $currentEvidence.mp_export) {
        $currentMpProvenanceState = Get-OptionalString -Object $currentEvidence.mp_export -Property "provenance_state"
        if ([string]::IsNullOrWhiteSpace($currentMpProvenanceState)) { $currentMpProvenanceState = "not_exported" }
        $currentMpSourceQualityCount = Get-OptionalString -Object $currentEvidence.mp_export -Property "source_quality_count"
        if ([string]::IsNullOrWhiteSpace($currentMpSourceQualityCount)) { $currentMpSourceQualityCount = "0" }
        $currentMpSourceQualities = Get-OptionalStringArray -Object $currentEvidence.mp_export -Property "source_qualities"
        $currentMpBootstrapCampaignCount = Get-OptionalString -Object $currentEvidence.mp_export -Property "bootstrap_campaign_count"
        if ([string]::IsNullOrWhiteSpace($currentMpBootstrapCampaignCount)) { $currentMpBootstrapCampaignCount = "0" }
        $currentMpPackageZipState = Get-OptionalString -Object $currentEvidence.mp_export -Property "package_zip_state"
        $currentMpPackageZipReason = Get-OptionalString -Object $currentEvidence.mp_export -Property "package_zip_reason"
        $currentMpPackageZipSha256 = Get-OptionalString -Object $currentEvidence.mp_export -Property "package_zip_sha256"
        $currentMpPackageZipPath = Get-OptionalString -Object $currentEvidence.mp_export -Property "package_zip_path"
        $currentMpPackageZipBytes = Get-OptionalString -Object $currentEvidence.mp_export -Property "package_zip_bytes"
    }
}

$previousMpManifestHash = ""
$currentMpManifestHash = ""
$previousMpPackageOutputDir = ""
$currentMpPackageOutputDir = ""
$previousMpReadiness = ""
$currentMpReadiness = ""
$previousMpWarningCount = ""
$currentMpWarningCount = ""
$previousMpHostReadiness = ""
$currentMpHostReadiness = ""
$previousMpClientReadinessGate = ""
$currentMpClientReadinessGate = ""
$previousMpHostNextStep = ""
$currentMpHostNextStep = ""
$previousMpClientNextStep = ""
$currentMpClientNextStep = ""
$previousMpVerifyCommand = ""
$currentMpVerifyCommand = ""
$previousMpImportCommand = ""
$currentMpImportCommand = ""
$previousMpStrictVerifyCommand = ""
$currentMpStrictVerifyCommand = ""
$previousMpStrictImportCommand = ""
$currentMpStrictImportCommand = ""
$previousMpHandoffStatus = ""
$currentMpHandoffStatus = ""
$previousMpPreviousHostAvailable = ""
$currentMpPreviousHostAvailable = ""
$previousMpIdentityMismatchWarning = ""
$currentMpIdentityMismatchWarning = ""
$previousMpMismatchWarningState = ""
$currentMpMismatchWarningState = ""
$previousMpMismatchWarningReason = ""
$currentMpMismatchWarningReason = ""
$previousMpWarningCodes = @()
$currentMpWarningCodes = @()
$previousMpIdentityMismatchWarningCodes = @()
$currentMpIdentityMismatchWarningCodes = @()
$previousMpMismatchWarningCodes = @()
$currentMpMismatchWarningCodes = @()
if (Test-Path -LiteralPath $previousStatusWithMpPath) {
    $previousMpStatusSnapshotPresent = $true
    $previousStatusWithMp = Read-JsonFile -Path $previousStatusWithMpPath
    $previousMpPackageOutputDirCandidate = Join-Path $previousSessionDirFull "mp_overlay_package"
    if (Test-Path -LiteralPath $previousMpPackageOutputDirCandidate) {
        $previousMpPackageOutputDir = [System.IO.Path]::GetFullPath($previousMpPackageOutputDirCandidate)
    }
    if ($null -ne $previousStatusWithMp.mp_overlay_package_status) {
        $previousMpManifestHash = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "package_manifest_hash"
        $previousMpReadiness = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "readiness"
        $previousMpWarningCount = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "warning_count"
        $previousMpHostReadiness = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "host_readiness"
        $previousMpClientReadinessGate = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "client_readiness_gate"
        $previousMpHostNextStep = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "host_next_step"
        $previousMpClientNextStep = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "client_next_step"
        $previousMpVerifyCommand = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "verify_command"
        $previousMpImportCommand = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "import_command"
        $previousMpStrictVerifyCommand = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "strict_verify_command"
        $previousMpStrictImportCommand = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "strict_import_command"
        $previousMpHandoffStatus = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "handoff_status"
        $previousMpWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $previousMpIdentityMismatchWarning = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $previousMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
        $previousMpMismatchWarningState = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_state"
        $previousMpMismatchWarningReason = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_reason"
        $previousMpMismatchWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_codes"
        if ($previousMpHandoffStatus -eq "complete") {
            $previousMpPreviousHostAvailable = "true"
        }
        elseif ($previousMpHandoffStatus -eq "degraded_previous_host_unavailable") {
            $previousMpPreviousHostAvailable = "false"
        }
    }
}
if (Test-Path -LiteralPath $currentStatusWithMpPath) {
    $currentMpStatusSnapshotPresent = $true
    $currentStatusWithMp = Read-JsonFile -Path $currentStatusWithMpPath
    $currentMpPackageOutputDirCandidate = Join-Path $currentSessionDirFull "mp_overlay_package"
    if (Test-Path -LiteralPath $currentMpPackageOutputDirCandidate) {
        $currentMpPackageOutputDir = [System.IO.Path]::GetFullPath($currentMpPackageOutputDirCandidate)
    }
    if ($null -ne $currentStatusWithMp.mp_overlay_package_status) {
        $currentMpManifestHash = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "package_manifest_hash"
        $currentMpReadiness = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "readiness"
        $currentMpWarningCount = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "warning_count"
        $currentMpHostReadiness = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "host_readiness"
        $currentMpClientReadinessGate = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "client_readiness_gate"
        $currentMpHostNextStep = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "host_next_step"
        $currentMpClientNextStep = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "client_next_step"
        $currentMpVerifyCommand = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "verify_command"
        $currentMpImportCommand = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "import_command"
        $currentMpStrictVerifyCommand = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "strict_verify_command"
        $currentMpStrictImportCommand = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "strict_import_command"
        $currentMpHandoffStatus = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "handoff_status"
        $currentMpWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $currentMpIdentityMismatchWarning = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $currentMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
        $currentMpMismatchWarningState = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_state"
        $currentMpMismatchWarningReason = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_reason"
        $currentMpMismatchWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "mismatch_warning_codes"
        if ($currentMpHandoffStatus -eq "complete") {
            $currentMpPreviousHostAvailable = "true"
        }
        elseif ($currentMpHandoffStatus -eq "degraded_previous_host_unavailable") {
            $currentMpPreviousHostAvailable = "false"
        }
    }
}
$previousMpWarningCodes = @($previousMpWarningCodes | Sort-Object -Unique)
$currentMpWarningCodes = @($currentMpWarningCodes | Sort-Object -Unique)
$previousMpWarningCodesJoined = ($previousMpWarningCodes -join "|")
$currentMpWarningCodesJoined = ($currentMpWarningCodes -join "|")
$mpWarningCodesChanged = ($previousMpWarningCodesJoined -ne $currentMpWarningCodesJoined)
$previousMpIdentityMismatchWarningCodes = @($previousMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$currentMpIdentityMismatchWarningCodes = @($currentMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$previousMpMismatchWarningCodes = @($previousMpMismatchWarningCodes | Sort-Object -Unique)
$currentMpMismatchWarningCodes = @($currentMpMismatchWarningCodes | Sort-Object -Unique)
$previousMpAllMismatchCodes = @($previousMpWarningCodes + $previousMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$currentMpAllMismatchCodes = @($currentMpWarningCodes + $currentMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$previousMpIdentityMismatchWarningCodesJoined = ($previousMpIdentityMismatchWarningCodes -join "|")
$currentMpIdentityMismatchWarningCodesJoined = ($currentMpIdentityMismatchWarningCodes -join "|")
$mpIdentityMismatchWarningChanged = ($previousMpIdentityMismatchWarning -ne $currentMpIdentityMismatchWarning)
$mpIdentityMismatchWarningCodesChanged = ($previousMpIdentityMismatchWarningCodesJoined -ne $currentMpIdentityMismatchWarningCodesJoined)
$previousMpVersionMismatchWarning = (Contains-Value -Values $previousMpAllMismatchCodes -Needle "package_game_version_mismatch")
$currentMpVersionMismatchWarning = (Contains-Value -Values $currentMpAllMismatchCodes -Needle "package_game_version_mismatch")
$previousMpCampaignIdMismatchWarning = (Contains-Value -Values $previousMpAllMismatchCodes -Needle "package_campaign_id_mismatch")
$currentMpCampaignIdMismatchWarning = (Contains-Value -Values $currentMpAllMismatchCodes -Needle "package_campaign_id_mismatch")
$previousMpOverlayVersionMismatchWarning = (Contains-Value -Values $previousMpAllMismatchCodes -Needle "package_overlay_version_mismatch")
$currentMpOverlayVersionMismatchWarning = (Contains-Value -Values $currentMpAllMismatchCodes -Needle "package_overlay_version_mismatch")
$previousMpModVersionMismatchWarning = (Contains-Value -Values $previousMpAllMismatchCodes -Needle "package_mod_version_mismatch")
$currentMpModVersionMismatchWarning = (Contains-Value -Values $currentMpAllMismatchCodes -Needle "package_mod_version_mismatch")
$previousMpManifestHashMismatchWarning = (Contains-Value -Values $previousMpAllMismatchCodes -Needle "package_manifest_hash_mismatch")
$currentMpManifestHashMismatchWarning = (Contains-Value -Values $currentMpAllMismatchCodes -Needle "package_manifest_hash_mismatch")
$previousMpWarningCountParsed = Parse-OptionalInt -Value $previousMpWarningCount
$currentMpWarningCountParsed = Parse-OptionalInt -Value $currentMpWarningCount
$mpWarningCountDelta = $null
if ($null -ne $previousMpWarningCountParsed -and $null -ne $currentMpWarningCountParsed) {
    $mpWarningCountDelta = ($currentMpWarningCountParsed - $previousMpWarningCountParsed)
}
$identityRiskWarning = $false
$identityRiskWarningReason = "none"
if ($currentMpIdentityMismatchWarning -eq "true") {
    $identityRiskWarning = $true
    $identityRiskWarningReason = "current_session_identity_or_version_mismatch"
}
elseif ($previousMpIdentityMismatchWarning -eq "true") {
    $identityRiskWarning = $true
    $identityRiskWarningReason = "previous_session_identity_or_version_mismatch"
}
elseif ($previousMpIdentityMismatchWarningCodesJoined -ne $currentMpIdentityMismatchWarningCodesJoined -and `
        (-not [string]::IsNullOrWhiteSpace($previousMpIdentityMismatchWarningCodesJoined) -or `
         -not [string]::IsNullOrWhiteSpace($currentMpIdentityMismatchWarningCodesJoined))) {
    $identityRiskWarning = $true
    $identityRiskWarningReason = "identity_mismatch_warning_codes_changed_between_sessions"
}

$recommendation = "review_observable_deltas"
if (-not $overlayChanged -and $previousArchiveCount -eq $currentArchiveCount -and $previousGameplayStatus -eq $currentGameplayStatus -and $previousGameplayReason -eq $currentGameplayReason -and $previousMpManifestHash -eq $currentMpManifestHash) {
    $recommendation = "no_pipeline_delta_detected"
}
if ($identityRiskWarning) {
    $recommendation = "review_identity_risk_warning"
}
$mpHandoffFollowUpActive = $false
$mpHandoffFollowUpReason = "none"
if ($currentMpPreviousHostAvailable -eq "false") {
    $mpHandoffFollowUpActive = $true
    $mpHandoffFollowUpReason = "current_previous_host_unavailable"
}
elseif (-not [string]::IsNullOrWhiteSpace($currentMpHandoffStatus) -and $currentMpHandoffStatus -ne "complete") {
    $mpHandoffFollowUpActive = $true
    $mpHandoffFollowUpReason = "current_handoff_status_not_complete"
}
elseif ($previousMpHandoffStatus -ne $currentMpHandoffStatus -and
        (-not [string]::IsNullOrWhiteSpace($previousMpHandoffStatus) -or
         -not [string]::IsNullOrWhiteSpace($currentMpHandoffStatus))) {
    $mpHandoffFollowUpActive = $true
    $mpHandoffFollowUpReason = "handoff_status_changed_between_sessions"
}
elseif ($previousMpPreviousHostAvailable -ne $currentMpPreviousHostAvailable -and
        (-not [string]::IsNullOrWhiteSpace($previousMpPreviousHostAvailable) -or
         -not [string]::IsNullOrWhiteSpace($currentMpPreviousHostAvailable))) {
    $mpHandoffFollowUpActive = $true
    $mpHandoffFollowUpReason = "previous_host_availability_changed_between_sessions"
}
if (-not $identityRiskWarning -and $mpHandoffFollowUpActive) {
    $recommendation = "review_mp_handoff_continuity"
}
$campaignLibraryFollowUpActive = $false
$campaignLibraryFollowUpReason = "none"
$previousCampaignLibrarySkippedDueToLimitCountParsed = Parse-OptionalInt -Value $previousCampaignLibrarySkippedDueToLimitCount
$currentCampaignLibrarySkippedDueToLimitCountParsed = Parse-OptionalInt -Value $currentCampaignLibrarySkippedDueToLimitCount
if ($currentCampaignLibraryPlanPresent -ne "true") {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "current_campaign_library_plan_missing"
}
elseif ($currentCampaignLibraryPlanReadiness -ne "ready") {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "current_campaign_library_plan_needs_attention"
}
elseif ($currentCampaignLibraryLimitReached -eq "true") {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "current_campaign_library_limit_reached"
}
elseif ($previousCampaignLibraryPlanPresent -ne $currentCampaignLibraryPlanPresent) {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "campaign_library_plan_presence_changed_between_sessions"
}
elseif (-not [string]::IsNullOrWhiteSpace($previousCampaignLibraryPlanSource) -and
        -not [string]::IsNullOrWhiteSpace($currentCampaignLibraryPlanSource) -and
        $previousCampaignLibraryPlanSource -ne $currentCampaignLibraryPlanSource) {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "campaign_library_plan_source_changed_between_sessions"
}
elseif (-not [string]::IsNullOrWhiteSpace($previousCampaignLibraryPlanReadiness) -and
        -not [string]::IsNullOrWhiteSpace($currentCampaignLibraryPlanReadiness) -and
        $previousCampaignLibraryPlanReadiness -ne $currentCampaignLibraryPlanReadiness) {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "campaign_library_plan_readiness_changed_between_sessions"
}
elseif ($previousCampaignLibraryLimitReached -ne $currentCampaignLibraryLimitReached) {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "campaign_library_limit_state_changed_between_sessions"
}
elseif ($null -ne $previousCampaignLibrarySkippedDueToLimitCountParsed -and
        $null -ne $currentCampaignLibrarySkippedDueToLimitCountParsed -and
        $previousCampaignLibrarySkippedDueToLimitCountParsed -ne $currentCampaignLibrarySkippedDueToLimitCountParsed) {
    $campaignLibraryFollowUpActive = $true
    $campaignLibraryFollowUpReason = "campaign_library_limit_skip_count_changed_between_sessions"
}
$observableEffectSignal = $false
$observableEffectReason = "no_observable_delta"
if ($overlayChanged) {
    $observableEffectSignal = $true
    $observableEffectReason = "generated_overlay_manifest_changed"
}
elseif ($previousGameplayStatus -ne $currentGameplayStatus) {
    $observableEffectSignal = $true
    $observableEffectReason = "gameplay_acceptance_state_changed"
}
elseif ($previousGameplayReason -ne $currentGameplayReason) {
    $observableEffectSignal = $true
    $observableEffectReason = "gameplay_acceptance_reason_changed"
}
elseif ($previousArchiveCount -ne $currentArchiveCount) {
    $observableEffectSignal = $true
    $observableEffectReason = "verified_archive_save_count_changed"
}

$result = [ordered]@{
    schema_version = 1
    previous_session_id = $previousSessionId
    current_session_id = $currentSessionId
    previous_session_dir = $previousSessionDirFull
    current_session_dir = $currentSessionDirFull
    generated_overlay_manifest_hash = [ordered]@{
        previous = $previousOverlayManifestHash
        current = $currentOverlayManifestHash
        changed = $overlayChanged
    }
    verified_archive_save_count = [ordered]@{
        previous = $previousArchiveCount
        current = $currentArchiveCount
        delta = ($currentArchiveCount - $previousArchiveCount)
    }
    gameplay_acceptance_state = [ordered]@{
        previous = $previousGameplayStatus
        current = $currentGameplayStatus
        changed = ($previousGameplayStatus -ne $currentGameplayStatus)
    }
    gameplay_acceptance_reason = [ordered]@{
        previous = $previousGameplayReason
        current = $currentGameplayReason
        changed = ($previousGameplayReason -ne $currentGameplayReason)
    }
    status_center_state = [ordered]@{
        previous = $previousStatusCenterState
        current = $currentStatusCenterState
        changed = ($previousStatusCenterState -ne $currentStatusCenterState)
    }
    status_center_reason = [ordered]@{
        previous = $previousStatusCenterReason
        current = $currentStatusCenterReason
        changed = ($previousStatusCenterReason -ne $currentStatusCenterReason)
    }
    status_center_summary_text = [ordered]@{
        previous = $previousStatusCenterSummaryText
        current = $currentStatusCenterSummaryText
        changed = ($previousStatusCenterSummaryText -ne $currentStatusCenterSummaryText)
    }
    generated_overlay_publish_gate_state = [ordered]@{
        previous = $previousGeneratedOverlayPublishGateState
        current = $currentGeneratedOverlayPublishGateState
        changed = ($previousGeneratedOverlayPublishGateState -ne $currentGeneratedOverlayPublishGateState)
    }
    generated_overlay_publish_gate_reason = [ordered]@{
        previous = $previousGeneratedOverlayPublishGateReason
        current = $currentGeneratedOverlayPublishGateReason
        changed = ($previousGeneratedOverlayPublishGateReason -ne $currentGeneratedOverlayPublishGateReason)
    }
    generated_overlay_publish_gate_can_publish = [ordered]@{
        previous = $previousGeneratedOverlayPublishGateCanPublish
        current = $currentGeneratedOverlayPublishGateCanPublish
        changed = ($previousGeneratedOverlayPublishGateCanPublish -ne $currentGeneratedOverlayPublishGateCanPublish)
    }
    generated_overlay_publish_gate_publish_command = [ordered]@{
        previous = $previousGeneratedOverlayPublishGatePublishCommand
        current = $currentGeneratedOverlayPublishGatePublishCommand
        changed = ($previousGeneratedOverlayPublishGatePublishCommand -ne $currentGeneratedOverlayPublishGatePublishCommand)
    }
    save_root_resolution = [ordered]@{
        previous = $previousSaveRootResolution
        current = $currentSaveRootResolution
        changed = ($previousSaveRootResolution -ne $currentSaveRootResolution)
    }
    save_root_source = [ordered]@{
        previous = $previousSaveRootSource
        current = $currentSaveRootSource
        changed = ($previousSaveRootSource -ne $currentSaveRootSource)
    }
    save_root_path = [ordered]@{
        previous = $previousSaveRootPath
        current = $currentSaveRootPath
        changed = ($previousSaveRootPath -ne $currentSaveRootPath)
    }
    save_root_campaign_count = [ordered]@{
        previous = $previousSaveRootCampaignCount
        current = $currentSaveRootCampaignCount
        changed = ($previousSaveRootCampaignCount -ne $currentSaveRootCampaignCount)
    }
    save_root_save_file_count = [ordered]@{
        previous = $previousSaveRootSaveFileCount
        current = $currentSaveRootSaveFileCount
        changed = ($previousSaveRootSaveFileCount -ne $currentSaveRootSaveFileCount)
    }
    save_root_autosave_anchor_count = [ordered]@{
        previous = $previousSaveRootAutosaveAnchorCount
        current = $currentSaveRootAutosaveAnchorCount
        changed = ($previousSaveRootAutosaveAnchorCount -ne $currentSaveRootAutosaveAnchorCount)
    }
    entry_point_analysis_path = [ordered]@{
        previous = $previousEntryPointAnalysisPath
        current = $currentEntryPointAnalysisPath
        changed = ($previousEntryPointAnalysisPath -ne $currentEntryPointAnalysisPath)
    }
    entry_point_readiness = [ordered]@{
        previous = $previousEntryPointReadiness
        current = $currentEntryPointReadiness
        changed = ($previousEntryPointReadiness -ne $currentEntryPointReadiness)
    }
    entry_point_reason = [ordered]@{
        previous = $previousEntryPointReason
        current = $currentEntryPointReason
        changed = ($previousEntryPointReason -ne $currentEntryPointReason)
    }
    entry_point_count = [ordered]@{
        previous = $previousEntryPointCount
        current = $currentEntryPointCount
        changed = ($previousEntryPointCount -ne $currentEntryPointCount)
    }
    entry_point_branch_ambiguity = [ordered]@{
        previous = $previousEntryPointBranchAmbiguity
        current = $currentEntryPointBranchAmbiguity
        changed = ($previousEntryPointBranchAmbiguity -ne $currentEntryPointBranchAmbiguity)
    }
    post_play_package_campaign_identity_state_summary = [ordered]@{
        previous = $previousPostPlayPackageCampaignIdentityStateSummary
        current = $currentPostPlayPackageCampaignIdentityStateSummary
        changed = ($previousPostPlayPackageCampaignIdentityStateSummary -ne $currentPostPlayPackageCampaignIdentityStateSummary)
    }
    post_play_personality_profile = [ordered]@{
        applied_previous = $previousPostPlayPersonalityProfileApplied
        applied_current = $currentPostPlayPersonalityProfileApplied
        applied_changed = ($previousPostPlayPersonalityProfileApplied -ne $currentPostPlayPersonalityProfileApplied)
        source_schema_version_previous = $previousPostPlayPersonalityProfileSourceSchemaVersion
        source_schema_version_current = $currentPostPlayPersonalityProfileSourceSchemaVersion
        source_schema_version_changed = ($previousPostPlayPersonalityProfileSourceSchemaVersion -ne $currentPostPlayPersonalityProfileSourceSchemaVersion)
        schema_compatibility_state_previous = $previousPostPlayPersonalityProfileSchemaCompatibilityState
        schema_compatibility_state_current = $currentPostPlayPersonalityProfileSchemaCompatibilityState
        schema_compatibility_state_changed = ($previousPostPlayPersonalityProfileSchemaCompatibilityState -ne $currentPostPlayPersonalityProfileSchemaCompatibilityState)
        schema_compatibility_note_previous = $previousPostPlayPersonalityProfileSchemaCompatibilityNote
        schema_compatibility_note_current = $currentPostPlayPersonalityProfileSchemaCompatibilityNote
        schema_compatibility_note_changed = ($previousPostPlayPersonalityProfileSchemaCompatibilityNote -ne $currentPostPlayPersonalityProfileSchemaCompatibilityNote)
        validated_update_summary_previous = $previousPostPlayPersonalityProfileValidatedUpdateSummary
        validated_update_summary_current = $currentPostPlayPersonalityProfileValidatedUpdateSummary
        validated_update_summary_changed = ($previousPostPlayPersonalityProfileValidatedUpdateSummary -ne $currentPostPlayPersonalityProfileValidatedUpdateSummary)
        prompt_output_note_previous = $previousPostPlayPersonalityProfilePromptOutputNote
        prompt_output_note_current = $currentPostPlayPersonalityProfilePromptOutputNote
        prompt_output_note_changed = ($previousPostPlayPersonalityProfilePromptOutputNote -ne $currentPostPlayPersonalityProfilePromptOutputNote)
        source_save_date_previous = $previousPostPlayPersonalityProfileSourceSaveDate
        source_save_date_current = $currentPostPlayPersonalityProfileSourceSaveDate
        source_save_date_changed = ($previousPostPlayPersonalityProfileSourceSaveDate -ne $currentPostPlayPersonalityProfileSourceSaveDate)
        zero_history_bootstrap_previous = $previousPostPlayPersonalityProfileZeroHistoryBootstrap
        zero_history_bootstrap_current = $currentPostPlayPersonalityProfileZeroHistoryBootstrap
        zero_history_bootstrap_changed = ($previousPostPlayPersonalityProfileZeroHistoryBootstrap -ne $currentPostPlayPersonalityProfileZeroHistoryBootstrap)
    }
    generated_overlay_publish_allowed = [ordered]@{
        previous = $previousGeneratedOverlayPublishAllowed
        current = $currentGeneratedOverlayPublishAllowed
        changed = ($previousGeneratedOverlayPublishAllowed -ne $currentGeneratedOverlayPublishAllowed)
    }
    memory_recovery_anchor_entry_point_id = [ordered]@{
        previous = $previousMemoryRecoveryAnchorEntryPointId
        current = $currentMemoryRecoveryAnchorEntryPointId
        changed = ($previousMemoryRecoveryAnchorEntryPointId -ne $currentMemoryRecoveryAnchorEntryPointId)
    }
    next_action = [ordered]@{
        previous = $previousNextAction
        current = $currentNextAction
        changed = ($previousNextAction -ne $currentNextAction)
    }
    next_action_reason = [ordered]@{
        previous = $previousNextActionReason
        current = $currentNextActionReason
        changed = ($previousNextActionReason -ne $currentNextActionReason)
    }
    next_action_command_hint_source = [ordered]@{
        previous = $previousNextActionCommandHintSource
        current = $currentNextActionCommandHintSource
        changed = ($previousNextActionCommandHintSource -ne $currentNextActionCommandHintSource)
    }
    next_action_command_hint = [ordered]@{
        previous = $previousNextActionCommandHint
        current = $currentNextActionCommandHint
        changed = ($previousNextActionCommandHint -ne $currentNextActionCommandHint)
    }
    next_action_path = [ordered]@{
        previous = $previousNextActionPath
        current = $currentNextActionPath
        changed = ($previousNextActionPath -ne $currentNextActionPath)
    }
    friend_trust_store_state = [ordered]@{
        previous = $previousFriendTrustStoreState
        current = $currentFriendTrustStoreState
        changed = ($previousFriendTrustStoreState -ne $currentFriendTrustStoreState)
    }
    friend_trust_store_reason = [ordered]@{
        previous = $previousFriendTrustStoreReason
        current = $currentFriendTrustStoreReason
        changed = ($previousFriendTrustStoreReason -ne $currentFriendTrustStoreReason)
    }
    friend_trust_store_path = [ordered]@{
        previous = $previousFriendTrustStorePath
        current = $currentFriendTrustStorePath
        changed = ($previousFriendTrustStorePath -ne $currentFriendTrustStorePath)
    }
    friend_trust_store_controls_state = [ordered]@{
        previous = $previousFriendTrustStoreControlsState
        current = $currentFriendTrustStoreControlsState
        changed = ($previousFriendTrustStoreControlsState -ne $currentFriendTrustStoreControlsState)
    }
    friend_trust_store_controls_reason = [ordered]@{
        previous = $previousFriendTrustStoreControlsReason
        current = $currentFriendTrustStoreControlsReason
        changed = ($previousFriendTrustStoreControlsReason -ne $currentFriendTrustStoreControlsReason)
    }
    friend_trust_store_controls_next_step = [ordered]@{
        previous = $previousFriendTrustStoreControlsNextStep
        current = $currentFriendTrustStoreControlsNextStep
        changed = ($previousFriendTrustStoreControlsNextStep -ne $currentFriendTrustStoreControlsNextStep)
    }
    friend_mesh_update_state = [ordered]@{
        previous = $previousFriendMeshUpdateState
        current = $currentFriendMeshUpdateState
        changed = ($previousFriendMeshUpdateState -ne $currentFriendMeshUpdateState)
    }
    friend_mesh_update_reason = [ordered]@{
        previous = $previousFriendMeshUpdateReason
        current = $currentFriendMeshUpdateReason
        changed = ($previousFriendMeshUpdateReason -ne $currentFriendMeshUpdateReason)
    }
    friend_mesh_update_next_step = [ordered]@{
        previous = $previousFriendMeshUpdateNextStep
        current = $currentFriendMeshUpdateNextStep
        changed = ($previousFriendMeshUpdateNextStep -ne $currentFriendMeshUpdateNextStep)
    }
    friend_mp_sync_transport_adapter_state = [ordered]@{
        previous = $previousFriendMpSyncTransportAdapterState
        current = $currentFriendMpSyncTransportAdapterState
        changed = ($previousFriendMpSyncTransportAdapterState -ne $currentFriendMpSyncTransportAdapterState)
    }
    friend_mp_sync_transport_adapter_reason = [ordered]@{
        previous = $previousFriendMpSyncTransportAdapterReason
        current = $currentFriendMpSyncTransportAdapterReason
        changed = ($previousFriendMpSyncTransportAdapterReason -ne $currentFriendMpSyncTransportAdapterReason)
    }
    friend_mp_sync_transport_adapter_next_step = [ordered]@{
        previous = $previousFriendMpSyncTransportAdapterNextStep
        current = $currentFriendMpSyncTransportAdapterNextStep
        changed = ($previousFriendMpSyncTransportAdapterNextStep -ne $currentFriendMpSyncTransportAdapterNextStep)
    }
    friend_mp_sync_transport_adapter_kind = [ordered]@{
        previous = $previousFriendMpSyncTransportAdapterKind
        current = $currentFriendMpSyncTransportAdapterKind
        changed = ($previousFriendMpSyncTransportAdapterKind -ne $currentFriendMpSyncTransportAdapterKind)
    }
    friend_mp_sync_transport_adapter_path = [ordered]@{
        previous = $previousFriendMpSyncTransportAdapterPath
        current = $currentFriendMpSyncTransportAdapterPath
        changed = ($previousFriendMpSyncTransportAdapterPath -ne $currentFriendMpSyncTransportAdapterPath)
    }
    campaign_library_plan_present = [ordered]@{
        previous = $previousCampaignLibraryPlanPresent
        current = $currentCampaignLibraryPlanPresent
        changed = ($previousCampaignLibraryPlanPresent -ne $currentCampaignLibraryPlanPresent)
    }
    campaign_library_plan_path = [ordered]@{
        previous = $previousCampaignLibraryPlanPath
        current = $currentCampaignLibraryPlanPath
        changed = ($previousCampaignLibraryPlanPath -ne $currentCampaignLibraryPlanPath)
    }
    campaign_library_plan_source = [ordered]@{
        previous = $previousCampaignLibraryPlanSource
        current = $currentCampaignLibraryPlanSource
        changed = ($previousCampaignLibraryPlanSource -ne $currentCampaignLibraryPlanSource)
    }
    campaign_library_plan_readiness = [ordered]@{
        previous = $previousCampaignLibraryPlanReadiness
        current = $currentCampaignLibraryPlanReadiness
        changed = ($previousCampaignLibraryPlanReadiness -ne $currentCampaignLibraryPlanReadiness)
    }
    campaign_library_plan_reason = [ordered]@{
        previous = $previousCampaignLibraryPlanReason
        current = $currentCampaignLibraryPlanReason
        changed = ($previousCampaignLibraryPlanReason -ne $currentCampaignLibraryPlanReason)
    }
    campaign_library_limit_reached = [ordered]@{
        previous = $previousCampaignLibraryLimitReached
        current = $currentCampaignLibraryLimitReached
        changed = ($previousCampaignLibraryLimitReached -ne $currentCampaignLibraryLimitReached)
    }
    campaign_library_skipped_due_to_limit_count = [ordered]@{
        previous = $previousCampaignLibrarySkippedDueToLimitCount
        current = $currentCampaignLibrarySkippedDueToLimitCount
        changed = ($previousCampaignLibrarySkippedDueToLimitCount -ne $currentCampaignLibrarySkippedDueToLimitCount)
    }
    campaign_library_follow_up = [ordered]@{
        active = $campaignLibraryFollowUpActive
        reason = $campaignLibraryFollowUpReason
    }
    mp_package_manifest_hash = [ordered]@{
        previous = $previousMpManifestHash
        current = $currentMpManifestHash
        changed = ($previousMpManifestHash -ne $currentMpManifestHash)
    }
    mp_status_snapshot_present = [ordered]@{
        previous = $previousMpStatusSnapshotPresent
        current = $currentMpStatusSnapshotPresent
        changed = ($previousMpStatusSnapshotPresent -ne $currentMpStatusSnapshotPresent)
    }
    mp_package_output_dir = [ordered]@{
        previous = $previousMpPackageOutputDir
        current = $currentMpPackageOutputDir
        changed = ($previousMpPackageOutputDir -ne $currentMpPackageOutputDir)
    }
    mp_package_zip_state = [ordered]@{
        previous = $previousMpPackageZipState
        current = $currentMpPackageZipState
        changed = ($previousMpPackageZipState -ne $currentMpPackageZipState)
    }
    mp_package_zip_reason = [ordered]@{
        previous = $previousMpPackageZipReason
        current = $currentMpPackageZipReason
        changed = ($previousMpPackageZipReason -ne $currentMpPackageZipReason)
    }
    mp_package_zip_sha256 = [ordered]@{
        previous = $previousMpPackageZipSha256
        current = $currentMpPackageZipSha256
        changed = ($previousMpPackageZipSha256 -ne $currentMpPackageZipSha256)
    }
    mp_package_zip_path = [ordered]@{
        previous = $previousMpPackageZipPath
        current = $currentMpPackageZipPath
        changed = ($previousMpPackageZipPath -ne $currentMpPackageZipPath)
    }
    mp_package_zip_bytes = [ordered]@{
        previous = $previousMpPackageZipBytes
        current = $currentMpPackageZipBytes
        changed = ($previousMpPackageZipBytes -ne $currentMpPackageZipBytes)
    }
    mp_package_provenance_state = [ordered]@{
        previous = $previousMpProvenanceState
        current = $currentMpProvenanceState
        changed = ($previousMpProvenanceState -ne $currentMpProvenanceState)
    }
    mp_package_source_quality_count = [ordered]@{
        previous = $previousMpSourceQualityCount
        current = $currentMpSourceQualityCount
        changed = ($previousMpSourceQualityCount -ne $currentMpSourceQualityCount)
    }
    mp_package_source_qualities = [ordered]@{
        previous = $previousMpSourceQualities
        current = $currentMpSourceQualities
        changed = (($previousMpSourceQualities -join "|") -ne ($currentMpSourceQualities -join "|"))
    }
    mp_package_bootstrap_campaign_count = [ordered]@{
        previous = $previousMpBootstrapCampaignCount
        current = $currentMpBootstrapCampaignCount
        changed = ($previousMpBootstrapCampaignCount -ne $currentMpBootstrapCampaignCount)
    }
    mp_package_readiness = [ordered]@{
        previous = $previousMpReadiness
        current = $currentMpReadiness
        changed = ($previousMpReadiness -ne $currentMpReadiness)
    }
    mp_package_warning_count = [ordered]@{
        previous = $previousMpWarningCount
        current = $currentMpWarningCount
        changed = ($previousMpWarningCount -ne $currentMpWarningCount)
        previous_int = $previousMpWarningCountParsed
        current_int = $currentMpWarningCountParsed
        delta_int = $mpWarningCountDelta
    }
    mp_package_warning_codes = [ordered]@{
        previous = $previousMpWarningCodes
        current = $currentMpWarningCodes
        changed = $mpWarningCodesChanged
    }
    mp_host_readiness = [ordered]@{
        previous = $previousMpHostReadiness
        current = $currentMpHostReadiness
        changed = ($previousMpHostReadiness -ne $currentMpHostReadiness)
    }
    mp_client_readiness_gate = [ordered]@{
        previous = $previousMpClientReadinessGate
        current = $currentMpClientReadinessGate
        changed = ($previousMpClientReadinessGate -ne $currentMpClientReadinessGate)
    }
    mp_host_next_step = [ordered]@{
        previous = $previousMpHostNextStep
        current = $currentMpHostNextStep
        changed = ($previousMpHostNextStep -ne $currentMpHostNextStep)
    }
    mp_client_next_step = [ordered]@{
        previous = $previousMpClientNextStep
        current = $currentMpClientNextStep
        changed = ($previousMpClientNextStep -ne $currentMpClientNextStep)
    }
    mp_handoff_status = [ordered]@{
        previous = $previousMpHandoffStatus
        current = $currentMpHandoffStatus
        changed = ($previousMpHandoffStatus -ne $currentMpHandoffStatus)
    }
    mp_previous_host_available = [ordered]@{
        previous = $previousMpPreviousHostAvailable
        current = $currentMpPreviousHostAvailable
        changed = ($previousMpPreviousHostAvailable -ne $currentMpPreviousHostAvailable)
    }
    mp_handoff_follow_up = [ordered]@{
        active = $mpHandoffFollowUpActive
        reason = $mpHandoffFollowUpReason
        command_hint = $compareCommandHint
    }
    mp_verify_command = [ordered]@{
        previous = $previousMpVerifyCommand
        current = $currentMpVerifyCommand
        changed = ($previousMpVerifyCommand -ne $currentMpVerifyCommand)
    }
    mp_import_command = [ordered]@{
        previous = $previousMpImportCommand
        current = $currentMpImportCommand
        changed = ($previousMpImportCommand -ne $currentMpImportCommand)
    }
    mp_strict_verify_command = [ordered]@{
        previous = $previousMpStrictVerifyCommand
        current = $currentMpStrictVerifyCommand
        changed = ($previousMpStrictVerifyCommand -ne $currentMpStrictVerifyCommand)
    }
    mp_strict_import_command = [ordered]@{
        previous = $previousMpStrictImportCommand
        current = $currentMpStrictImportCommand
        changed = ($previousMpStrictImportCommand -ne $currentMpStrictImportCommand)
    }
    mp_identity_mismatch_warning = [ordered]@{
        previous = $previousMpIdentityMismatchWarning
        current = $currentMpIdentityMismatchWarning
        changed = $mpIdentityMismatchWarningChanged
    }
    mp_identity_mismatch_warning_codes = [ordered]@{
        previous = $previousMpIdentityMismatchWarningCodes
        current = $currentMpIdentityMismatchWarningCodes
        changed = $mpIdentityMismatchWarningCodesChanged
    }
    mp_mismatch_warning_state = [ordered]@{
        previous = $previousMpMismatchWarningState
        current = $currentMpMismatchWarningState
        changed = ($previousMpMismatchWarningState -ne $currentMpMismatchWarningState)
    }
    mp_mismatch_warning_reason = [ordered]@{
        previous = $previousMpMismatchWarningReason
        current = $currentMpMismatchWarningReason
        changed = ($previousMpMismatchWarningReason -ne $currentMpMismatchWarningReason)
    }
    mp_mismatch_warning_codes = [ordered]@{
        previous = $previousMpMismatchWarningCodes
        current = $currentMpMismatchWarningCodes
        changed = (($previousMpMismatchWarningCodes -join "|") -ne ($currentMpMismatchWarningCodes -join "|"))
    }
    mp_version_mismatch_warning = [ordered]@{
        previous = $previousMpVersionMismatchWarning
        current = $currentMpVersionMismatchWarning
        changed = ($previousMpVersionMismatchWarning -ne $currentMpVersionMismatchWarning)
    }
    mp_campaign_id_mismatch_warning = [ordered]@{
        previous = $previousMpCampaignIdMismatchWarning
        current = $currentMpCampaignIdMismatchWarning
        changed = ($previousMpCampaignIdMismatchWarning -ne $currentMpCampaignIdMismatchWarning)
    }
    mp_overlay_version_mismatch_warning = [ordered]@{
        previous = $previousMpOverlayVersionMismatchWarning
        current = $currentMpOverlayVersionMismatchWarning
        changed = ($previousMpOverlayVersionMismatchWarning -ne $currentMpOverlayVersionMismatchWarning)
    }
    mp_mod_version_mismatch_warning = [ordered]@{
        previous = $previousMpModVersionMismatchWarning
        current = $currentMpModVersionMismatchWarning
        changed = ($previousMpModVersionMismatchWarning -ne $currentMpModVersionMismatchWarning)
    }
    mp_manifest_hash_mismatch_warning = [ordered]@{
        previous = $previousMpManifestHashMismatchWarning
        current = $currentMpManifestHashMismatchWarning
        changed = ($previousMpManifestHashMismatchWarning -ne $currentMpManifestHashMismatchWarning)
    }
    identity_risk_warning = [ordered]@{
        active = $identityRiskWarning
        reason = $identityRiskWarningReason
        current_warning_codes = $currentMpIdentityMismatchWarningCodes
    }
    observable_effect_signal = [ordered]@{
        active = $observableEffectSignal
        reason = $observableEffectReason
    }
    compare_command_hint = $compareCommandHint
    next_step_recommendation = $recommendation
}

if ([string]::IsNullOrWhiteSpace($OutputJson)) {
    $OutputJson = Join-Path $repoRoot "dist/private_reports/real_session_v0_compare.json"
}
$outputJsonFull = [System.IO.Path]::GetFullPath($OutputJson)
New-Item -ItemType Directory -Path (Split-Path -Parent $outputJsonFull) -Force | Out-Null
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputJsonFull -Encoding UTF8

Write-Host "real_session_v0_compare_ok=true"
Write-Host ("real_session_v0_compare_output_json=" + $outputJsonFull)
Write-Host ("real_session_v0_compare_command_hint=" + $compareCommandHint)
Write-Host ("real_session_v0_compare_recommendation=" + $recommendation)
$identityRiskWarningText = "false"
if ($identityRiskWarning) {
    $identityRiskWarningText = "true"
}
Write-Host ("real_session_v0_compare_identity_risk_warning=" + $identityRiskWarningText)
Write-Host ("real_session_v0_compare_identity_risk_warning_reason=" + $identityRiskWarningReason)
Write-Host ("real_session_v0_compare_observable_effect_signal=" + ($observableEffectSignal.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_observable_effect_reason=" + $observableEffectReason)
Write-Host ("real_session_v0_compare_verified_archive_save_count_current=" + $currentArchiveCount)
Write-Host ("real_session_v0_compare_verified_archive_save_count_previous=" + $previousArchiveCount)
Write-Host ("real_session_v0_compare_verified_archive_save_count_delta=" + ($currentArchiveCount - $previousArchiveCount))
Write-Host ("real_session_v0_compare_verified_archive_save_count_changed=" + ((($previousArchiveCount -ne $currentArchiveCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_gameplay_acceptance_state_current=" + $currentGameplayStatus)
Write-Host ("real_session_v0_compare_gameplay_acceptance_state_previous=" + $previousGameplayStatus)
Write-Host ("real_session_v0_compare_gameplay_acceptance_state_changed=" + ((($previousGameplayStatus -ne $currentGameplayStatus).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_gameplay_acceptance_reason_current=" + $currentGameplayReason)
Write-Host ("real_session_v0_compare_gameplay_acceptance_reason_previous=" + $previousGameplayReason)
Write-Host ("real_session_v0_compare_gameplay_acceptance_reason_changed=" + ((($previousGameplayReason -ne $currentGameplayReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_status_center_state_current=" + $currentStatusCenterState)
Write-Host ("real_session_v0_compare_status_center_state_previous=" + $previousStatusCenterState)
Write-Host ("real_session_v0_compare_status_center_state_changed=" + ((($previousStatusCenterState -ne $currentStatusCenterState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_status_center_reason_current=" + $currentStatusCenterReason)
Write-Host ("real_session_v0_compare_status_center_reason_previous=" + $previousStatusCenterReason)
Write-Host ("real_session_v0_compare_status_center_reason_changed=" + ((($previousStatusCenterReason -ne $currentStatusCenterReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_status_center_summary_text_current=" + $currentStatusCenterSummaryText)
Write-Host ("real_session_v0_compare_status_center_summary_text_previous=" + $previousStatusCenterSummaryText)
Write-Host ("real_session_v0_compare_status_center_summary_text_changed=" + ((($previousStatusCenterSummaryText -ne $currentStatusCenterSummaryText).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_state_current=" + $currentGeneratedOverlayPublishGateState)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_state_previous=" + $previousGeneratedOverlayPublishGateState)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_state_changed=" + ((($previousGeneratedOverlayPublishGateState -ne $currentGeneratedOverlayPublishGateState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_reason_current=" + $currentGeneratedOverlayPublishGateReason)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_reason_previous=" + $previousGeneratedOverlayPublishGateReason)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_reason_changed=" + ((($previousGeneratedOverlayPublishGateReason -ne $currentGeneratedOverlayPublishGateReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_can_publish_current=" + $currentGeneratedOverlayPublishGateCanPublish)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_can_publish_previous=" + $previousGeneratedOverlayPublishGateCanPublish)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_can_publish_changed=" + ((($previousGeneratedOverlayPublishGateCanPublish -ne $currentGeneratedOverlayPublishGateCanPublish).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_publish_command_current=" + $currentGeneratedOverlayPublishGatePublishCommand)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_publish_command_previous=" + $previousGeneratedOverlayPublishGatePublishCommand)
Write-Host ("real_session_v0_compare_generated_overlay_publish_gate_publish_command_changed=" + ((($previousGeneratedOverlayPublishGatePublishCommand -ne $currentGeneratedOverlayPublishGatePublishCommand).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_resolution_current=" + $currentSaveRootResolution)
Write-Host ("real_session_v0_compare_save_root_resolution_previous=" + $previousSaveRootResolution)
Write-Host ("real_session_v0_compare_save_root_resolution_changed=" + ((($previousSaveRootResolution -ne $currentSaveRootResolution).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_source_current=" + $currentSaveRootSource)
Write-Host ("real_session_v0_compare_save_root_source_previous=" + $previousSaveRootSource)
Write-Host ("real_session_v0_compare_save_root_source_changed=" + ((($previousSaveRootSource -ne $currentSaveRootSource).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_path_current=" + $currentSaveRootPath)
Write-Host ("real_session_v0_compare_save_root_path_previous=" + $previousSaveRootPath)
Write-Host ("real_session_v0_compare_save_root_path_changed=" + ((($previousSaveRootPath -ne $currentSaveRootPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_campaign_count_current=" + $currentSaveRootCampaignCount)
Write-Host ("real_session_v0_compare_save_root_campaign_count_previous=" + $previousSaveRootCampaignCount)
Write-Host ("real_session_v0_compare_save_root_campaign_count_changed=" + ((($previousSaveRootCampaignCount -ne $currentSaveRootCampaignCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_save_file_count_current=" + $currentSaveRootSaveFileCount)
Write-Host ("real_session_v0_compare_save_root_save_file_count_previous=" + $previousSaveRootSaveFileCount)
Write-Host ("real_session_v0_compare_save_root_save_file_count_changed=" + ((($previousSaveRootSaveFileCount -ne $currentSaveRootSaveFileCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_save_root_autosave_anchor_count_current=" + $currentSaveRootAutosaveAnchorCount)
Write-Host ("real_session_v0_compare_save_root_autosave_anchor_count_previous=" + $previousSaveRootAutosaveAnchorCount)
Write-Host ("real_session_v0_compare_save_root_autosave_anchor_count_changed=" + ((($previousSaveRootAutosaveAnchorCount -ne $currentSaveRootAutosaveAnchorCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_entry_point_analysis_path_current=" + $currentEntryPointAnalysisPath)
Write-Host ("real_session_v0_compare_entry_point_analysis_path_previous=" + $previousEntryPointAnalysisPath)
Write-Host ("real_session_v0_compare_entry_point_analysis_path_changed=" + ((($previousEntryPointAnalysisPath -ne $currentEntryPointAnalysisPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_entry_point_readiness_current=" + $currentEntryPointReadiness)
Write-Host ("real_session_v0_compare_entry_point_readiness_previous=" + $previousEntryPointReadiness)
Write-Host ("real_session_v0_compare_entry_point_readiness_changed=" + ((($previousEntryPointReadiness -ne $currentEntryPointReadiness).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_entry_point_reason_current=" + $currentEntryPointReason)
Write-Host ("real_session_v0_compare_entry_point_reason_previous=" + $previousEntryPointReason)
Write-Host ("real_session_v0_compare_entry_point_reason_changed=" + ((($previousEntryPointReason -ne $currentEntryPointReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_entry_point_count_current=" + $currentEntryPointCount)
Write-Host ("real_session_v0_compare_entry_point_count_previous=" + $previousEntryPointCount)
Write-Host ("real_session_v0_compare_entry_point_count_changed=" + ((($previousEntryPointCount -ne $currentEntryPointCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_entry_point_branch_ambiguity_current=" + $currentEntryPointBranchAmbiguity)
Write-Host ("real_session_v0_compare_entry_point_branch_ambiguity_previous=" + $previousEntryPointBranchAmbiguity)
Write-Host ("real_session_v0_compare_entry_point_branch_ambiguity_changed=" + ((($previousEntryPointBranchAmbiguity -ne $currentEntryPointBranchAmbiguity).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_package_campaign_identity_state_summary_current=" + $currentPostPlayPackageCampaignIdentityStateSummary)
Write-Host ("real_session_v0_compare_post_play_package_campaign_identity_state_summary_previous=" + $previousPostPlayPackageCampaignIdentityStateSummary)
Write-Host ("real_session_v0_compare_post_play_package_campaign_identity_state_summary_changed=" + ((($previousPostPlayPackageCampaignIdentityStateSummary -ne $currentPostPlayPackageCampaignIdentityStateSummary).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_applied_current=" + $currentPostPlayPersonalityProfileApplied)
Write-Host ("real_session_v0_compare_post_play_personality_profile_applied_previous=" + $previousPostPlayPersonalityProfileApplied)
Write-Host ("real_session_v0_compare_post_play_personality_profile_applied_changed=" + ((($previousPostPlayPersonalityProfileApplied -ne $currentPostPlayPersonalityProfileApplied).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_schema_version_current=" + $currentPostPlayPersonalityProfileSourceSchemaVersion)
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_schema_version_previous=" + $previousPostPlayPersonalityProfileSourceSchemaVersion)
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_schema_version_changed=" + ((($previousPostPlayPersonalityProfileSourceSchemaVersion -ne $currentPostPlayPersonalityProfileSourceSchemaVersion).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_state_current=" + $currentPostPlayPersonalityProfileSchemaCompatibilityState)
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_state_previous=" + $previousPostPlayPersonalityProfileSchemaCompatibilityState)
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_state_changed=" + ((($previousPostPlayPersonalityProfileSchemaCompatibilityState -ne $currentPostPlayPersonalityProfileSchemaCompatibilityState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_note_current=" + $currentPostPlayPersonalityProfileSchemaCompatibilityNote)
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_note_previous=" + $previousPostPlayPersonalityProfileSchemaCompatibilityNote)
Write-Host ("real_session_v0_compare_post_play_personality_profile_schema_compatibility_note_changed=" + ((($previousPostPlayPersonalityProfileSchemaCompatibilityNote -ne $currentPostPlayPersonalityProfileSchemaCompatibilityNote).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_validated_update_summary_current=" + $currentPostPlayPersonalityProfileValidatedUpdateSummary)
Write-Host ("real_session_v0_compare_post_play_personality_profile_validated_update_summary_previous=" + $previousPostPlayPersonalityProfileValidatedUpdateSummary)
Write-Host ("real_session_v0_compare_post_play_personality_profile_validated_update_summary_changed=" + ((($previousPostPlayPersonalityProfileValidatedUpdateSummary -ne $currentPostPlayPersonalityProfileValidatedUpdateSummary).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_prompt_output_note_current=" + $currentPostPlayPersonalityProfilePromptOutputNote)
Write-Host ("real_session_v0_compare_post_play_personality_profile_prompt_output_note_previous=" + $previousPostPlayPersonalityProfilePromptOutputNote)
Write-Host ("real_session_v0_compare_post_play_personality_profile_prompt_output_note_changed=" + ((($previousPostPlayPersonalityProfilePromptOutputNote -ne $currentPostPlayPersonalityProfilePromptOutputNote).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_save_date_current=" + $currentPostPlayPersonalityProfileSourceSaveDate)
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_save_date_previous=" + $previousPostPlayPersonalityProfileSourceSaveDate)
Write-Host ("real_session_v0_compare_post_play_personality_profile_source_save_date_changed=" + ((($previousPostPlayPersonalityProfileSourceSaveDate -ne $currentPostPlayPersonalityProfileSourceSaveDate).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_post_play_personality_profile_zero_history_bootstrap_current=" + $currentPostPlayPersonalityProfileZeroHistoryBootstrap)
Write-Host ("real_session_v0_compare_post_play_personality_profile_zero_history_bootstrap_previous=" + $previousPostPlayPersonalityProfileZeroHistoryBootstrap)
Write-Host ("real_session_v0_compare_post_play_personality_profile_zero_history_bootstrap_changed=" + ((($previousPostPlayPersonalityProfileZeroHistoryBootstrap -ne $currentPostPlayPersonalityProfileZeroHistoryBootstrap).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_generated_overlay_publish_allowed_current=" + $currentGeneratedOverlayPublishAllowed)
Write-Host ("real_session_v0_compare_generated_overlay_publish_allowed_previous=" + $previousGeneratedOverlayPublishAllowed)
Write-Host ("real_session_v0_compare_generated_overlay_publish_allowed_changed=" + ((($previousGeneratedOverlayPublishAllowed -ne $currentGeneratedOverlayPublishAllowed).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_memory_recovery_anchor_entry_point_id_current=" + $currentMemoryRecoveryAnchorEntryPointId)
Write-Host ("real_session_v0_compare_memory_recovery_anchor_entry_point_id_previous=" + $previousMemoryRecoveryAnchorEntryPointId)
Write-Host ("real_session_v0_compare_memory_recovery_anchor_entry_point_id_changed=" + ((($previousMemoryRecoveryAnchorEntryPointId -ne $currentMemoryRecoveryAnchorEntryPointId).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_next_action_current=" + $currentNextAction)
Write-Host ("real_session_v0_compare_next_action_previous=" + $previousNextAction)
Write-Host ("real_session_v0_compare_next_action_changed=" + ((($previousNextAction -ne $currentNextAction).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_next_action_reason_current=" + $currentNextActionReason)
Write-Host ("real_session_v0_compare_next_action_reason_previous=" + $previousNextActionReason)
Write-Host ("real_session_v0_compare_next_action_reason_changed=" + ((($previousNextActionReason -ne $currentNextActionReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_next_action_command_hint_source_current=" + $currentNextActionCommandHintSource)
Write-Host ("real_session_v0_compare_next_action_command_hint_source_previous=" + $previousNextActionCommandHintSource)
Write-Host ("real_session_v0_compare_next_action_command_hint_source_changed=" + ((($previousNextActionCommandHintSource -ne $currentNextActionCommandHintSource).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_next_action_command_hint_current=" + $currentNextActionCommandHint)
Write-Host ("real_session_v0_compare_next_action_command_hint_previous=" + $previousNextActionCommandHint)
Write-Host ("real_session_v0_compare_next_action_command_hint_changed=" + ((($previousNextActionCommandHint -ne $currentNextActionCommandHint).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_next_action_path_current=" + $currentNextActionPath)
Write-Host ("real_session_v0_compare_next_action_path_previous=" + $previousNextActionPath)
Write-Host ("real_session_v0_compare_next_action_path_changed=" + ((($previousNextActionPath -ne $currentNextActionPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_state_current=" + $currentFriendTrustStoreState)
Write-Host ("real_session_v0_compare_friend_trust_store_state_previous=" + $previousFriendTrustStoreState)
Write-Host ("real_session_v0_compare_friend_trust_store_state_changed=" + ((($previousFriendTrustStoreState -ne $currentFriendTrustStoreState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_reason_current=" + $currentFriendTrustStoreReason)
Write-Host ("real_session_v0_compare_friend_trust_store_reason_previous=" + $previousFriendTrustStoreReason)
Write-Host ("real_session_v0_compare_friend_trust_store_reason_changed=" + ((($previousFriendTrustStoreReason -ne $currentFriendTrustStoreReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_path_current=" + $currentFriendTrustStorePath)
Write-Host ("real_session_v0_compare_friend_trust_store_path_previous=" + $previousFriendTrustStorePath)
Write-Host ("real_session_v0_compare_friend_trust_store_path_changed=" + ((($previousFriendTrustStorePath -ne $currentFriendTrustStorePath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_controls_state_current=" + $currentFriendTrustStoreControlsState)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_state_previous=" + $previousFriendTrustStoreControlsState)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_state_changed=" + ((($previousFriendTrustStoreControlsState -ne $currentFriendTrustStoreControlsState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_controls_reason_current=" + $currentFriendTrustStoreControlsReason)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_reason_previous=" + $previousFriendTrustStoreControlsReason)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_reason_changed=" + ((($previousFriendTrustStoreControlsReason -ne $currentFriendTrustStoreControlsReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_trust_store_controls_next_step_current=" + $currentFriendTrustStoreControlsNextStep)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_next_step_previous=" + $previousFriendTrustStoreControlsNextStep)
Write-Host ("real_session_v0_compare_friend_trust_store_controls_next_step_changed=" + ((($previousFriendTrustStoreControlsNextStep -ne $currentFriendTrustStoreControlsNextStep).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mesh_update_state_current=" + $currentFriendMeshUpdateState)
Write-Host ("real_session_v0_compare_friend_mesh_update_state_previous=" + $previousFriendMeshUpdateState)
Write-Host ("real_session_v0_compare_friend_mesh_update_state_changed=" + ((($previousFriendMeshUpdateState -ne $currentFriendMeshUpdateState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mesh_update_reason_current=" + $currentFriendMeshUpdateReason)
Write-Host ("real_session_v0_compare_friend_mesh_update_reason_previous=" + $previousFriendMeshUpdateReason)
Write-Host ("real_session_v0_compare_friend_mesh_update_reason_changed=" + ((($previousFriendMeshUpdateReason -ne $currentFriendMeshUpdateReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mesh_update_next_step_current=" + $currentFriendMeshUpdateNextStep)
Write-Host ("real_session_v0_compare_friend_mesh_update_next_step_previous=" + $previousFriendMeshUpdateNextStep)
Write-Host ("real_session_v0_compare_friend_mesh_update_next_step_changed=" + ((($previousFriendMeshUpdateNextStep -ne $currentFriendMeshUpdateNextStep).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_state_current=" + $currentFriendMpSyncTransportAdapterState)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_state_previous=" + $previousFriendMpSyncTransportAdapterState)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_state_changed=" + ((($previousFriendMpSyncTransportAdapterState -ne $currentFriendMpSyncTransportAdapterState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_reason_current=" + $currentFriendMpSyncTransportAdapterReason)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_reason_previous=" + $previousFriendMpSyncTransportAdapterReason)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_reason_changed=" + ((($previousFriendMpSyncTransportAdapterReason -ne $currentFriendMpSyncTransportAdapterReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_next_step_current=" + $currentFriendMpSyncTransportAdapterNextStep)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_next_step_previous=" + $previousFriendMpSyncTransportAdapterNextStep)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_next_step_changed=" + ((($previousFriendMpSyncTransportAdapterNextStep -ne $currentFriendMpSyncTransportAdapterNextStep).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_kind_current=" + $currentFriendMpSyncTransportAdapterKind)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_kind_previous=" + $previousFriendMpSyncTransportAdapterKind)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_kind_changed=" + ((($previousFriendMpSyncTransportAdapterKind -ne $currentFriendMpSyncTransportAdapterKind).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_path_current=" + $currentFriendMpSyncTransportAdapterPath)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_path_previous=" + $previousFriendMpSyncTransportAdapterPath)
Write-Host ("real_session_v0_compare_friend_mp_sync_transport_adapter_path_changed=" + ((($previousFriendMpSyncTransportAdapterPath -ne $currentFriendMpSyncTransportAdapterPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_plan_present_current=" + $currentCampaignLibraryPlanPresent)
Write-Host ("real_session_v0_compare_campaign_library_plan_present_previous=" + $previousCampaignLibraryPlanPresent)
Write-Host ("real_session_v0_compare_campaign_library_plan_present_changed=" + ((($previousCampaignLibraryPlanPresent -ne $currentCampaignLibraryPlanPresent).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_plan_path_current=" + $currentCampaignLibraryPlanPath)
Write-Host ("real_session_v0_compare_campaign_library_plan_path_previous=" + $previousCampaignLibraryPlanPath)
Write-Host ("real_session_v0_compare_campaign_library_plan_path_changed=" + ((($previousCampaignLibraryPlanPath -ne $currentCampaignLibraryPlanPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_plan_source_current=" + $currentCampaignLibraryPlanSource)
Write-Host ("real_session_v0_compare_campaign_library_plan_source_previous=" + $previousCampaignLibraryPlanSource)
Write-Host ("real_session_v0_compare_campaign_library_plan_source_changed=" + ((($previousCampaignLibraryPlanSource -ne $currentCampaignLibraryPlanSource).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_plan_readiness_current=" + $currentCampaignLibraryPlanReadiness)
Write-Host ("real_session_v0_compare_campaign_library_plan_readiness_previous=" + $previousCampaignLibraryPlanReadiness)
Write-Host ("real_session_v0_compare_campaign_library_plan_readiness_changed=" + ((($previousCampaignLibraryPlanReadiness -ne $currentCampaignLibraryPlanReadiness).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_plan_reason_current=" + $currentCampaignLibraryPlanReason)
Write-Host ("real_session_v0_compare_campaign_library_plan_reason_previous=" + $previousCampaignLibraryPlanReason)
Write-Host ("real_session_v0_compare_campaign_library_plan_reason_changed=" + ((($previousCampaignLibraryPlanReason -ne $currentCampaignLibraryPlanReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_limit_reached_current=" + $currentCampaignLibraryLimitReached)
Write-Host ("real_session_v0_compare_campaign_library_limit_reached_previous=" + $previousCampaignLibraryLimitReached)
Write-Host ("real_session_v0_compare_campaign_library_limit_reached_changed=" + ((($previousCampaignLibraryLimitReached -ne $currentCampaignLibraryLimitReached).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_current=" + $currentCampaignLibrarySkippedDueToLimitCount)
Write-Host ("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_previous=" + $previousCampaignLibrarySkippedDueToLimitCount)
Write-Host ("real_session_v0_compare_campaign_library_skipped_due_to_limit_count_changed=" + ((($previousCampaignLibrarySkippedDueToLimitCount -ne $currentCampaignLibrarySkippedDueToLimitCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_campaign_library_follow_up_active=" + ($campaignLibraryFollowUpActive.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_campaign_library_follow_up_reason=" + $campaignLibraryFollowUpReason)
Write-Host ("real_session_v0_compare_mp_host_readiness_current=" + $currentMpHostReadiness)
Write-Host ("real_session_v0_compare_mp_host_readiness_previous=" + $previousMpHostReadiness)
Write-Host ("real_session_v0_compare_mp_host_readiness_changed=" + ((($previousMpHostReadiness -ne $currentMpHostReadiness).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_client_readiness_gate_current=" + $currentMpClientReadinessGate)
Write-Host ("real_session_v0_compare_mp_client_readiness_gate_previous=" + $previousMpClientReadinessGate)
Write-Host ("real_session_v0_compare_mp_client_readiness_gate_changed=" + ((($previousMpClientReadinessGate -ne $currentMpClientReadinessGate).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_host_next_step_current=" + $currentMpHostNextStep)
Write-Host ("real_session_v0_compare_mp_host_next_step_previous=" + $previousMpHostNextStep)
Write-Host ("real_session_v0_compare_mp_host_next_step_changed=" + ((($previousMpHostNextStep -ne $currentMpHostNextStep).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_client_next_step_current=" + $currentMpClientNextStep)
Write-Host ("real_session_v0_compare_mp_client_next_step_previous=" + $previousMpClientNextStep)
Write-Host ("real_session_v0_compare_mp_client_next_step_changed=" + ((($previousMpClientNextStep -ne $currentMpClientNextStep).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_handoff_status_current=" + $currentMpHandoffStatus)
Write-Host ("real_session_v0_compare_mp_handoff_status_previous=" + $previousMpHandoffStatus)
Write-Host ("real_session_v0_compare_mp_handoff_status_changed=" + ((($previousMpHandoffStatus -ne $currentMpHandoffStatus).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_previous_host_available_current=" + $currentMpPreviousHostAvailable)
Write-Host ("real_session_v0_compare_mp_previous_host_available_previous=" + $previousMpPreviousHostAvailable)
Write-Host ("real_session_v0_compare_mp_previous_host_available_changed=" + ((($previousMpPreviousHostAvailable -ne $currentMpPreviousHostAvailable).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_handoff_follow_up_active=" + ($mpHandoffFollowUpActive.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_handoff_follow_up_reason=" + $mpHandoffFollowUpReason)
Write-Host ("real_session_v0_compare_mp_handoff_follow_up_command_hint=" + $compareCommandHint)
Write-Host ("real_session_v0_compare_mp_verify_command_current=" + $currentMpVerifyCommand)
Write-Host ("real_session_v0_compare_mp_verify_command_previous=" + $previousMpVerifyCommand)
Write-Host ("real_session_v0_compare_mp_verify_command_changed=" + ((($previousMpVerifyCommand -ne $currentMpVerifyCommand).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_import_command_current=" + $currentMpImportCommand)
Write-Host ("real_session_v0_compare_mp_import_command_previous=" + $previousMpImportCommand)
Write-Host ("real_session_v0_compare_mp_import_command_changed=" + ((($previousMpImportCommand -ne $currentMpImportCommand).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_strict_verify_command_current=" + $currentMpStrictVerifyCommand)
Write-Host ("real_session_v0_compare_mp_strict_verify_command_previous=" + $previousMpStrictVerifyCommand)
Write-Host ("real_session_v0_compare_mp_strict_verify_command_changed=" + ((($previousMpStrictVerifyCommand -ne $currentMpStrictVerifyCommand).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_strict_import_command_current=" + $currentMpStrictImportCommand)
Write-Host ("real_session_v0_compare_mp_strict_import_command_previous=" + $previousMpStrictImportCommand)
Write-Host ("real_session_v0_compare_mp_strict_import_command_changed=" + ((($previousMpStrictImportCommand -ne $currentMpStrictImportCommand).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_manifest_hash_current=" + $currentMpManifestHash)
Write-Host ("real_session_v0_compare_mp_manifest_hash_previous=" + $previousMpManifestHash)
Write-Host ("real_session_v0_compare_mp_manifest_hash_changed=" + ((($previousMpManifestHash -ne $currentMpManifestHash).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_status_snapshot_present_current=" + ($currentMpStatusSnapshotPresent.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_status_snapshot_present_previous=" + ($previousMpStatusSnapshotPresent.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_status_snapshot_present_changed=" + ((($previousMpStatusSnapshotPresent -ne $currentMpStatusSnapshotPresent).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_output_dir_current=" + $currentMpPackageOutputDir)
Write-Host ("real_session_v0_compare_mp_package_output_dir_previous=" + $previousMpPackageOutputDir)
Write-Host ("real_session_v0_compare_mp_package_output_dir_changed=" + ((($previousMpPackageOutputDir -ne $currentMpPackageOutputDir).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_zip_state_current=" + $currentMpPackageZipState)
Write-Host ("real_session_v0_compare_mp_package_zip_state_previous=" + $previousMpPackageZipState)
Write-Host ("real_session_v0_compare_mp_package_zip_state_changed=" + ((($previousMpPackageZipState -ne $currentMpPackageZipState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_zip_reason_current=" + $currentMpPackageZipReason)
Write-Host ("real_session_v0_compare_mp_package_zip_reason_previous=" + $previousMpPackageZipReason)
Write-Host ("real_session_v0_compare_mp_package_zip_reason_changed=" + ((($previousMpPackageZipReason -ne $currentMpPackageZipReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_zip_sha256_current=" + $currentMpPackageZipSha256)
Write-Host ("real_session_v0_compare_mp_package_zip_sha256_previous=" + $previousMpPackageZipSha256)
Write-Host ("real_session_v0_compare_mp_package_zip_sha256_changed=" + ((($previousMpPackageZipSha256 -ne $currentMpPackageZipSha256).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_zip_path_current=" + $currentMpPackageZipPath)
Write-Host ("real_session_v0_compare_mp_package_zip_path_previous=" + $previousMpPackageZipPath)
Write-Host ("real_session_v0_compare_mp_package_zip_path_changed=" + ((($previousMpPackageZipPath -ne $currentMpPackageZipPath).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_package_zip_bytes_current=" + $currentMpPackageZipBytes)
Write-Host ("real_session_v0_compare_mp_package_zip_bytes_previous=" + $previousMpPackageZipBytes)
Write-Host ("real_session_v0_compare_mp_package_zip_bytes_changed=" + ((($previousMpPackageZipBytes -ne $currentMpPackageZipBytes).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_provenance_state_current=" + $currentMpProvenanceState)
Write-Host ("real_session_v0_compare_mp_provenance_state_previous=" + $previousMpProvenanceState)
Write-Host ("real_session_v0_compare_mp_provenance_state_changed=" + ((($previousMpProvenanceState -ne $currentMpProvenanceState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_source_quality_count_current=" + $currentMpSourceQualityCount)
Write-Host ("real_session_v0_compare_mp_source_quality_count_previous=" + $previousMpSourceQualityCount)
Write-Host ("real_session_v0_compare_mp_source_quality_count_changed=" + ((($previousMpSourceQualityCount -ne $currentMpSourceQualityCount).ToString().ToLowerInvariant())))
foreach ($sourceQuality in $previousMpSourceQualities) {
    Write-Host ("real_session_v0_compare_mp_source_quality_previous=" + $sourceQuality)
}
foreach ($sourceQuality in $currentMpSourceQualities) {
    Write-Host ("real_session_v0_compare_mp_source_quality_current=" + $sourceQuality)
}
Write-Host ("real_session_v0_compare_mp_source_qualities_changed=" + ((($previousMpSourceQualities -join "|") -ne ($currentMpSourceQualities -join "|")).ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_bootstrap_campaign_count_current=" + $currentMpBootstrapCampaignCount)
Write-Host ("real_session_v0_compare_mp_bootstrap_campaign_count_previous=" + $previousMpBootstrapCampaignCount)
Write-Host ("real_session_v0_compare_mp_bootstrap_campaign_count_changed=" + ((($previousMpBootstrapCampaignCount -ne $currentMpBootstrapCampaignCount).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_warning_count_current=" + $currentMpWarningCount)
if ($null -ne $mpWarningCountDelta) {
    Write-Host ("real_session_v0_compare_mp_warning_count_delta=" + $mpWarningCountDelta)
}
Write-Host ("real_session_v0_compare_mp_warning_codes_changed=" + ($mpWarningCodesChanged.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_current=" + $currentMpIdentityMismatchWarning)
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_previous=" + $previousMpIdentityMismatchWarning)
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_changed=" + ($mpIdentityMismatchWarningChanged.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_codes_changed=" + ($mpIdentityMismatchWarningCodesChanged.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_mismatch_warning_state_current=" + $currentMpMismatchWarningState)
Write-Host ("real_session_v0_compare_mp_mismatch_warning_state_previous=" + $previousMpMismatchWarningState)
Write-Host ("real_session_v0_compare_mp_mismatch_warning_state_changed=" + ((($previousMpMismatchWarningState -ne $currentMpMismatchWarningState).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_mismatch_warning_reason_current=" + $currentMpMismatchWarningReason)
Write-Host ("real_session_v0_compare_mp_mismatch_warning_reason_previous=" + $previousMpMismatchWarningReason)
Write-Host ("real_session_v0_compare_mp_mismatch_warning_reason_changed=" + ((($previousMpMismatchWarningReason -ne $currentMpMismatchWarningReason).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_mismatch_warning_codes_changed=" + (((($previousMpMismatchWarningCodes -join "|") -ne ($currentMpMismatchWarningCodes -join "|")).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_game_version_mismatch_warning_current=" + ($currentMpVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_game_version_mismatch_warning_previous=" + ($previousMpVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_game_version_mismatch_warning_changed=" + ((($previousMpVersionMismatchWarning -ne $currentMpVersionMismatchWarning).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_campaign_id_mismatch_warning_current=" + ($currentMpCampaignIdMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_campaign_id_mismatch_warning_previous=" + ($previousMpCampaignIdMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_campaign_id_mismatch_warning_changed=" + ((($previousMpCampaignIdMismatchWarning -ne $currentMpCampaignIdMismatchWarning).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_overlay_version_mismatch_warning_current=" + ($currentMpOverlayVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_overlay_version_mismatch_warning_previous=" + ($previousMpOverlayVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_overlay_version_mismatch_warning_changed=" + ((($previousMpOverlayVersionMismatchWarning -ne $currentMpOverlayVersionMismatchWarning).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_mod_version_mismatch_warning_current=" + ($currentMpModVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_mod_version_mismatch_warning_previous=" + ($previousMpModVersionMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_mod_version_mismatch_warning_changed=" + ((($previousMpModVersionMismatchWarning -ne $currentMpModVersionMismatchWarning).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_manifest_hash_mismatch_warning_current=" + ($currentMpManifestHashMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_manifest_hash_mismatch_warning_previous=" + ($previousMpManifestHashMismatchWarning.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_manifest_hash_mismatch_warning_changed=" + ((($previousMpManifestHashMismatchWarning -ne $currentMpManifestHashMismatchWarning).ToString().ToLowerInvariant())))
foreach ($warningCode in $previousMpWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $currentMpWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $previousMpIdentityMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $currentMpIdentityMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $previousMpMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_mismatch_warning_code_previous=" + $warningCode)
}
foreach ($warningCode in $currentMpMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_mismatch_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $currentMpIdentityMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_identity_risk_warning_code=" + $warningCode)
}
exit 0
