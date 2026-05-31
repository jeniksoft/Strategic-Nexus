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
$previousNextAction = ""
$currentNextAction = ""
$previousNextActionReason = ""
$currentNextActionReason = ""
$previousNextActionCommandHintSource = ""
$currentNextActionCommandHintSource = ""
$previousNextActionCommandHint = ""
$currentNextActionCommandHint = ""
if (Test-Path -LiteralPath $previousEvidencePath) {
    $previousEvidence = Read-JsonFile -Path $previousEvidencePath
    if ($null -ne $previousEvidence.next_action) {
        $previousNextAction = Get-OptionalString -Object $previousEvidence.next_action -Property "action"
        $previousNextActionReason = Get-OptionalString -Object $previousEvidence.next_action -Property "reason"
        $previousNextActionCommandHintSource = Get-OptionalString -Object $previousEvidence.next_action -Property "command_hint_source"
        $previousNextActionCommandHint = Get-OptionalString -Object $previousEvidence.next_action -Property "command_hint"
    }
}
if (Test-Path -LiteralPath $currentEvidencePath) {
    $currentEvidence = Read-JsonFile -Path $currentEvidencePath
    if ($null -ne $currentEvidence.next_action) {
        $currentNextAction = Get-OptionalString -Object $currentEvidence.next_action -Property "action"
        $currentNextActionReason = Get-OptionalString -Object $currentEvidence.next_action -Property "reason"
        $currentNextActionCommandHintSource = Get-OptionalString -Object $currentEvidence.next_action -Property "command_hint_source"
        $currentNextActionCommandHint = Get-OptionalString -Object $currentEvidence.next_action -Property "command_hint"
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
$previousMpIdentityMismatchWarning = ""
$currentMpIdentityMismatchWarning = ""
$previousMpWarningCodes = @()
$currentMpWarningCodes = @()
$previousMpIdentityMismatchWarningCodes = @()
$currentMpIdentityMismatchWarningCodes = @()
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
        $previousMpWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $previousMpIdentityMismatchWarning = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $previousMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
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
        $currentMpWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $currentMpIdentityMismatchWarning = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $currentMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
    }
}
$previousMpWarningCodes = @($previousMpWarningCodes | Sort-Object -Unique)
$currentMpWarningCodes = @($currentMpWarningCodes | Sort-Object -Unique)
$previousMpWarningCodesJoined = ($previousMpWarningCodes -join "|")
$currentMpWarningCodesJoined = ($currentMpWarningCodes -join "|")
$mpWarningCodesChanged = ($previousMpWarningCodesJoined -ne $currentMpWarningCodesJoined)
$previousMpIdentityMismatchWarningCodes = @($previousMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$currentMpIdentityMismatchWarningCodes = @($currentMpIdentityMismatchWarningCodes | Sort-Object -Unique)
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
Write-Host ("real_session_v0_compare_mp_warning_count_current=" + $currentMpWarningCount)
if ($null -ne $mpWarningCountDelta) {
    Write-Host ("real_session_v0_compare_mp_warning_count_delta=" + $mpWarningCountDelta)
}
Write-Host ("real_session_v0_compare_mp_warning_codes_changed=" + ($mpWarningCodesChanged.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_current=" + $currentMpIdentityMismatchWarning)
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_previous=" + $previousMpIdentityMismatchWarning)
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_changed=" + ($mpIdentityMismatchWarningChanged.ToString().ToLowerInvariant()))
Write-Host ("real_session_v0_compare_mp_identity_mismatch_warning_codes_changed=" + ($mpIdentityMismatchWarningCodesChanged.ToString().ToLowerInvariant()))
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
foreach ($warningCode in $currentMpIdentityMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_identity_risk_warning_code=" + $warningCode)
}
exit 0
