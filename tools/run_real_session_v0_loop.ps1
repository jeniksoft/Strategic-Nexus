param(
    [Parameter(Mandatory = $true)]
    [string]$SaveRoot,
    [Parameter(Mandatory = $true)]
    [string]$ArchiveRoot,
    [Parameter(Mandatory = $true)]
    [string]$CampaignId,
    [Parameter(Mandatory = $true)]
    [string]$EmpireId,
    [Parameter(Mandatory = $true)]
    [string]$DslInput,
    [string]$PreviousSessionDirForCompare,
    [string]$SessionId,
    [string]$WorkDir,
    [string]$OverlayOutputDir,
    [string]$StatusOutputJson,
    [string]$SessionEvidenceJson,
    [string]$MpPackageOutputDir,
    [string]$MpOverlayVersion = "overlay_v0_session",
    [string]$MpGameVersion = "stellaris_4.x",
    [string]$MpModVersion = "strategic_nexus_v0",
    [switch]$ExportMpPackage,
    [switch]$EmitTrendSummary,
    [int]$StabilityDelayMs = 1200
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-ExePath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $candidatePaths = @(
        (Join-Path $repoRoot $RelativePath),
        (Join-Path $repoRoot ("dist/" + (Split-Path -Leaf $RelativePath)))
    )
    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

function Assert-LastExitCodeOk {
    param([Parameter(Mandatory = $true)][string]$StepName)
    if ($LASTEXITCODE -ne 0) {
        throw "$StepName failed (exit code $LASTEXITCODE)."
    }
}

function Get-KeyValueLineValue {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Lines,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $prefix = $Key + "="
    $line = $Lines | Where-Object { $_ -like "$prefix*" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return ""
    }
    return $line.Substring($prefix.Length)
}

function Get-KeyValueLineValues {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Lines,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $prefix = $Key + "="
    $result = @()
    foreach ($line in $Lines) {
        if ($line -like "$prefix*") {
            $result += $line.Substring($prefix.Length)
        }
    }
    return @($result)
}

function Get-SafeFileToken {
    param([Parameter(Mandatory = $true)][string]$Value)
    $safe = $Value -replace '[^A-Za-z0-9._-]', "_"
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "session"
    }
    return $safe
}

function Get-VariableOrDefault {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$DefaultValue = ""
    )

    $var = Get-Variable -Name $Name -ErrorAction SilentlyContinue
    if ($null -eq $var -or $null -eq $var.Value) {
        return $DefaultValue
    }
    return [string]$var.Value
}

if ([string]::IsNullOrWhiteSpace($SessionId)) {
    $SessionId = "session_" + (Get-Date -Format "yyyyMMdd_HHmmss")
}

$defaultRunRoot = Join-Path $repoRoot ("dist/real_session_v0_loop/" + $SessionId)
if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    $WorkDir = Join-Path $defaultRunRoot "work"
}
if ([string]::IsNullOrWhiteSpace($OverlayOutputDir)) {
    $OverlayOutputDir = Join-Path $defaultRunRoot "generated_overlay"
}
if ([string]::IsNullOrWhiteSpace($StatusOutputJson)) {
    $StatusOutputJson = Join-Path $defaultRunRoot "snc_status_snapshot.json"
}
if ([string]::IsNullOrWhiteSpace($SessionEvidenceJson)) {
    $SessionEvidenceJson = Join-Path $defaultRunRoot "real_session_v0_loop_evidence.json"
}
$statusWithMpOutputJson = Join-Path $defaultRunRoot "snc_status_snapshot_with_mp.json"
if ([string]::IsNullOrWhiteSpace($MpPackageOutputDir)) {
    $MpPackageOutputDir = Join-Path $defaultRunRoot "mp_overlay_package"
}

$exe = Resolve-ExePath -RelativePath "dist/strategic_nexus_app_test.exe"
if (-not $exe) {
    throw "Missing strategic_nexus_app_test.exe. Tip: run cmd /c tools/run_v0_pipeline_tests.cmd once to build and verify first."
}

$saveRootFull = [System.IO.Path]::GetFullPath($SaveRoot)
$archiveRootFull = [System.IO.Path]::GetFullPath($ArchiveRoot)
$dslInputFull = [System.IO.Path]::GetFullPath($DslInput)
$workDirFull = [System.IO.Path]::GetFullPath($WorkDir)
$overlayOutputDirFull = [System.IO.Path]::GetFullPath($OverlayOutputDir)
$statusOutputJsonFull = [System.IO.Path]::GetFullPath($StatusOutputJson)
$sessionEvidenceJsonFull = [System.IO.Path]::GetFullPath($SessionEvidenceJson)
$statusWithMpOutputJsonFull = [System.IO.Path]::GetFullPath($statusWithMpOutputJson)
$mpPackageOutputDirFull = [System.IO.Path]::GetFullPath($MpPackageOutputDir)
$sessionArchiveDir = Join-Path $archiveRootFull $SessionId
$archiveSummaryPath = Join-Path $workDirFull "archive_summary.json"

if (-not (Test-Path -LiteralPath $saveRootFull)) {
    throw "SaveRoot does not exist: $saveRootFull"
}
if (-not (Test-Path -LiteralPath $dslInputFull)) {
    throw "DslInput does not exist: $dslInputFull"
}
if (Test-Path -LiteralPath $sessionArchiveDir) {
    throw "Session archive directory already exists: $sessionArchiveDir. Use a different SessionId."
}

New-Item -ItemType Directory -Force -Path $archiveRootFull | Out-Null
New-Item -ItemType Directory -Force -Path $workDirFull | Out-Null
New-Item -ItemType Directory -Force -Path $overlayOutputDirFull | Out-Null

Remove-Item -LiteralPath $archiveSummaryPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $statusOutputJsonFull -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $sessionEvidenceJsonFull -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $statusWithMpOutputJsonFull -Force -ErrorAction SilentlyContinue

Write-Host "==> archive stable saves"
& $exe --archive-stable-saves $saveRootFull $archiveRootFull $SessionId $StabilityDelayMs
Assert-LastExitCodeOk -StepName "archive stable saves"

Write-Host "==> verify autosave archive"
& $exe --verify-autosave-archive $sessionArchiveDir
Assert-LastExitCodeOk -StepName "verify autosave archive"

