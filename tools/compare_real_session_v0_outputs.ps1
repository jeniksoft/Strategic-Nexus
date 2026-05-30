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

$previousSessionDirFull = [System.IO.Path]::GetFullPath($PreviousSessionDir)
$currentSessionDirFull = [System.IO.Path]::GetFullPath($CurrentSessionDir)
$previousSessionId = Get-SessionIdFromDir -SessionDir $previousSessionDirFull
$currentSessionId = Get-SessionIdFromDir -SessionDir $currentSessionDirFull
$compareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "' + $previousSessionDirFull + '" "' + $currentSessionDirFull + '" "dist\private_reports\real_session_v0_compare_' + $previousSessionId + '_to_' + $currentSessionId + '.json"'

$previousStatusPath = Join-Path $previousSessionDirFull "snc_status_snapshot.json"
$currentStatusPath = Join-Path $currentSessionDirFull "snc_status_snapshot.json"
$previousSummaryPath = Join-Path $previousSessionDirFull "work/archive_summary.json"
$currentSummaryPath = Join-Path $currentSessionDirFull "work/archive_summary.json"
$previousStatusWithMpPath = Join-Path $previousSessionDirFull "snc_status_snapshot_with_mp.json"
$currentStatusWithMpPath = Join-Path $currentSessionDirFull "snc_status_snapshot_with_mp.json"

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
if ($null -ne $previousStatus.gameplay_acceptance_status -and $null -ne $previousStatus.gameplay_acceptance_status.state) {
    $previousGameplayStatus = [string]$previousStatus.gameplay_acceptance_status.state
}
if ($null -ne $currentStatus.gameplay_acceptance_status -and $null -ne $currentStatus.gameplay_acceptance_status.state) {
    $currentGameplayStatus = [string]$currentStatus.gameplay_acceptance_status.state
}

$previousMpManifestHash = ""
$currentMpManifestHash = ""
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
$previousMpIdentityMismatchWarning = ""
$currentMpIdentityMismatchWarning = ""
$previousMpWarningCodes = @()
$currentMpWarningCodes = @()
$previousMpIdentityMismatchWarningCodes = @()
$currentMpIdentityMismatchWarningCodes = @()
if (Test-Path -LiteralPath $previousStatusWithMpPath) {
    $previousStatusWithMp = Read-JsonFile -Path $previousStatusWithMpPath
    if ($null -ne $previousStatusWithMp.mp_overlay_package_status) {
        $previousMpManifestHash = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "package_manifest_hash"
        $previousMpReadiness = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "readiness"
        $previousMpWarningCount = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "warning_count"
        $previousMpHostReadiness = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "host_readiness"
        $previousMpClientReadinessGate = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "client_readiness_gate"
        $previousMpHostNextStep = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "host_next_step"
        $previousMpClientNextStep = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "client_next_step"
        $previousMpWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $previousMpIdentityMismatchWarning = Get-OptionalString -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $previousMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $previousStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
    }
}
if (Test-Path -LiteralPath $currentStatusWithMpPath) {
    $currentStatusWithMp = Read-JsonFile -Path $currentStatusWithMpPath
    if ($null -ne $currentStatusWithMp.mp_overlay_package_status) {
        $currentMpManifestHash = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "package_manifest_hash"
        $currentMpReadiness = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "readiness"
        $currentMpWarningCount = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "warning_count"
        $currentMpHostReadiness = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "host_readiness"
        $currentMpClientReadinessGate = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "client_readiness_gate"
        $currentMpHostNextStep = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "host_next_step"
        $currentMpClientNextStep = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "client_next_step"
        $currentMpWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "warning_codes"
        $currentMpIdentityMismatchWarning = Get-OptionalString -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning"
        $currentMpIdentityMismatchWarningCodes = Get-OptionalStringArray -Object $currentStatusWithMp.mp_overlay_package_status -Property "identity_mismatch_warning_codes"
    }
}
$previousMpWarningCodes = @($previousMpWarningCodes | Sort-Object -Unique)
$currentMpWarningCodes = @($currentMpWarningCodes | Sort-Object -Unique)
$previousMpWarningCodesJoined = ($previousMpWarningCodes -join "|")
$currentMpWarningCodesJoined = ($currentMpWarningCodes -join "|")
$previousMpIdentityMismatchWarningCodes = @($previousMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$currentMpIdentityMismatchWarningCodes = @($currentMpIdentityMismatchWarningCodes | Sort-Object -Unique)
$previousMpIdentityMismatchWarningCodesJoined = ($previousMpIdentityMismatchWarningCodes -join "|")
$currentMpIdentityMismatchWarningCodesJoined = ($currentMpIdentityMismatchWarningCodes -join "|")
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
if (-not $overlayChanged -and $previousArchiveCount -eq $currentArchiveCount -and $previousGameplayStatus -eq $currentGameplayStatus -and $previousMpManifestHash -eq $currentMpManifestHash) {
    $recommendation = "no_pipeline_delta_detected"
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
    mp_package_manifest_hash = [ordered]@{
        previous = $previousMpManifestHash
        current = $currentMpManifestHash
        changed = ($previousMpManifestHash -ne $currentMpManifestHash)
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
        changed = ($previousMpWarningCodesJoined -ne $currentMpWarningCodesJoined)
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
    mp_identity_mismatch_warning = [ordered]@{
        previous = $previousMpIdentityMismatchWarning
        current = $currentMpIdentityMismatchWarning
        changed = ($previousMpIdentityMismatchWarning -ne $currentMpIdentityMismatchWarning)
    }
    mp_identity_mismatch_warning_codes = [ordered]@{
        previous = $previousMpIdentityMismatchWarningCodes
        current = $currentMpIdentityMismatchWarningCodes
        changed = ($previousMpIdentityMismatchWarningCodesJoined -ne $currentMpIdentityMismatchWarningCodesJoined)
    }
    identity_risk_warning = [ordered]@{
        active = $identityRiskWarning
        reason = $identityRiskWarningReason
        current_warning_codes = $currentMpIdentityMismatchWarningCodes
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
Write-Host ("real_session_v0_compare_mp_host_readiness_current=" + $currentMpHostReadiness)
Write-Host ("real_session_v0_compare_mp_client_readiness_gate_current=" + $currentMpClientReadinessGate)
Write-Host ("real_session_v0_compare_mp_host_next_step_current=" + $currentMpHostNextStep)
Write-Host ("real_session_v0_compare_mp_client_next_step_current=" + $currentMpClientNextStep)
Write-Host ("real_session_v0_compare_mp_manifest_hash_current=" + $currentMpManifestHash)
Write-Host ("real_session_v0_compare_mp_manifest_hash_changed=" + ((($previousMpManifestHash -ne $currentMpManifestHash).ToString().ToLowerInvariant())))
Write-Host ("real_session_v0_compare_mp_warning_count_current=" + $currentMpWarningCount)
if ($null -ne $mpWarningCountDelta) {
    Write-Host ("real_session_v0_compare_mp_warning_count_delta=" + $mpWarningCountDelta)
}
foreach ($warningCode in $currentMpWarningCodes) {
    Write-Host ("real_session_v0_compare_mp_warning_code_current=" + $warningCode)
}
foreach ($warningCode in $currentMpIdentityMismatchWarningCodes) {
    Write-Host ("real_session_v0_compare_identity_risk_warning_code=" + $warningCode)
}
exit 0