Write-Host "==> summarize autosave archive"
& $exe --summarize-autosave-archive $sessionArchiveDir $archiveSummaryPath
Assert-LastExitCodeOk -StepName "summarize autosave archive"

Write-Host "==> run offline spine"
& $exe --run-offline-spine $sessionArchiveDir $CampaignId $EmpireId $dslInputFull $workDirFull $overlayOutputDirFull $statusOutputJsonFull
Assert-LastExitCodeOk -StepName "run offline spine"

$mpExportReadiness = ""
$mpExportManifestHash = ""
$mpExportWarningCount = ""
$mpExportIdentityMismatchWarning = ""
$mpExportIdentityMismatchWarningCodes = @()
$mpExportVerifyCommand = ""
$mpExportImportCommand = ""
$mpExportStrictVerifyCommand = ""
$mpExportStrictImportCommand = ""
$mpExportHostReadiness = ""
$mpExportClientReadinessGate = ""
$mpExportHostNextStep = ""
$mpExportClientNextStep = ""
$mpExportWarningCodes = @()
$sncStatusWithMpReadiness = ""
if ($ExportMpPackage) {
    if (Test-Path -LiteralPath $mpPackageOutputDirFull) {
        throw "MpPackageOutputDir must not already exist: $mpPackageOutputDirFull"
    }
    Write-Host "==> export mp overlay package"
    $mpExportLines = & $exe --export-mp-overlay-package $overlayOutputDirFull $CampaignId $MpOverlayVersion $MpGameVersion $MpModVersion $mpPackageOutputDirFull
    Assert-LastExitCodeOk -StepName "export mp overlay package"
    if ($null -eq $mpExportLines) {
        throw "MP package export returned no output."
    }

    $mpExportReadiness = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_readiness"
    if ([string]::IsNullOrWhiteSpace($mpExportReadiness)) {
        throw "MP package export output is missing mp_overlay_package_export_readiness."
    }

    if ($mpExportReadiness -ne "ready_for_mp") {
        throw "MP package export readiness is '$mpExportReadiness' instead of ready_for_mp."
    }
    $mpExportManifestHash = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_manifest_hash"
    $mpExportWarningCount = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_warning_count"
    $mpExportIdentityMismatchWarning = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_identity_mismatch_warning"
    $mpExportIdentityMismatchWarningCodes = Get-KeyValueLineValues -Lines $mpExportLines -Key "mp_overlay_package_export_identity_mismatch_warning_code"
    $mpExportVerifyCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_verify_command"
    $mpExportImportCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_import_command"
    $mpExportStrictVerifyCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_strict_verify_command"
    $mpExportStrictImportCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_strict_import_command"
    $mpExportHostReadiness = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_host_readiness"
    $mpExportClientReadinessGate = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_client_readiness_gate"
    $mpExportHostNextStep = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_host_next_step"
    $mpExportClientNextStep = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_client_next_step"
    $mpExportWarningCodes = Get-KeyValueLineValues -Lines $mpExportLines -Key "mp_overlay_package_export_warning_code"

    Write-Host "==> refresh snc status snapshot with mp package visibility"
    & $exe --snc-status-snapshot $archiveRootFull $overlayOutputDirFull $statusWithMpOutputJsonFull false $mpPackageOutputDirFull false
    Assert-LastExitCodeOk -StepName "snc status snapshot with mp package"
    if (-not (Test-Path -LiteralPath $statusWithMpOutputJsonFull)) {
        throw "Missing SNC MP status snapshot output: $statusWithMpOutputJsonFull"
    }

    $statusWithMpText = Get-Content -Raw -LiteralPath $statusWithMpOutputJsonFull
    if ($statusWithMpText -notmatch '"readiness"\s*:\s*"ready_for_mp"') {
        throw "SNC MP status snapshot is missing mp overlay package readiness=ready_for_mp."
    }
    $sncStatusWithMpReadiness = "ready_for_mp"
}

if (-not (Test-Path -LiteralPath $statusOutputJsonFull)) {
    throw "Missing SNC status snapshot output: $statusOutputJsonFull"
}
if (-not (Test-Path -LiteralPath $archiveSummaryPath)) {
    throw "Missing archive summary output: $archiveSummaryPath"
}

$statusText = Get-Content -Raw -LiteralPath $statusOutputJsonFull
if ($statusText -notmatch '"generated_overlay_status"\s*:\s*\{\s*"state"\s*:\s*"ready"') {
    throw "Offline spine status snapshot is missing generated_overlay_status.state=ready."
}
$statusJson = $statusText | ConvertFrom-Json
$gameplayAcceptanceState = ""
$gameplayAcceptanceReason = ""
$gameplayAcceptancePath = ""
if ($null -ne $statusJson.gameplay_acceptance_status) {
    if ($null -ne $statusJson.gameplay_acceptance_status.state) {
        $gameplayAcceptanceState = [string]$statusJson.gameplay_acceptance_status.state
    }
    if ($null -ne $statusJson.gameplay_acceptance_status.reason) {
        $gameplayAcceptanceReason = [string]$statusJson.gameplay_acceptance_status.reason
    }
    if ($null -ne $statusJson.gameplay_acceptance_status.path) {
        $gameplayAcceptancePath = [string]$statusJson.gameplay_acceptance_status.path
    }
}
if ([string]::IsNullOrWhiteSpace($gameplayAcceptanceState)) {
    throw "Offline spine status snapshot is missing gameplay_acceptance_status.state."
}
if ([string]::IsNullOrWhiteSpace($gameplayAcceptanceReason)) {
    throw "Offline spine status snapshot is missing gameplay_acceptance_status.reason."
}

Write-Host "real_session_v0_loop_ok=true"
Write-Host ("real_session_v0_loop_session_id=" + $SessionId)
Write-Host ("real_session_v0_loop_session_archive_dir=" + $sessionArchiveDir)
Write-Host ("real_session_v0_loop_archive_summary_path=" + $archiveSummaryPath)
Write-Host ("real_session_v0_loop_generated_overlay_dir=" + $overlayOutputDirFull)
Write-Host ("real_session_v0_loop_status_snapshot_path=" + $statusOutputJsonFull)
Write-Host ("real_session_v0_loop_gameplay_acceptance_state=" + $gameplayAcceptanceState)
Write-Host ("real_session_v0_loop_gameplay_acceptance_reason=" + $gameplayAcceptanceReason)
if (-not [string]::IsNullOrWhiteSpace($gameplayAcceptancePath)) {
    Write-Host ("real_session_v0_loop_gameplay_acceptance_path=" + $gameplayAcceptancePath)
}
Write-Host ("real_session_v0_loop_compare_previous_session_dir_hint=dist\\real_session_v0_loop\\<previous_session_id>")
$compareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "dist\real_session_v0_loop\<previous_session_id>" "' + $defaultRunRoot + '" "dist\private_reports\real_session_v0_compare_' + $SessionId + '.json"'
Write-Host ("real_session_v0_loop_compare_command_hint=" + $compareCommandHint)
Write-Host "real_session_v0_loop_trend_sessions_root_hint=dist\\real_session_v0_loop"
$trendCommandHint = 'cmd /c tools\analyze_real_session_v0_trend.cmd "dist\real_session_v0_loop" "dist\private_reports\real_session_v0_trend.json"'
Write-Host ("real_session_v0_loop_trend_command_hint=" + $trendCommandHint)
$nextSessionCommandHint = 'cmd /c tools\run_real_session_v0_loop.cmd "' + $saveRootFull + '" "' + $archiveRootFull + '" "' + $CampaignId + '" "' + $EmpireId + '" "' + $dslInputFull + '"'
if ($ExportMpPackage) {
    $nextSessionCommandHint += " -ExportMpPackage"
}
if ($EmitTrendSummary) {
    $nextSessionCommandHint += " -EmitTrendSummary"
}
$nextSessionCommandHint += ' -PreviousSessionDirForCompare "' + $defaultRunRoot + '"'
Write-Host ("real_session_v0_loop_next_session_compare_baseline_dir=" + $defaultRunRoot)
Write-Host ("real_session_v0_loop_next_session_command_hint=" + $nextSessionCommandHint)
if (-not [string]::IsNullOrWhiteSpace($PreviousSessionDirForCompare)) {
    $previousSessionDirForCompareFull = [System.IO.Path]::GetFullPath($PreviousSessionDirForCompare)
    if (-not (Test-Path -LiteralPath $previousSessionDirForCompareFull)) {
        throw "PreviousSessionDirForCompare does not exist: $previousSessionDirForCompareFull"
    }

    $compareScriptPath = Join-Path $PSScriptRoot "compare_real_session_v0_outputs.ps1"
    if (-not (Test-Path -LiteralPath $compareScriptPath)) {
        throw "Missing compare script: $compareScriptPath"
    }

    $previousSessionToken = Get-SafeFileToken -Value (Split-Path -Leaf $previousSessionDirForCompareFull)
    $compareOutputJson = Join-Path $repoRoot ("dist/private_reports/real_session_v0_compare_" + $previousSessionToken + "_to_" + $SessionId + ".json")
    $compareLines = & powershell -NoProfile -ExecutionPolicy Bypass -File $compareScriptPath $previousSessionDirForCompareFull $defaultRunRoot $compareOutputJson
    Assert-LastExitCodeOk -StepName "compare real session v0 outputs"
    if ($null -eq $compareLines) {
        throw "Compare script returned no output."
    }
    $compareRecommendation = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_recommendation"
    $compareOutputJsonLine = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_output_json"
    $compareCommandHintLine = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_command_hint"
    $compareIdentityRiskWarning = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_identity_risk_warning"
    $compareIdentityRiskWarningReason = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_identity_risk_warning_reason"
    $compareObservableEffectSignal = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_observable_effect_signal"
    $compareObservableEffectReason = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_observable_effect_reason"
    $compareGameplayAcceptanceStateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_current"
    $compareGameplayAcceptanceStatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_previous"
    $compareGameplayAcceptanceStateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_changed"
    $compareGameplayAcceptanceReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_current"
    $compareGameplayAcceptanceReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_previous"
    $compareGameplayAcceptanceReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_changed"
    $compareMpHostReadinessCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_readiness_current"
    $compareMpHostReadinessPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_readiness_previous"
    $compareMpHostReadinessChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_readiness_changed"
    $compareMpClientReadinessGateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_readiness_gate_current"
    $compareMpClientReadinessGatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_readiness_gate_previous"
    $compareMpClientReadinessGateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_readiness_gate_changed"
    $compareMpHostNextStepCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_next_step_current"
    $compareMpHostNextStepPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_next_step_previous"
    $compareMpHostNextStepChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_host_next_step_changed"
    $compareMpClientNextStepCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_next_step_current"
    $compareMpClientNextStepPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_next_step_previous"
    $compareMpClientNextStepChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_client_next_step_changed"
    $compareMpVerifyCommandCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_verify_command_current"
    $compareMpVerifyCommandPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_verify_command_previous"
    $compareMpVerifyCommandChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_verify_command_changed"
    $compareMpImportCommandCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_import_command_current"
    $compareMpImportCommandPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_import_command_previous"
    $compareMpImportCommandChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_import_command_changed"
    $compareMpStrictVerifyCommandCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_verify_command_current"
    $compareMpStrictVerifyCommandPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_verify_command_previous"
    $compareMpStrictVerifyCommandChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_verify_command_changed"
    $compareMpStrictImportCommandCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_import_command_current"
    $compareMpStrictImportCommandPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_import_command_previous"
    $compareMpStrictImportCommandChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_strict_import_command_changed"
    $compareMpIdentityMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_current"
    $compareMpIdentityMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_previous"
    $compareMpIdentityMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_changed"
    $compareMpIdentityMismatchWarningCodesChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_codes_changed"
    $compareMpIdentityMismatchWarningCodesPrevious = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_code_previous"
    $compareMpIdentityMismatchWarningCodesCurrent = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_identity_mismatch_warning_code_current"
    $compareMpManifestHashCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_current"
    $compareMpManifestHashPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_previous"
    $compareMpManifestHashChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_changed"
    $compareMpPackageOutputDirCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_current"
    $compareMpPackageOutputDirPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_previous"
    $compareMpPackageOutputDirChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_changed"
    $compareMpWarningCountCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_warning_count_current"
    $compareMpWarningCountDelta = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_warning_count_delta"
    $compareMpWarningCodesChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_warning_codes_changed"
    $compareMpWarningCodesPrevious = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_warning_code_previous"
    $compareMpWarningCodesCurrent = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_warning_code_current"
    $compareIdentityRiskWarningCodes = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_identity_risk_warning_code"
    if ([string]::IsNullOrWhiteSpace($compareRecommendation)) {
        throw "Compare output is missing real_session_v0_compare_recommendation."
    }
    if ([string]::IsNullOrWhiteSpace($compareOutputJsonLine)) {
        throw "Compare output is missing real_session_v0_compare_output_json."
    }
    if ([string]::IsNullOrWhiteSpace($compareIdentityRiskWarning)) {
        throw "Compare output is missing real_session_v0_compare_identity_risk_warning."
    }
    if ([string]::IsNullOrWhiteSpace($compareIdentityRiskWarningReason)) {
        throw "Compare output is missing real_session_v0_compare_identity_risk_warning_reason."
    }
    if ([string]::IsNullOrWhiteSpace($compareObservableEffectSignal)) {
        throw "Compare output is missing real_session_v0_compare_observable_effect_signal."
    }
    if ([string]::IsNullOrWhiteSpace($compareObservableEffectReason)) {
        throw "Compare output is missing real_session_v0_compare_observable_effect_reason."
    }
    if ([string]::IsNullOrWhiteSpace($compareCommandHintLine)) {
        throw "Compare output is missing real_session_v0_compare_command_hint."
    }
    $allowedCompareRecommendations = @("review_observable_deltas", "no_pipeline_delta_detected", "review_identity_risk_warning")
    if ($allowedCompareRecommendations -notcontains $compareRecommendation) {
        throw "Compare output has unsupported recommendation '$compareRecommendation'."
    }
    Write-Host "real_session_v0_loop_compare_auto_ok=true"
    Write-Host ("real_session_v0_loop_compare_auto_output_json=" + $compareOutputJsonLine)
    Write-Host ("real_session_v0_loop_compare_auto_command_hint=" + $compareCommandHintLine)
    Write-Host ("real_session_v0_loop_compare_auto_recommendation=" + $compareRecommendation)
    Write-Host ("real_session_v0_loop_compare_auto_identity_risk_warning=" + $compareIdentityRiskWarning)
    Write-Host ("real_session_v0_loop_compare_auto_identity_risk_warning_reason=" + $compareIdentityRiskWarningReason)
    Write-Host ("real_session_v0_loop_compare_auto_observable_effect_signal=" + $compareObservableEffectSignal)
    Write-Host ("real_session_v0_loop_compare_auto_observable_effect_reason=" + $compareObservableEffectReason)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_current=" + $compareGameplayAcceptanceStateCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_previous=" + $compareGameplayAcceptanceStatePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_changed=" + $compareGameplayAcceptanceStateChanged)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_current=" + $compareGameplayAcceptanceReasonCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_previous=" + $compareGameplayAcceptanceReasonPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_changed=" + $compareGameplayAcceptanceReasonChanged)
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_readiness_current=" + $compareMpHostReadinessCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_readiness_previous=" + $compareMpHostReadinessPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostReadinessChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_readiness_changed=" + $compareMpHostReadinessChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGateCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_readiness_gate_current=" + $compareMpClientReadinessGateCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGatePrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_readiness_gate_previous=" + $compareMpClientReadinessGatePrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientReadinessGateChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_readiness_gate_changed=" + $compareMpClientReadinessGateChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_next_step_current=" + $compareMpHostNextStepCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_next_step_previous=" + $compareMpHostNextStepPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpHostNextStepChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_host_next_step_changed=" + $compareMpHostNextStepChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_next_step_current=" + $compareMpClientNextStepCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_next_step_previous=" + $compareMpClientNextStepPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpClientNextStepChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_client_next_step_changed=" + $compareMpClientNextStepChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_verify_command_current=" + $compareMpVerifyCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_verify_command_previous=" + $compareMpVerifyCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpVerifyCommandChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_verify_command_changed=" + $compareMpVerifyCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_import_command_current=" + $compareMpImportCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_import_command_previous=" + $compareMpImportCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpImportCommandChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_import_command_changed=" + $compareMpImportCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_verify_command_current=" + $compareMpStrictVerifyCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_verify_command_previous=" + $compareMpStrictVerifyCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictVerifyCommandChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_verify_command_changed=" + $compareMpStrictVerifyCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_import_command_current=" + $compareMpStrictImportCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_import_command_previous=" + $compareMpStrictImportCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpStrictImportCommandChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_strict_import_command_changed=" + $compareMpStrictImportCommandChanged)
    }
    Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_previous=" + $compareMpManifestHashPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_current=" + $compareMpManifestHashCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_changed=" + $compareMpManifestHashChanged)
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_previous=" + $compareMpPackageOutputDirPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_current=" + $compareMpPackageOutputDirCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_changed=" + $compareMpPackageOutputDirChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCountCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_warning_count_current=" + $compareMpWarningCountCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCountDelta)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_warning_count_delta=" + $compareMpWarningCountDelta)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_warning_codes_changed=" + $compareMpWarningCodesChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_current=" + $compareMpIdentityMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_previous=" + $compareMpIdentityMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_changed=" + $compareMpIdentityMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpIdentityMismatchWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_codes_changed=" + $compareMpIdentityMismatchWarningCodesChanged)
    }
    foreach ($warningCode in $compareMpWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $compareMpWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_warning_code_current=" + $warningCode)
    }
    foreach ($warningCode in $compareMpIdentityMismatchWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $compareMpIdentityMismatchWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_identity_mismatch_warning_code_current=" + $warningCode)
    }
    foreach ($warningCode in $compareIdentityRiskWarningCodes) {
        Write-Host ("real_session_v0_loop_compare_auto_identity_risk_warning_code=" + $warningCode)
    }
}
if ($ExportMpPackage) {
    Write-Host ("real_session_v0_loop_mp_package_output_dir=" + $mpPackageOutputDirFull)
    Write-Host ("real_session_v0_loop_mp_package_readiness=" + $mpExportReadiness)
    Write-Host ("real_session_v0_loop_mp_package_manifest_hash=" + $mpExportManifestHash)
    Write-Host ("real_session_v0_loop_mp_package_warning_count=" + $mpExportWarningCount)
    Write-Host ("real_session_v0_loop_mp_package_identity_mismatch_warning=" + $mpExportIdentityMismatchWarning)
    foreach ($warningCode in $mpExportIdentityMismatchWarningCodes) {
        Write-Host ("real_session_v0_loop_mp_package_identity_mismatch_warning_code=" + $warningCode)
    }
    Write-Host ("real_session_v0_loop_mp_package_verify_command=" + $mpExportVerifyCommand)
    Write-Host ("real_session_v0_loop_mp_package_import_command=" + $mpExportImportCommand)
    Write-Host ("real_session_v0_loop_mp_package_strict_verify_command=" + $mpExportStrictVerifyCommand)
    Write-Host ("real_session_v0_loop_mp_package_strict_import_command=" + $mpExportStrictImportCommand)
    Write-Host ("real_session_v0_loop_mp_package_host_readiness=" + $mpExportHostReadiness)
    Write-Host ("real_session_v0_loop_mp_package_client_readiness_gate=" + $mpExportClientReadinessGate)
    Write-Host ("real_session_v0_loop_mp_package_host_next_step=" + $mpExportHostNextStep)
    Write-Host ("real_session_v0_loop_mp_package_client_next_step=" + $mpExportClientNextStep)
    foreach ($warningCode in $mpExportWarningCodes) {
        Write-Host ("real_session_v0_loop_mp_package_warning_code=" + $warningCode)
    }
    Write-Host ("real_session_v0_loop_status_snapshot_with_mp_path=" + $statusWithMpOutputJsonFull)
    Write-Host ("real_session_v0_loop_status_snapshot_with_mp_readiness=" + $sncStatusWithMpReadiness)
}
if ($EmitTrendSummary) {
    $trendScriptPath = Join-Path $PSScriptRoot "analyze_real_session_v0_trend.ps1"
    if (-not (Test-Path -LiteralPath $trendScriptPath)) {
        throw "Missing trend script: $trendScriptPath"
    }

    $trendSessionsRoot = Join-Path $repoRoot "dist/real_session_v0_loop"
    $trendOutputJson = Join-Path $repoRoot "dist/private_reports/real_session_v0_trend.json"
    $trendLines = & powershell -NoProfile -ExecutionPolicy Bypass -File $trendScriptPath $trendSessionsRoot $trendOutputJson
    Assert-LastExitCodeOk -StepName "analyze real session v0 trend"
    if ($null -eq $trendLines) {
        throw "Trend script returned no output."
    }
    $trendOutputJsonLine = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_output_json"
    $trendSessionCount = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_session_count"
    $trendRecommendation = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_recommendation"
    $trendIdentityRiskWarning = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_identity_risk_warning"
    $trendIdentityRiskWarningReason = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_identity_risk_warning_reason"
    $trendObservableEffectSignal = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_observable_effect_signal"
    $trendObservableEffectReason = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_observable_effect_reason"
    $trendGameplayAcceptanceStateCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_state_current"
    $trendGameplayAcceptanceStatePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_state_previous"
    $trendGameplayAcceptanceStateChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_state_changed"
    $trendGameplayAcceptanceReasonCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_reason_current"
    $trendGameplayAcceptanceReasonPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_reason_previous"
    $trendGameplayAcceptanceReasonChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_gameplay_acceptance_reason_changed"
    $trendMpWarningCountCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_count_current"
    $trendMpWarningCountDelta = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_count_delta"
    $trendMpWarningCodesChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_codes_changed"
    $trendMpManifestHashCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_current"
    $trendMpManifestHashPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_previous"
    $trendMpManifestHashChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_changed"
    $trendMpPackageOutputDirCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_current"
    $trendMpPackageOutputDirPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_previous"
    $trendMpPackageOutputDirChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_changed"
    $trendMpWarningCodesPrevious = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_warning_code_previous"
    $trendMpWarningCodesCurrent = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_warning_code_current"
    $trendMpHostReadinessCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_readiness_current"
    $trendMpHostReadinessPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_readiness_previous"
    $trendMpHostReadinessChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_readiness_changed"
    $trendMpClientReadinessGateCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_readiness_gate_current"
    $trendMpClientReadinessGatePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_readiness_gate_previous"
    $trendMpClientReadinessGateChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_readiness_gate_changed"
    $trendMpHostNextStepCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_next_step_current"
    $trendMpHostNextStepPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_next_step_previous"
    $trendMpHostNextStepChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_host_next_step_changed"
    $trendMpClientNextStepCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_next_step_current"
    $trendMpClientNextStepPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_next_step_previous"
    $trendMpClientNextStepChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_client_next_step_changed"
    $trendMpVerifyCommandCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_verify_command_current"
    $trendMpVerifyCommandPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_verify_command_previous"
    $trendMpVerifyCommandChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_verify_command_changed"
    $trendMpImportCommandCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_import_command_current"
    $trendMpImportCommandPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_import_command_previous"
    $trendMpImportCommandChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_import_command_changed"
    $trendMpStrictVerifyCommandCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_verify_command_current"
    $trendMpStrictVerifyCommandPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_verify_command_previous"
    $trendMpStrictVerifyCommandChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_verify_command_changed"
    $trendMpStrictImportCommandCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_import_command_current"
    $trendMpStrictImportCommandPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_import_command_previous"
    $trendMpStrictImportCommandChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_strict_import_command_changed"
    $trendMpIdentityMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_current"
    $trendMpIdentityMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_previous"
    $trendMpIdentityMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_changed"
    $trendMpIdentityMismatchWarningCodesChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_codes_changed"
    $trendMpIdentityMismatchWarningCodesPrevious = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_code_previous"
    $trendMpIdentityMismatchWarningCodesCurrent = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_identity_mismatch_warning_code_current"
    $trendLatestCompareCommandHint = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_latest_compare_command_hint"
    $trendNextSessionCommandHint = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_session_command_hint"
    $trendIdentityRiskWarningCodes = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_identity_risk_warning_code"
    $trendSessionCountInt = 0
    if (-not [int]::TryParse($trendSessionCount, [ref]$trendSessionCountInt)) {
        throw "Trend output has invalid session count '$trendSessionCount'."
    }
    $requireTrendCompareHint = ($trendSessionCountInt -ge 2)
    if ([string]::IsNullOrWhiteSpace($trendOutputJsonLine) -or
        [string]::IsNullOrWhiteSpace($trendSessionCount) -or
        [string]::IsNullOrWhiteSpace($trendRecommendation) -or
        [string]::IsNullOrWhiteSpace($trendIdentityRiskWarning) -or
        [string]::IsNullOrWhiteSpace($trendIdentityRiskWarningReason) -or
        [string]::IsNullOrWhiteSpace($trendObservableEffectSignal) -or
        [string]::IsNullOrWhiteSpace($trendObservableEffectReason) -or
        ($requireTrendCompareHint -and [string]::IsNullOrWhiteSpace($trendLatestCompareCommandHint)) -or
        [string]::IsNullOrWhiteSpace($trendNextSessionCommandHint)) {
        throw "Trend output is missing required fields."
    }
    $allowedTrendRecommendations = @("review_observable_deltas", "review_identity_risk_warning", "no_pipeline_delta_detected", "need_more_real_sessions")
    if ($allowedTrendRecommendations -notcontains $trendRecommendation) {
        throw "Trend output has unsupported recommendation '$trendRecommendation'."
    }
    Write-Host "real_session_v0_loop_trend_auto_ok=true"
    Write-Host ("real_session_v0_loop_trend_auto_output_json=" + $trendOutputJsonLine)
    Write-Host ("real_session_v0_loop_trend_auto_session_count=" + $trendSessionCount)
    Write-Host ("real_session_v0_loop_trend_auto_recommendation=" + $trendRecommendation)
    Write-Host ("real_session_v0_loop_trend_auto_identity_risk_warning=" + $trendIdentityRiskWarning)
    Write-Host ("real_session_v0_loop_trend_auto_identity_risk_warning_reason=" + $trendIdentityRiskWarningReason)
    Write-Host ("real_session_v0_loop_trend_auto_observable_effect_signal=" + $trendObservableEffectSignal)
    Write-Host ("real_session_v0_loop_trend_auto_observable_effect_reason=" + $trendObservableEffectReason)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_state_current=" + $trendGameplayAcceptanceStateCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_state_previous=" + $trendGameplayAcceptanceStatePrevious)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_state_changed=" + $trendGameplayAcceptanceStateChanged)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_reason_current=" + $trendGameplayAcceptanceReasonCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_reason_previous=" + $trendGameplayAcceptanceReasonPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_gameplay_acceptance_reason_changed=" + $trendGameplayAcceptanceReasonChanged)
    if (-not [string]::IsNullOrWhiteSpace($trendMpWarningCountCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_warning_count_current=" + $trendMpWarningCountCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpWarningCountDelta)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_warning_count_delta=" + $trendMpWarningCountDelta)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_warning_codes_changed=" + $trendMpWarningCodesChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_previous=" + $trendMpManifestHashPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_current=" + $trendMpManifestHashCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_changed=" + $trendMpManifestHashChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpPackageOutputDirPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_package_output_dir_previous=" + $trendMpPackageOutputDirPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpPackageOutputDirCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_package_output_dir_current=" + $trendMpPackageOutputDirCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpPackageOutputDirChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_package_output_dir_changed=" + $trendMpPackageOutputDirChanged)
    }
    foreach ($warningCode in $trendMpWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $trendMpWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_warning_code_current=" + $warningCode)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostReadinessCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_readiness_current=" + $trendMpHostReadinessCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostReadinessPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_readiness_previous=" + $trendMpHostReadinessPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostReadinessChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_readiness_changed=" + $trendMpHostReadinessChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientReadinessGateCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_readiness_gate_current=" + $trendMpClientReadinessGateCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientReadinessGatePrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_readiness_gate_previous=" + $trendMpClientReadinessGatePrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientReadinessGateChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_readiness_gate_changed=" + $trendMpClientReadinessGateChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostNextStepCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_next_step_current=" + $trendMpHostNextStepCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostNextStepPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_next_step_previous=" + $trendMpHostNextStepPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpHostNextStepChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_host_next_step_changed=" + $trendMpHostNextStepChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientNextStepCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_next_step_current=" + $trendMpClientNextStepCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientNextStepPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_next_step_previous=" + $trendMpClientNextStepPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpClientNextStepChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_client_next_step_changed=" + $trendMpClientNextStepChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpVerifyCommandCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_verify_command_current=" + $trendMpVerifyCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpVerifyCommandPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_verify_command_previous=" + $trendMpVerifyCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpVerifyCommandChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_verify_command_changed=" + $trendMpVerifyCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpImportCommandCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_import_command_current=" + $trendMpImportCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpImportCommandPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_import_command_previous=" + $trendMpImportCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpImportCommandChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_import_command_changed=" + $trendMpImportCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictVerifyCommandCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_verify_command_current=" + $trendMpStrictVerifyCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictVerifyCommandPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_verify_command_previous=" + $trendMpStrictVerifyCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictVerifyCommandChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_verify_command_changed=" + $trendMpStrictVerifyCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictImportCommandCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_import_command_current=" + $trendMpStrictImportCommandCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictImportCommandPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_import_command_previous=" + $trendMpStrictImportCommandPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpStrictImportCommandChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_strict_import_command_changed=" + $trendMpStrictImportCommandChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpIdentityMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_current=" + $trendMpIdentityMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpIdentityMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_previous=" + $trendMpIdentityMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpIdentityMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_changed=" + $trendMpIdentityMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpIdentityMismatchWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_codes_changed=" + $trendMpIdentityMismatchWarningCodesChanged)
    }
    foreach ($warningCode in $trendMpIdentityMismatchWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $trendMpIdentityMismatchWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_code_current=" + $warningCode)
    }
    Write-Host ("real_session_v0_loop_trend_auto_latest_compare_command_hint=" + $trendLatestCompareCommandHint)
    Write-Host ("real_session_v0_loop_trend_auto_next_session_command_hint=" + $trendNextSessionCommandHint)
    foreach ($warningCode in $trendIdentityRiskWarningCodes) {
        Write-Host ("real_session_v0_loop_trend_auto_identity_risk_warning_code=" + $warningCode)
    }
}

$sessionEvidence = [ordered]@{
    session_id = $SessionId
    session_archive_dir = $sessionArchiveDir
    archive_summary_path = $archiveSummaryPath
    generated_overlay_dir = $overlayOutputDirFull
    status_snapshot_path = $statusOutputJsonFull
    gameplay_acceptance = [ordered]@{
        state = $gameplayAcceptanceState
        reason = $gameplayAcceptanceReason
        path = $gameplayAcceptancePath
    }
    command_hints = [ordered]@{
        compare = $compareCommandHint
        trend = $trendCommandHint
        next_session_compare_baseline_dir = $defaultRunRoot
        next_session = $nextSessionCommandHint
    }
    mp_export = [ordered]@{
        enabled = [bool]$ExportMpPackage
        output_dir = $mpPackageOutputDirFull
        readiness = $mpExportReadiness
        manifest_hash = $mpExportManifestHash
        warning_count = $mpExportWarningCount
        identity_mismatch_warning = $mpExportIdentityMismatchWarning
        identity_mismatch_warning_codes = @($mpExportIdentityMismatchWarningCodes)
        warning_codes = @($mpExportWarningCodes)
        verify_command = $mpExportVerifyCommand
        import_command = $mpExportImportCommand
        strict_verify_command = $mpExportStrictVerifyCommand
        strict_import_command = $mpExportStrictImportCommand
        host_readiness = $mpExportHostReadiness
        client_readiness_gate = $mpExportClientReadinessGate
        host_next_step = $mpExportHostNextStep
        client_next_step = $mpExportClientNextStep
    }
    auto_compare = [ordered]@{
        enabled = -not [string]::IsNullOrWhiteSpace($PreviousSessionDirForCompare)
        output_json = (Get-VariableOrDefault -Name "compareOutputJsonLine")
        command_hint = (Get-VariableOrDefault -Name "compareCommandHintLine")
        recommendation = (Get-VariableOrDefault -Name "compareRecommendation")
        identity_risk_warning = (Get-VariableOrDefault -Name "compareIdentityRiskWarning")
        identity_risk_warning_reason = (Get-VariableOrDefault -Name "compareIdentityRiskWarningReason")
        identity_risk_warning_codes = @((Get-VariableOrDefault -Name "compareIdentityRiskWarningCodes"))
        observable_effect_signal = (Get-VariableOrDefault -Name "compareObservableEffectSignal")
        observable_effect_reason = (Get-VariableOrDefault -Name "compareObservableEffectReason")
        gameplay_acceptance_state_current = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStateCurrent")
        gameplay_acceptance_state_previous = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStatePrevious")
        gameplay_acceptance_state_changed = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStateChanged")
        gameplay_acceptance_reason_current = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonCurrent")
        gameplay_acceptance_reason_previous = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonPrevious")
        gameplay_acceptance_reason_changed = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonChanged")
        mp = [ordered]@{
            host_readiness_current = (Get-VariableOrDefault -Name "compareMpHostReadinessCurrent")
            host_readiness_previous = (Get-VariableOrDefault -Name "compareMpHostReadinessPrevious")
            host_readiness_changed = (Get-VariableOrDefault -Name "compareMpHostReadinessChanged")
            client_readiness_gate_current = (Get-VariableOrDefault -Name "compareMpClientReadinessGateCurrent")
            client_readiness_gate_previous = (Get-VariableOrDefault -Name "compareMpClientReadinessGatePrevious")
            client_readiness_gate_changed = (Get-VariableOrDefault -Name "compareMpClientReadinessGateChanged")
            host_next_step_current = (Get-VariableOrDefault -Name "compareMpHostNextStepCurrent")
            host_next_step_previous = (Get-VariableOrDefault -Name "compareMpHostNextStepPrevious")
            host_next_step_changed = (Get-VariableOrDefault -Name "compareMpHostNextStepChanged")
            client_next_step_current = (Get-VariableOrDefault -Name "compareMpClientNextStepCurrent")
            client_next_step_previous = (Get-VariableOrDefault -Name "compareMpClientNextStepPrevious")
            client_next_step_changed = (Get-VariableOrDefault -Name "compareMpClientNextStepChanged")
            verify_command_current = (Get-VariableOrDefault -Name "compareMpVerifyCommandCurrent")
            verify_command_previous = (Get-VariableOrDefault -Name "compareMpVerifyCommandPrevious")
            verify_command_changed = (Get-VariableOrDefault -Name "compareMpVerifyCommandChanged")
            import_command_current = (Get-VariableOrDefault -Name "compareMpImportCommandCurrent")
            import_command_previous = (Get-VariableOrDefault -Name "compareMpImportCommandPrevious")
            import_command_changed = (Get-VariableOrDefault -Name "compareMpImportCommandChanged")
            strict_verify_command_current = (Get-VariableOrDefault -Name "compareMpStrictVerifyCommandCurrent")
            strict_verify_command_previous = (Get-VariableOrDefault -Name "compareMpStrictVerifyCommandPrevious")
            strict_verify_command_changed = (Get-VariableOrDefault -Name "compareMpStrictVerifyCommandChanged")
            strict_import_command_current = (Get-VariableOrDefault -Name "compareMpStrictImportCommandCurrent")
            strict_import_command_previous = (Get-VariableOrDefault -Name "compareMpStrictImportCommandPrevious")
            strict_import_command_changed = (Get-VariableOrDefault -Name "compareMpStrictImportCommandChanged")
            manifest_hash_current = (Get-VariableOrDefault -Name "compareMpManifestHashCurrent")
            manifest_hash_previous = (Get-VariableOrDefault -Name "compareMpManifestHashPrevious")
            manifest_hash_changed = (Get-VariableOrDefault -Name "compareMpManifestHashChanged")
            package_output_dir_current = (Get-VariableOrDefault -Name "compareMpPackageOutputDirCurrent")
            package_output_dir_previous = (Get-VariableOrDefault -Name "compareMpPackageOutputDirPrevious")
            package_output_dir_changed = (Get-VariableOrDefault -Name "compareMpPackageOutputDirChanged")
            warning_count_current = (Get-VariableOrDefault -Name "compareMpWarningCountCurrent")
            warning_count_delta = (Get-VariableOrDefault -Name "compareMpWarningCountDelta")
            warning_codes_changed = (Get-VariableOrDefault -Name "compareMpWarningCodesChanged")
            warning_codes_current = @((Get-VariableOrDefault -Name "compareMpWarningCodesCurrent"))
            warning_codes_previous = @((Get-VariableOrDefault -Name "compareMpWarningCodesPrevious"))
            identity_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCurrent")
            identity_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningPrevious")
            identity_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningChanged")
            identity_mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCodesChanged")
            identity_mismatch_warning_codes_current = @((Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCodesCurrent"))
            identity_mismatch_warning_codes_previous = @((Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCodesPrevious"))
        }
    }
    auto_trend = [ordered]@{
        enabled = [bool]$EmitTrendSummary
        output_json = (Get-VariableOrDefault -Name "trendOutputJsonLine")
        session_count = (Get-VariableOrDefault -Name "trendSessionCount")
        recommendation = (Get-VariableOrDefault -Name "trendRecommendation")
        identity_risk_warning = (Get-VariableOrDefault -Name "trendIdentityRiskWarning")
        identity_risk_warning_reason = (Get-VariableOrDefault -Name "trendIdentityRiskWarningReason")
        identity_risk_warning_codes = @((Get-VariableOrDefault -Name "trendIdentityRiskWarningCodes"))
        observable_effect_signal = (Get-VariableOrDefault -Name "trendObservableEffectSignal")
        observable_effect_reason = (Get-VariableOrDefault -Name "trendObservableEffectReason")
        gameplay_acceptance_state_current = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStateCurrent")
        gameplay_acceptance_state_previous = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStatePrevious")
        gameplay_acceptance_state_changed = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStateChanged")
        gameplay_acceptance_reason_current = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonCurrent")
        gameplay_acceptance_reason_previous = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonPrevious")
        gameplay_acceptance_reason_changed = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonChanged")
        latest_compare_command_hint = (Get-VariableOrDefault -Name "trendLatestCompareCommandHint")
        next_session_command_hint = (Get-VariableOrDefault -Name "trendNextSessionCommandHint")
        mp = [ordered]@{
            host_readiness_current = (Get-VariableOrDefault -Name "trendMpHostReadinessCurrent")
            host_readiness_previous = (Get-VariableOrDefault -Name "trendMpHostReadinessPrevious")
            host_readiness_changed = (Get-VariableOrDefault -Name "trendMpHostReadinessChanged")
            client_readiness_gate_current = (Get-VariableOrDefault -Name "trendMpClientReadinessGateCurrent")
            client_readiness_gate_previous = (Get-VariableOrDefault -Name "trendMpClientReadinessGatePrevious")
            client_readiness_gate_changed = (Get-VariableOrDefault -Name "trendMpClientReadinessGateChanged")
            host_next_step_current = (Get-VariableOrDefault -Name "trendMpHostNextStepCurrent")
            host_next_step_previous = (Get-VariableOrDefault -Name "trendMpHostNextStepPrevious")
            host_next_step_changed = (Get-VariableOrDefault -Name "trendMpHostNextStepChanged")
            client_next_step_current = (Get-VariableOrDefault -Name "trendMpClientNextStepCurrent")
            client_next_step_previous = (Get-VariableOrDefault -Name "trendMpClientNextStepPrevious")
            client_next_step_changed = (Get-VariableOrDefault -Name "trendMpClientNextStepChanged")
            verify_command_current = (Get-VariableOrDefault -Name "trendMpVerifyCommandCurrent")
            verify_command_previous = (Get-VariableOrDefault -Name "trendMpVerifyCommandPrevious")
            verify_command_changed = (Get-VariableOrDefault -Name "trendMpVerifyCommandChanged")
            import_command_current = (Get-VariableOrDefault -Name "trendMpImportCommandCurrent")
            import_command_previous = (Get-VariableOrDefault -Name "trendMpImportCommandPrevious")
            import_command_changed = (Get-VariableOrDefault -Name "trendMpImportCommandChanged")
            strict_verify_command_current = (Get-VariableOrDefault -Name "trendMpStrictVerifyCommandCurrent")
            strict_verify_command_previous = (Get-VariableOrDefault -Name "trendMpStrictVerifyCommandPrevious")
            strict_verify_command_changed = (Get-VariableOrDefault -Name "trendMpStrictVerifyCommandChanged")
            strict_import_command_current = (Get-VariableOrDefault -Name "trendMpStrictImportCommandCurrent")
            strict_import_command_previous = (Get-VariableOrDefault -Name "trendMpStrictImportCommandPrevious")
            strict_import_command_changed = (Get-VariableOrDefault -Name "trendMpStrictImportCommandChanged")
            manifest_hash_current = (Get-VariableOrDefault -Name "trendMpManifestHashCurrent")
            manifest_hash_previous = (Get-VariableOrDefault -Name "trendMpManifestHashPrevious")
            manifest_hash_changed = (Get-VariableOrDefault -Name "trendMpManifestHashChanged")
            package_output_dir_current = (Get-VariableOrDefault -Name "trendMpPackageOutputDirCurrent")
            package_output_dir_previous = (Get-VariableOrDefault -Name "trendMpPackageOutputDirPrevious")
            package_output_dir_changed = (Get-VariableOrDefault -Name "trendMpPackageOutputDirChanged")
            warning_count_current = (Get-VariableOrDefault -Name "trendMpWarningCountCurrent")
            warning_count_delta = (Get-VariableOrDefault -Name "trendMpWarningCountDelta")
            warning_codes_changed = (Get-VariableOrDefault -Name "trendMpWarningCodesChanged")
            warning_codes_current = @((Get-VariableOrDefault -Name "trendMpWarningCodesCurrent"))
            warning_codes_previous = @((Get-VariableOrDefault -Name "trendMpWarningCodesPrevious"))
            identity_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCurrent")
            identity_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningPrevious")
            identity_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningChanged")
            identity_mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCodesChanged")
            identity_mismatch_warning_codes_current = @((Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCodesCurrent"))
            identity_mismatch_warning_codes_previous = @((Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCodesPrevious"))
        }
    }
}

$sessionEvidenceJsonDir = Split-Path -Parent $sessionEvidenceJsonFull
if (-not [string]::IsNullOrWhiteSpace($sessionEvidenceJsonDir)) {
    New-Item -ItemType Directory -Force -Path $sessionEvidenceJsonDir | Out-Null
}
($sessionEvidence | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $sessionEvidenceJsonFull -Encoding utf8
Write-Host ("real_session_v0_loop_evidence_json=" + $sessionEvidenceJsonFull)
exit 0
