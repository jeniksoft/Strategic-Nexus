param(
    [string]$SaveRoot = "auto",
    [Parameter(Mandatory = $true)]
    [string]$ArchiveRoot,
    [string]$CampaignId = "",
    [string]$EmpireId = "",
    [string]$DslInput = "",
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
        [AllowEmptyString()]
        [object]$Lines,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    if ($null -eq $Lines) {
        return ""
    }
    $lineItems = @($Lines | ForEach-Object { [string]$_ })
    $prefix = $Key + "="
    $line = $lineItems | Where-Object { $_ -like "$prefix*" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return ""
    }
    return $line.Substring($prefix.Length)
}

function Get-KeyValueLineValues {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [object]$Lines,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    if ($null -eq $Lines) {
        return @()
    }
    $lineItems = @($Lines | ForEach-Object { [string]$_ })
    $prefix = $Key + "="
    $result = @()
    foreach ($line in $lineItems) {
        if ($line -like "$prefix*") {
            $result += $line.Substring($prefix.Length)
        }
    }
    return @($result)
}

function Contains-Value {
    param(
        [string[]]$Values,
        [string]$Needle
    )

    if ($null -eq $Values) {
        return $false
    }

    foreach ($value in $Values) {
        if ($value -eq $Needle) {
            return $true
        }
    }

    return $false
}

function Get-MpPackageMismatchSummary {
    param(
        [string]$IdentityMismatchWarning,
        [string]$CampaignIdMismatchWarning,
        [string]$OverlayVersionMismatchWarning,
        [string]$GameVersionMismatchWarning,
        [string]$ModVersionMismatchWarning,
        [string]$ManifestHashMismatchWarning
    )

    $codes = @()
    if ($IdentityMismatchWarning -eq "true") {
        $codes += "identity_mismatch"
    }
    if ($CampaignIdMismatchWarning -eq "true") {
        $codes += "campaign_id_mismatch"
    }
    if ($OverlayVersionMismatchWarning -eq "true") {
        $codes += "overlay_version_mismatch"
    }
    if ($GameVersionMismatchWarning -eq "true") {
        $codes += "game_version_mismatch"
    }
    if ($ModVersionMismatchWarning -eq "true") {
        $codes += "mod_version_mismatch"
    }
    if ($ManifestHashMismatchWarning -eq "true") {
        $codes += "manifest_hash_mismatch"
    }

    $state = "clear"
    $reason = "none"
    if ($codes.Count -gt 0) {
        $state = "warning"
        $reason = [string]::Join("|", $codes)
    }

    return [ordered]@{
        state = $state
        reason = $reason
        codes = @($codes)
    }
}

function Get-SafeFileToken {
    param([Parameter(Mandatory = $true)][string]$Value)
    $safe = $Value -replace '[^A-Za-z0-9._-]', "_"
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "session"
    }
    return $safe
}

function Get-UniqueNonEmptyValues {
    param([object[]]$Values)

    $result = New-Object System.Collections.Generic.List[string]
    foreach ($value in @($Values)) {
        $text = [string]$value
        if ([string]::IsNullOrWhiteSpace($text)) {
            continue
        }
        if (-not $result.Contains($text)) {
            [void]$result.Add($text)
        }
    }

    return @($result)
}

function New-CommandHintChoice {
    param(
        [string]$CommandHint,
        [string]$Source
    )

    return [ordered]@{
        command_hint = $CommandHint
        command_hint_source = $Source
    }
}

function Get-FirstCommandHintChoice {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Choices
    )

    foreach ($choice in @($Choices)) {
        if ($null -eq $choice) {
            continue
        }

        $commandHint = ""
        $commandHintSource = "none"
        if ($null -ne $choice.command_hint) {
            $commandHint = [string]$choice.command_hint
        }
        if ($null -ne $choice.command_hint_source -and -not [string]::IsNullOrWhiteSpace([string]$choice.command_hint_source)) {
            $commandHintSource = [string]$choice.command_hint_source
        }

        if (-not [string]::IsNullOrWhiteSpace($commandHint)) {
            return [ordered]@{
                command_hint = $commandHint
                command_hint_source = $commandHintSource
            }
        }
    }

    return [ordered]@{
        command_hint = ""
        command_hint_source = "none"
    }
}

function Convert-ToStableIdentityToken {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value,
        [string]$Fallback = "identity"
    )

    $safe = $Value.ToLowerInvariant() -replace '[^a-z0-9._-]', "_"
    $safe = $safe.Trim('_', '.', '-')
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return $Fallback
    }
    return $safe
}

function Get-NextActionSummary {
    param(
        [string]$MpMismatchWarningState,
        [string]$CompareRecommendation,
        [string]$TrendRecommendation,
        [string]$MpExportStrictVerifyCommand,
        [string]$CompareMpStrictVerifyCommandCurrent,
        [string]$TrendMpStrictVerifyCommandCurrent,
        [string]$CompareCommandHint,
        [string]$TrendNextSessionCommandHint,
        [string]$DefaultNextSessionCommandHint
    )

    if ($MpMismatchWarningState -eq "warning") {
        $preferredStrictVerifyHint = Get-FirstCommandHintChoice -Choices @(
            (New-CommandHintChoice -CommandHint $MpExportStrictVerifyCommand -Source "mp_export_strict_verify"),
            (New-CommandHintChoice -CommandHint $CompareMpStrictVerifyCommandCurrent -Source "compare_current_mp_strict_verify"),
            (New-CommandHintChoice -CommandHint $TrendMpStrictVerifyCommandCurrent -Source "trend_current_mp_strict_verify")
        )
        return [ordered]@{
            action = "review_mp_package_mismatch_warning"
            reason = "mp_export_mismatch_warning_active"
            command_hint = $preferredStrictVerifyHint.command_hint
            command_hint_source = $preferredStrictVerifyHint.command_hint_source
        }
    }

    if ($CompareRecommendation -eq "review_identity_risk_warning" -or $TrendRecommendation -eq "review_identity_risk_warning") {
        $preferredIdentityRiskHint = Get-FirstCommandHintChoice -Choices @(
            (New-CommandHintChoice -CommandHint $CompareMpStrictVerifyCommandCurrent -Source "compare_current_mp_strict_verify"),
            (New-CommandHintChoice -CommandHint $TrendMpStrictVerifyCommandCurrent -Source "trend_current_mp_strict_verify"),
            (New-CommandHintChoice -CommandHint $MpExportStrictVerifyCommand -Source "mp_export_strict_verify")
        )
        return [ordered]@{
            action = "review_identity_risk_warning"
            reason = "identity_risk_warning_active"
            command_hint = $preferredIdentityRiskHint.command_hint
            command_hint_source = $preferredIdentityRiskHint.command_hint_source
        }
    }

    if ($CompareRecommendation -eq "review_observable_deltas") {
        return [ordered]@{
            action = "run_next_session_compare_loop"
            reason = "observable_delta_detected"
            command_hint = $DefaultNextSessionCommandHint
            command_hint_source = "loop_next_session"
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($TrendNextSessionCommandHint)) {
        return [ordered]@{
            action = "run_next_session_compare_loop"
            reason = "trend_follow_up"
            command_hint = $TrendNextSessionCommandHint
            command_hint_source = "trend_next_session"
        }
    }

    return [ordered]@{
        action = "run_next_session_compare_loop"
        reason = "baseline_follow_up"
        command_hint = $DefaultNextSessionCommandHint
        command_hint_source = "loop_next_session"
    }
}

function Resolve-SaveRootFromDiscovery {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][string]$DefaultRunRootPath
    )

    $discoveryOutputPath = Join-Path $DefaultRunRootPath "save_root_discovery.json"
    $scanDir = Join-Path $DefaultRunRootPath "save_root_scans"
    Remove-Item -LiteralPath $discoveryOutputPath -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $scanDir | Out-Null

    $discoveryLines = & $ExePath --discover-stellaris-save-roots $discoveryOutputPath
    Assert-LastExitCodeOk -StepName "discover stellaris save roots"
    if (-not (Test-Path -LiteralPath $discoveryOutputPath)) {
        throw "Save root discovery output is missing: $discoveryOutputPath"
    }

    $discoveryJson = (Get-Content -Raw -LiteralPath $discoveryOutputPath) | ConvertFrom-Json
    $best = $null
    $candidateIndex = 0
    foreach ($candidate in @($discoveryJson.candidates)) {
        if ($null -eq $candidate -or $candidate.exists -ne $true -or [string]::IsNullOrWhiteSpace([string]$candidate.path)) {
            continue
        }

        $candidatePath = [string]$candidate.path
        $candidateSource = [string]$candidate.source
        $scanOutputPath = Join-Path $scanDir ("scan_" + $candidateIndex + ".json")
        $candidateIndex += 1
        Remove-Item -LiteralPath $scanOutputPath -Force -ErrorAction SilentlyContinue

        & $ExePath --scan-save-campaigns $candidatePath $scanOutputPath | Out-Null
        Assert-LastExitCodeOk -StepName ("scan save campaigns for " + $candidateSource)
        if (-not (Test-Path -LiteralPath $scanOutputPath)) {
            continue
        }

        $scanJson = (Get-Content -Raw -LiteralPath $scanOutputPath) | ConvertFrom-Json
        $campaignCount = 0
        if ($null -ne $scanJson.campaign_count) {
            $campaignCount = [int]$scanJson.campaign_count
        }
        $saveFileCount = 0
        $autosaveAnchorCount = 0
        foreach ($campaign in @($scanJson.campaigns)) {
            if ($null -ne $campaign.save_file_count) {
                $saveFileCount += [int]$campaign.save_file_count
            }
            $anchorName = ""
            if ($null -ne $campaign.anchor_save_name) {
                $anchorName = [string]$campaign.anchor_save_name
            }
            if ($anchorName -match '^(?i:autosave).*\.sav$' -or $anchorName -ieq "ironman.sav") {
                $autosaveAnchorCount += 1
            }
        }

        $sourceWeight = 0
        switch ($candidateSource) {
            "user_profile_documents" { $sourceWeight = 4 }
            "steam_cloud_userdata" { $sourceWeight = 3 }
            "onedrive_dokumenty" { $sourceWeight = 2 }
            "onedrive_documents" { $sourceWeight = 1 }
            default { $sourceWeight = 0 }
        }

        $score = ($autosaveAnchorCount * 1000000000) + ($sourceWeight * 10000000) + ($campaignCount * 100000) + $saveFileCount
        $candidateResult = [ordered]@{
            path = [string]$scanJson.root_path
            source = $candidateSource
            autosave_anchor_count = $autosaveAnchorCount
            campaign_count = $campaignCount
            save_file_count = $saveFileCount
            score = $score
        }
        if ($null -eq $best -or $candidateResult.score -gt $best.score) {
            $best = $candidateResult
        }
    }

    if ($null -eq $best -or [string]::IsNullOrWhiteSpace([string]$best.path)) {
        throw "SaveRoot=auto could not find any existing Stellaris save root."
    }

    return $best
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

function Get-VariableArrayOrDefault {
    param(
        [Parameter(Mandatory = $true)][string]$Name
    )

    $var = Get-Variable -Name $Name -ErrorAction SilentlyContinue
    if ($null -eq $var -or $null -eq $var.Value) {
        return @()
    }

    if ($var.Value -is [System.Array]) {
        return @($var.Value | ForEach-Object { [string]$_ })
    }

    return @([string]$var.Value)
}

if ([string]::IsNullOrWhiteSpace($SessionId)) {
    $SessionId = "session_" + (Get-Date -Format "yyyyMMdd_HHmmss")
}
$realSessionLoopRunId = "real-session-v0-loop-" + [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssfffZ")

$defaultRunRoot = Join-Path $repoRoot ("dist/real_session_v0_loop/" + $SessionId)
New-Item -ItemType Directory -Force -Path $defaultRunRoot | Out-Null
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

$saveRootResolution = "explicit"
$saveRootSource = "user_input"
$saveRootAutoAutosaveAnchorCount = ""
$saveRootAutoCampaignCount = ""
$saveRootAutoSaveFileCount = ""
if ($SaveRoot -eq "auto") {
    $resolvedSaveRoot = Resolve-SaveRootFromDiscovery -ExePath $exe -DefaultRunRootPath $defaultRunRoot
    $SaveRoot = [string]$resolvedSaveRoot.path
    $saveRootResolution = "auto_discovery"
    $saveRootSource = [string]$resolvedSaveRoot.source
    $saveRootAutoAutosaveAnchorCount = [string]$resolvedSaveRoot.autosave_anchor_count
    $saveRootAutoCampaignCount = [string]$resolvedSaveRoot.campaign_count
    $saveRootAutoSaveFileCount = [string]$resolvedSaveRoot.save_file_count
}

$saveRootFull = [System.IO.Path]::GetFullPath($SaveRoot)
$archiveRootFull = [System.IO.Path]::GetFullPath($ArchiveRoot)
$dslInputFull = ""
$workDirFull = [System.IO.Path]::GetFullPath($WorkDir)
$overlayOutputDirFull = [System.IO.Path]::GetFullPath($OverlayOutputDir)
$statusOutputJsonFull = [System.IO.Path]::GetFullPath($StatusOutputJson)
$sessionEvidenceJsonFull = [System.IO.Path]::GetFullPath($SessionEvidenceJson)
$statusWithMpOutputJsonFull = [System.IO.Path]::GetFullPath($statusWithMpOutputJson)
$mpPackageOutputDirFull = [System.IO.Path]::GetFullPath($MpPackageOutputDir)
$sessionArchiveDir = Join-Path $archiveRootFull $SessionId
$archiveSummaryPath = Join-Path $workDirFull "archive_summary.json"
$entryPointAnalysisPath = Join-Path $workDirFull "snc_entry_point_analysis.json"
$postPlayPackagePath = Join-Path $workDirFull "snc_post_play_package.json"
$decisionInputPackagePath = Join-Path $workDirFull "snc_decision_input_package.json"
$candidateDecisionPackagePath = Join-Path $workDirFull "snc_candidate_decision_package.json"
$dslDraftPath = Join-Path $workDirFull "snc_validated_dsl_draft.dsl"
$dslDraftAuditPath = Join-Path $workDirFull "snc_dsl_draft_package.json"
$sncGeneratedOverlayStagingDir = Join-Path $defaultRunRoot "snc_generated_overlay_staged"
$sncGeneratedOverlayStagingStatusPath = Join-Path $workDirFull "snc_generated_overlay_staging_status.json"

if (-not (Test-Path -LiteralPath $saveRootFull)) {
    throw "SaveRoot does not exist: $saveRootFull"
}
if (Test-Path -LiteralPath $sessionArchiveDir) {
    throw "Session archive directory already exists: $sessionArchiveDir. Use a different SessionId."
}

New-Item -ItemType Directory -Force -Path $archiveRootFull | Out-Null
New-Item -ItemType Directory -Force -Path $workDirFull | Out-Null
New-Item -ItemType Directory -Force -Path $overlayOutputDirFull | Out-Null

Remove-Item -LiteralPath $archiveSummaryPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $entryPointAnalysisPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $postPlayPackagePath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $decisionInputPackagePath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $candidateDecisionPackagePath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $dslDraftPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $dslDraftAuditPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $sncGeneratedOverlayStagingStatusPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $statusOutputJsonFull -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $sessionEvidenceJsonFull -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $statusWithMpOutputJsonFull -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $sncGeneratedOverlayStagingDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "==> archive stable saves"
Write-Host ("real_session_v0_loop_save_root_resolution=" + $saveRootResolution)
Write-Host ("real_session_v0_loop_save_root_source=" + $saveRootSource)
Write-Host ("real_session_v0_loop_save_root_path=" + $saveRootFull)
if (-not [string]::IsNullOrWhiteSpace($saveRootAutoCampaignCount)) {
    Write-Host ("real_session_v0_loop_save_root_campaign_count=" + $saveRootAutoCampaignCount)
}
if (-not [string]::IsNullOrWhiteSpace($saveRootAutoSaveFileCount)) {
    Write-Host ("real_session_v0_loop_save_root_save_file_count=" + $saveRootAutoSaveFileCount)
}
if (-not [string]::IsNullOrWhiteSpace($saveRootAutoAutosaveAnchorCount)) {
    Write-Host ("real_session_v0_loop_save_root_autosave_anchor_count=" + $saveRootAutoAutosaveAnchorCount)
}
& $exe --archive-stable-saves $saveRootFull $archiveRootFull $SessionId $StabilityDelayMs
Assert-LastExitCodeOk -StepName "archive stable saves"

Write-Host "==> verify autosave archive"
& $exe --verify-autosave-archive $sessionArchiveDir
Assert-LastExitCodeOk -StepName "verify autosave archive"

Write-Host "==> summarize autosave archive"
& $exe --summarize-autosave-archive $sessionArchiveDir $archiveSummaryPath
Assert-LastExitCodeOk -StepName "summarize autosave archive"
if (-not (Test-Path -LiteralPath $archiveSummaryPath)) {
    throw "Archive summary output is missing: $archiveSummaryPath"
}
$archiveSummaryText = Get-Content -Raw -LiteralPath $archiveSummaryPath
$archiveSummaryJson = $archiveSummaryText | ConvertFrom-Json
$archiveCopiedSaveCount = ""
$archiveLastArchivedPath = ""
if ($null -ne $archiveSummaryJson.copied_save_count) {
    $archiveCopiedSaveCount = [string]$archiveSummaryJson.copied_save_count
}
if ($null -ne $archiveSummaryJson.last_archived_path) {
    $archiveLastArchivedPath = [string]$archiveSummaryJson.last_archived_path
}
if ([string]::IsNullOrWhiteSpace($archiveCopiedSaveCount)) {
    throw "Archive summary is missing copied_save_count."
}

Write-Host "==> analyze save entry points"
& $exe --analyze-save-entry-points $sessionArchiveDir $entryPointAnalysisPath $saveRootFull
Assert-LastExitCodeOk -StepName "analyze save entry points"
if (-not (Test-Path -LiteralPath $entryPointAnalysisPath)) {
    throw "Entry point analysis output is missing: $entryPointAnalysisPath"
}
$entryPointAnalysisText = Get-Content -Raw -LiteralPath $entryPointAnalysisPath
$entryPointAnalysisJson = $entryPointAnalysisText | ConvertFrom-Json
$entryPointReadiness = [string]$entryPointAnalysisJson.readiness
$entryPointReason = [string]$entryPointAnalysisJson.reason
$entryPointCount = [string]$entryPointAnalysisJson.entry_point_count
$entryPointBranchAmbiguity = [string]([bool]$entryPointAnalysisJson.branch_ambiguity_detected).ToString().ToLowerInvariant()

Write-Host "==> build post-play package"
& $exe --build-post-play-package $sessionArchiveDir $postPlayPackagePath $saveRootFull
Assert-LastExitCodeOk -StepName "build post-play package"
if (-not (Test-Path -LiteralPath $postPlayPackagePath)) {
    throw "Post-play package output is missing: $postPlayPackagePath"
}
$postPlayPackageText = Get-Content -Raw -LiteralPath $postPlayPackagePath
$postPlayPackageJson = $postPlayPackageText | ConvertFrom-Json
$postPlayPackageReadiness = [string]$postPlayPackageJson.readiness
$postPlayPackageReason = [string]$postPlayPackageJson.reason
$postPlayDecisionReadyEntryCount = [string]$postPlayPackageJson.decision_ready_entry_count
$postPlayCampaignCount = [string]$postPlayPackageJson.campaigns.Count
$postPlayReadyCampaignCount = [string](@($postPlayPackageJson.campaigns | Where-Object { $_.readiness -eq "ready" }).Count)
$postPlayPartialCampaignCount = [string](@($postPlayPackageJson.campaigns | Where-Object { $_.readiness -eq "ready_partial" }).Count)
$postPlayBlockedCampaignCount = [string](@($postPlayPackageJson.campaigns | Where-Object { $_.readiness -notin @("ready", "ready_partial") }).Count)

Write-Host "==> build snc decision input package"
& $exe --build-snc-decision-input-package $postPlayPackagePath $decisionInputPackagePath
Assert-LastExitCodeOk -StepName "build snc decision input package"
if (-not (Test-Path -LiteralPath $decisionInputPackagePath)) {
    throw "SNC decision input package output is missing: $decisionInputPackagePath"
}
$decisionInputPackageText = Get-Content -Raw -LiteralPath $decisionInputPackagePath
$decisionInputPackageJson = $decisionInputPackageText | ConvertFrom-Json
$decisionInputPackageReadiness = [string]$decisionInputPackageJson.readiness
$decisionInputPackageReason = [string]$decisionInputPackageJson.reason
$decisionInputCount = [string]$decisionInputPackageJson.decision_input_count
$decisionInputBlockedEntryCount = [string]$decisionInputPackageJson.blocked_entry_count

Write-Host "==> build snc candidate decision package"
& $exe --build-snc-candidate-decision-package $decisionInputPackagePath $candidateDecisionPackagePath
Assert-LastExitCodeOk -StepName "build snc candidate decision package"
if (-not (Test-Path -LiteralPath $candidateDecisionPackagePath)) {
    throw "SNC candidate decision package output is missing: $candidateDecisionPackagePath"
}
$candidateDecisionPackageText = Get-Content -Raw -LiteralPath $candidateDecisionPackagePath
$candidateDecisionPackageJson = $candidateDecisionPackageText | ConvertFrom-Json
$candidateDecisionPackageReadiness = [string]$candidateDecisionPackageJson.readiness
$candidateDecisionPackageReason = [string]$candidateDecisionPackageJson.reason
$candidateDecisionCount = [string]$candidateDecisionPackageJson.candidate_decision_count
$candidateDecisionBlockedSourceEntryCount = [string]$candidateDecisionPackageJson.blocked_source_entry_count
$candidateDecisionValidatorPassed = [string]([bool]$candidateDecisionPackageJson.validator_passed).ToString().ToLowerInvariant()

$dslDraftReadiness = "not_attempted"
$dslDraftReason = "not_attempted"
$dslDraftRuleCount = "0"
$dslDraftEligibleCandidateCount = "0"
$dslDraftSkippedCandidateCount = "0"
$dslDraftValidatorPassed = "false"
$sncGeneratedOverlayStagingReadiness = "not_attempted"
$sncGeneratedOverlayStagingReason = "not_attempted"
$sncGeneratedOverlayStagingRuleCount = "0"
$sncGeneratedOverlayManifestVerified = "false"
$sncGeneratedOverlayPublishAllowed = "false"
$sncGeneratedOverlayDraftEligible = $false
foreach ($candidateDecision in @($candidateDecisionPackageJson.candidate_decisions)) {
    if ($null -eq $candidateDecision) {
        continue
    }

    $empireStateSummary = $candidateDecision.empire_state_summary
    if ($null -ne $empireStateSummary -and $empireStateSummary.parsed -eq $true) {
        $sncGeneratedOverlayDraftEligible = $true
        break
    }
}

if ($sncGeneratedOverlayDraftEligible) {
    Write-Host "==> build snc dsl draft package"
    & $exe --build-snc-dsl-draft-package $candidateDecisionPackagePath $dslDraftPath $dslDraftAuditPath
    Assert-LastExitCodeOk -StepName "build snc dsl draft package"
    if (-not (Test-Path -LiteralPath $dslDraftAuditPath)) {
        throw "SNC DSL draft audit output is missing: $dslDraftAuditPath"
    }
    if (-not (Test-Path -LiteralPath $dslDraftPath)) {
        throw "SNC DSL draft output is missing: $dslDraftPath"
    }

    $dslDraftAuditText = Get-Content -Raw -LiteralPath $dslDraftAuditPath
    $dslDraftAuditJson = $dslDraftAuditText | ConvertFrom-Json
    $dslDraftReadiness = [string]$dslDraftAuditJson.readiness
    $dslDraftReason = [string]$dslDraftAuditJson.reason
    $dslDraftRuleCount = [string]$dslDraftAuditJson.dsl_rule_count
    $dslDraftEligibleCandidateCount = [string]$dslDraftAuditJson.eligible_candidate_count
    $dslDraftSkippedCandidateCount = [string]$dslDraftAuditJson.skipped_candidate_count
    $dslDraftValidatorPassed = [string]([bool]$dslDraftAuditJson.validator_passed).ToString().ToLowerInvariant()

    Write-Host "==> stage snc generated overlay"
    & $exe --stage-snc-generated-overlay $dslDraftPath $sncGeneratedOverlayStagingDir $sncGeneratedOverlayStagingStatusPath
    Assert-LastExitCodeOk -StepName "stage snc generated overlay"
    if (-not (Test-Path -LiteralPath $sncGeneratedOverlayStagingStatusPath)) {
        throw "SNC generated overlay staging status output is missing: $sncGeneratedOverlayStagingStatusPath"
    }

    $sncGeneratedOverlayStagingStatusText = Get-Content -Raw -LiteralPath $sncGeneratedOverlayStagingStatusPath
    $sncGeneratedOverlayStagingStatusJson = $sncGeneratedOverlayStagingStatusText | ConvertFrom-Json
    $sncGeneratedOverlayStagingReadiness = [string]$sncGeneratedOverlayStagingStatusJson.readiness
    $sncGeneratedOverlayStagingReason = [string]$sncGeneratedOverlayStagingStatusJson.reason
    $sncGeneratedOverlayStagingRuleCount = [string]$sncGeneratedOverlayStagingStatusJson.dsl_rule_count
    $sncGeneratedOverlayManifestVerified = [string]([bool]$sncGeneratedOverlayStagingStatusJson.manifest_verified).ToString().ToLowerInvariant()
    $sncGeneratedOverlayPublishAllowed = [string]([bool]$sncGeneratedOverlayStagingStatusJson.publish_allowed).ToString().ToLowerInvariant()
} else {
    $dslDraftReadiness = "needs_identity"
    $dslDraftReason = "no candidate had enough parsed entry-point identity for a DSL draft"
    $sncGeneratedOverlayStagingReadiness = "needs_identity"
    $sncGeneratedOverlayStagingReason = "dsl draft not generated because parsed entry-point identity was missing"
}

$effectiveCampaignId = $CampaignId
$campaignIdSource = "explicit"
if ([string]::IsNullOrWhiteSpace($effectiveCampaignId)) {
    $decisionInputCampaignKeys = Get-UniqueNonEmptyValues (@($decisionInputPackageJson.decision_inputs | ForEach-Object { $_.campaign_key }))
    if ($decisionInputCampaignKeys.Count -eq 1) {
        $effectiveCampaignId = $decisionInputCampaignKeys[0]
        $campaignIdSource = "auto_decision_input_campaign_key"
    } elseif ($decisionInputCampaignKeys.Count -gt 1) {
        throw "CampaignId not provided and decision inputs span multiple campaign keys: $([string]::Join(', ', $decisionInputCampaignKeys))"
    } else {
        $postPlayCampaignKeys = Get-UniqueNonEmptyValues (@($postPlayPackageJson.campaigns | ForEach-Object { $_.campaign_key }))
        if ($postPlayCampaignKeys.Count -eq 1) {
            $effectiveCampaignId = $postPlayCampaignKeys[0]
            $campaignIdSource = "auto_post_play_campaign_key"
        } elseif ($postPlayCampaignKeys.Count -gt 1) {
            throw "CampaignId not provided and post-play package spans multiple campaigns: $([string]::Join(', ', $postPlayCampaignKeys))"
        } else {
            throw "CampaignId not provided and no bounded campaign key could be derived from post-play artifacts."
        }
    }
}

$effectiveEmpireId = $EmpireId
$empireIdSource = "explicit"
if ([string]::IsNullOrWhiteSpace($effectiveEmpireId)) {
    $decisionInputPlayerCountryIds = Get-UniqueNonEmptyValues (@($decisionInputPackageJson.decision_inputs | ForEach-Object { $_.player_country_id }))
    if ($decisionInputPlayerCountryIds.Count -eq 1) {
        $effectiveEmpireId = "player_country_" + (Convert-ToStableIdentityToken -Value $decisionInputPlayerCountryIds[0] -Fallback "player_country")
        $empireIdSource = "auto_player_country_id"
    } elseif ($decisionInputPlayerCountryIds.Count -gt 1) {
        throw "EmpireId not provided and decision inputs span multiple player_country_id values: $([string]::Join(', ', $decisionInputPlayerCountryIds))"
    } else {
        $decisionInputEmpireNames = Get-UniqueNonEmptyValues (@($decisionInputPackageJson.decision_inputs | ForEach-Object { $_.empire_name }))
        if ($decisionInputEmpireNames.Count -eq 1) {
            $effectiveEmpireId = "empire_" + (Convert-ToStableIdentityToken -Value $decisionInputEmpireNames[0] -Fallback "empire")
            $empireIdSource = "auto_empire_name"
        } elseif ($decisionInputEmpireNames.Count -gt 1) {
            throw "EmpireId not provided and decision inputs span multiple empire names: $([string]::Join(', ', $decisionInputEmpireNames))"
        } else {
            throw "EmpireId not provided and no bounded player empire identity could be derived from decision inputs."
        }
    }
}

$effectiveDslInputFull = ""
$dslInputSource = "explicit"
if (-not [string]::IsNullOrWhiteSpace($DslInput)) {
    $effectiveDslInputFull = [System.IO.Path]::GetFullPath($DslInput)
    if (-not (Test-Path -LiteralPath $effectiveDslInputFull)) {
        throw "DslInput does not exist: $effectiveDslInputFull"
    }
} elseif (Test-Path -LiteralPath $dslDraftPath) {
    $effectiveDslInputFull = [System.IO.Path]::GetFullPath($dslDraftPath)
    $dslInputSource = "auto_generated_dsl_draft"
} else {
    throw "DslInput not provided and no validated SNC DSL draft is available for offline spine."
}

Write-Host "==> run offline spine"
& $exe --run-offline-spine $sessionArchiveDir $effectiveCampaignId $effectiveEmpireId $effectiveDslInputFull $workDirFull $overlayOutputDirFull $statusOutputJsonFull
Assert-LastExitCodeOk -StepName "run offline spine"
$seasonDeltaLedgerPath = Join-Path $workDirFull "season_delta_ledger.json"
$empireBriefPath = Join-Path $workDirFull "empire_brief.json"
if (-not (Test-Path -LiteralPath $seasonDeltaLedgerPath)) {
    throw "Offline spine output is missing season delta ledger: $seasonDeltaLedgerPath"
}
if (-not (Test-Path -LiteralPath $empireBriefPath)) {
    throw "Offline spine output is missing empire brief: $empireBriefPath"
}

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
$mpExportCampaignIdMismatchWarning = "false"
$mpExportOverlayVersionMismatchWarning = "false"
$mpExportGameVersionMismatchWarning = "false"
$mpExportModVersionMismatchWarning = "false"
$mpExportManifestHashMismatchWarning = "false"
$mpPackageMismatchWarningState = "not_exported"
$mpPackageMismatchWarningReason = "mp_export_not_requested"
$mpPackageMismatchWarningCodes = @()
$mpPackageZipState = "not_exported"
$mpPackageZipReason = "mp_export_not_requested"
$mpPackageZipPath = ""
$mpPackageZipSha256 = ""
$mpPackageZipBytes = ""
$sncStatusWithMpReadiness = "not_exported"
$statusWithMpSnapshotPathForOutput = ""
if ($ExportMpPackage) {
    if (Test-Path -LiteralPath $mpPackageOutputDirFull) {
        throw "MpPackageOutputDir must not already exist: $mpPackageOutputDirFull"
    }
    Write-Host "==> export mp overlay package"
    $mpExportLines = & $exe --export-mp-overlay-package $overlayOutputDirFull $effectiveCampaignId $MpOverlayVersion $MpGameVersion $MpModVersion $mpPackageOutputDirFull
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
    $mpExportCampaignIdMismatchWarning = (Contains-Value -Values $mpExportWarningCodes -Needle "package_campaign_id_mismatch").ToString().ToLowerInvariant()
    $mpExportOverlayVersionMismatchWarning = (Contains-Value -Values $mpExportWarningCodes -Needle "package_overlay_version_mismatch").ToString().ToLowerInvariant()
    $mpExportGameVersionMismatchWarning = (Contains-Value -Values $mpExportWarningCodes -Needle "package_game_version_mismatch").ToString().ToLowerInvariant()
    $mpExportModVersionMismatchWarning = (Contains-Value -Values $mpExportWarningCodes -Needle "package_mod_version_mismatch").ToString().ToLowerInvariant()
    $mpExportManifestHashMismatchWarning = (Contains-Value -Values $mpExportWarningCodes -Needle "package_manifest_hash_mismatch").ToString().ToLowerInvariant()
    $mpPackageMismatchSummary = Get-MpPackageMismatchSummary `
        -IdentityMismatchWarning $mpExportIdentityMismatchWarning `
        -CampaignIdMismatchWarning $mpExportCampaignIdMismatchWarning `
        -OverlayVersionMismatchWarning $mpExportOverlayVersionMismatchWarning `
        -GameVersionMismatchWarning $mpExportGameVersionMismatchWarning `
        -ModVersionMismatchWarning $mpExportModVersionMismatchWarning `
        -ManifestHashMismatchWarning $mpExportManifestHashMismatchWarning
    $mpPackageMismatchWarningState = [string]$mpPackageMismatchSummary.state
    $mpPackageMismatchWarningReason = [string]$mpPackageMismatchSummary.reason
    $mpPackageMismatchWarningCodes = @($mpPackageMismatchSummary.codes)

    $mpPackageZipPath = Join-Path $defaultRunRoot "mp_overlay_package.zip"
    if (Test-Path -LiteralPath $mpPackageZipPath) {
        Remove-Item -LiteralPath $mpPackageZipPath -Force
    }
    Compress-Archive -Path (Join-Path $mpPackageOutputDirFull "*") -DestinationPath $mpPackageZipPath -Force
    if (-not (Test-Path -LiteralPath $mpPackageZipPath)) {
        throw "MP package zip export did not create expected file: $mpPackageZipPath"
    }
    $mpPackageZipHash = Get-FileHash -Algorithm SHA256 -LiteralPath $mpPackageZipPath
    $mpPackageZipSha256 = [string]$mpPackageZipHash.Hash
    $mpPackageZipBytes = [string](Get-Item -LiteralPath $mpPackageZipPath).Length
    $mpPackageZipState = "ready"
    $mpPackageZipReason = "zip_created"

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
    $statusWithMpSnapshotPathForOutput = $statusWithMpOutputJsonFull
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
$statusCenterState = ""
$statusCenterReason = ""
$statusCenterSummaryText = ""
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
if ($null -ne $statusJson.status_center) {
    if ($null -ne $statusJson.status_center.state) {
        $statusCenterState = [string]$statusJson.status_center.state
    }
    if ($null -ne $statusJson.status_center.reason) {
        $statusCenterReason = [string]$statusJson.status_center.reason
    }
}
if ($null -ne $statusJson.status_center_summary_text) {
    $statusCenterSummaryText = [string]$statusJson.status_center_summary_text
}
if ([string]::IsNullOrWhiteSpace($gameplayAcceptanceState)) {
    throw "Offline spine status snapshot is missing gameplay_acceptance_status.state."
}
if ([string]::IsNullOrWhiteSpace($gameplayAcceptanceReason)) {
    throw "Offline spine status snapshot is missing gameplay_acceptance_status.reason."
}
if ([string]::IsNullOrWhiteSpace($statusCenterState)) {
    throw "Offline spine status snapshot is missing status_center.state."
}
if ([string]::IsNullOrWhiteSpace($statusCenterReason)) {
    throw "Offline spine status snapshot is missing status_center.reason."
}

Write-Host "real_session_v0_loop_ok=true"
Write-Host ("real_session_v0_loop_session_id=" + $SessionId)
Write-Host ("real_session_v0_loop_campaign_id=" + $effectiveCampaignId)
Write-Host ("real_session_v0_loop_campaign_id_source=" + $campaignIdSource)
Write-Host ("real_session_v0_loop_empire_id=" + $effectiveEmpireId)
Write-Host ("real_session_v0_loop_empire_id_source=" + $empireIdSource)
Write-Host ("real_session_v0_loop_dsl_input_path=" + $effectiveDslInputFull)
Write-Host ("real_session_v0_loop_dsl_input_source=" + $dslInputSource)
Write-Host ("real_session_v0_loop_session_archive_dir=" + $sessionArchiveDir)
Write-Host ("real_session_v0_loop_archive_summary_path=" + $archiveSummaryPath)
Write-Host ("real_session_v0_loop_season_delta_ledger_path=" + $seasonDeltaLedgerPath)
Write-Host ("real_session_v0_loop_empire_brief_path=" + $empireBriefPath)
Write-Host ("real_session_v0_loop_archive_copied_save_count=" + $archiveCopiedSaveCount)
Write-Host ("real_session_v0_loop_entry_point_analysis_path=" + $entryPointAnalysisPath)
Write-Host ("real_session_v0_loop_entry_point_readiness=" + $entryPointReadiness)
Write-Host ("real_session_v0_loop_entry_point_reason=" + $entryPointReason)
Write-Host ("real_session_v0_loop_entry_point_count=" + $entryPointCount)
Write-Host ("real_session_v0_loop_entry_point_branch_ambiguity=" + $entryPointBranchAmbiguity)
Write-Host ("real_session_v0_loop_post_play_package_path=" + $postPlayPackagePath)
Write-Host ("real_session_v0_loop_post_play_package_readiness=" + $postPlayPackageReadiness)
Write-Host ("real_session_v0_loop_post_play_package_reason=" + $postPlayPackageReason)
Write-Host ("real_session_v0_loop_post_play_decision_ready_entry_count=" + $postPlayDecisionReadyEntryCount)
Write-Host ("real_session_v0_loop_post_play_campaign_count=" + $postPlayCampaignCount)
Write-Host ("real_session_v0_loop_post_play_ready_campaign_count=" + $postPlayReadyCampaignCount)
Write-Host ("real_session_v0_loop_post_play_partial_campaign_count=" + $postPlayPartialCampaignCount)
Write-Host ("real_session_v0_loop_post_play_blocked_campaign_count=" + $postPlayBlockedCampaignCount)
Write-Host ("real_session_v0_loop_decision_input_package_path=" + $decisionInputPackagePath)
Write-Host ("real_session_v0_loop_decision_input_package_readiness=" + $decisionInputPackageReadiness)
Write-Host ("real_session_v0_loop_decision_input_package_reason=" + $decisionInputPackageReason)
Write-Host ("real_session_v0_loop_decision_input_count=" + $decisionInputCount)
Write-Host ("real_session_v0_loop_decision_input_blocked_entry_count=" + $decisionInputBlockedEntryCount)
Write-Host ("real_session_v0_loop_candidate_decision_package_path=" + $candidateDecisionPackagePath)
Write-Host ("real_session_v0_loop_candidate_decision_package_readiness=" + $candidateDecisionPackageReadiness)
Write-Host ("real_session_v0_loop_candidate_decision_package_reason=" + $candidateDecisionPackageReason)
Write-Host ("real_session_v0_loop_candidate_decision_count=" + $candidateDecisionCount)
Write-Host ("real_session_v0_loop_candidate_decision_blocked_source_entry_count=" + $candidateDecisionBlockedSourceEntryCount)
Write-Host ("real_session_v0_loop_candidate_decision_validator_passed=" + $candidateDecisionValidatorPassed)
Write-Host ("real_session_v0_loop_dsl_draft_audit_path=" + $dslDraftAuditPath)
Write-Host ("real_session_v0_loop_dsl_draft_readiness=" + $dslDraftReadiness)
Write-Host ("real_session_v0_loop_dsl_draft_reason=" + $dslDraftReason)
Write-Host ("real_session_v0_loop_dsl_draft_rule_count=" + $dslDraftRuleCount)
Write-Host ("real_session_v0_loop_dsl_draft_eligible_candidate_count=" + $dslDraftEligibleCandidateCount)
Write-Host ("real_session_v0_loop_dsl_draft_skipped_candidate_count=" + $dslDraftSkippedCandidateCount)
Write-Host ("real_session_v0_loop_dsl_draft_validator_passed=" + $dslDraftValidatorPassed)
Write-Host ("real_session_v0_loop_snc_generated_overlay_staging_dir=" + $sncGeneratedOverlayStagingDir)
Write-Host ("real_session_v0_loop_snc_generated_overlay_staging_status_path=" + $sncGeneratedOverlayStagingStatusPath)
Write-Host ("real_session_v0_loop_snc_generated_overlay_staging_readiness=" + $sncGeneratedOverlayStagingReadiness)
Write-Host ("real_session_v0_loop_snc_generated_overlay_staging_reason=" + $sncGeneratedOverlayStagingReason)
Write-Host ("real_session_v0_loop_snc_generated_overlay_staging_rule_count=" + $sncGeneratedOverlayStagingRuleCount)
Write-Host ("real_session_v0_loop_snc_generated_overlay_manifest_verified=" + $sncGeneratedOverlayManifestVerified)
Write-Host ("real_session_v0_loop_snc_generated_overlay_publish_allowed=" + $sncGeneratedOverlayPublishAllowed)
if (Test-Path -LiteralPath $dslDraftPath) {
    Write-Host ("real_session_v0_loop_dsl_draft_path=" + $dslDraftPath)
}
if (-not [string]::IsNullOrWhiteSpace($archiveLastArchivedPath)) {
    Write-Host ("real_session_v0_loop_archive_last_archived_path=" + $archiveLastArchivedPath)
}
Write-Host ("real_session_v0_loop_generated_overlay_dir=" + $overlayOutputDirFull)
Write-Host ("real_session_v0_loop_status_snapshot_path=" + $statusOutputJsonFull)
Write-Host ("real_session_v0_loop_gameplay_acceptance_state=" + $gameplayAcceptanceState)
Write-Host ("real_session_v0_loop_gameplay_acceptance_reason=" + $gameplayAcceptanceReason)
Write-Host ("real_session_v0_loop_status_center_state=" + $statusCenterState)
Write-Host ("real_session_v0_loop_status_center_reason=" + $statusCenterReason)
if (-not [string]::IsNullOrWhiteSpace($statusCenterSummaryText)) {
    Write-Host ("real_session_v0_loop_status_center_summary_present=true")
    Write-Host ("real_session_v0_loop_status_center_summary_text=" + $statusCenterSummaryText)
}
if (-not [string]::IsNullOrWhiteSpace($gameplayAcceptancePath)) {
    Write-Host ("real_session_v0_loop_gameplay_acceptance_path=" + $gameplayAcceptancePath)
}
Write-Host ("real_session_v0_loop_compare_previous_session_dir_hint=dist\\real_session_v0_loop\\<previous_session_id>")
$compareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "dist\real_session_v0_loop\<previous_session_id>" "' + $defaultRunRoot + '" "dist\private_reports\real_session_v0_compare_' + $SessionId + '.json"'
Write-Host ("real_session_v0_loop_compare_command_hint=" + $compareCommandHint)
Write-Host "real_session_v0_loop_trend_sessions_root_hint=dist\\real_session_v0_loop"
$trendCommandHint = 'cmd /c tools\analyze_real_session_v0_trend.cmd "dist\real_session_v0_loop" "dist\private_reports\real_session_v0_trend.json"'
Write-Host ("real_session_v0_loop_trend_command_hint=" + $trendCommandHint)
$nextSessionCampaignIdHint = if ($campaignIdSource -eq "explicit") { $CampaignId } else { "" }
$nextSessionEmpireIdHint = if ($empireIdSource -eq "explicit") { $EmpireId } else { "" }
$nextSessionDslInputHint = if ($dslInputSource -eq "explicit") { $effectiveDslInputFull } else { "" }
$nextSessionCommandHint = 'cmd /c tools\run_real_session_v0_loop.cmd "' + $saveRootFull + '" "' + $archiveRootFull + '" "' + $nextSessionCampaignIdHint + '" "' + $nextSessionEmpireIdHint + '" "' + $nextSessionDslInputHint + '"'
if ($ExportMpPackage) {
    $nextSessionCommandHint += " -ExportMpPackage"
}
if ($EmitTrendSummary) {
    $nextSessionCommandHint += " -EmitTrendSummary"
}
$nextSessionCommandHint += ' -PreviousSessionDirForCompare "' + $defaultRunRoot + '"'
Write-Host ("real_session_v0_loop_next_session_compare_baseline_dir=" + $defaultRunRoot)
Write-Host ("real_session_v0_loop_next_session_command_hint=" + $nextSessionCommandHint)
Write-Host ("real_session_v0_loop_run_id=" + $realSessionLoopRunId)
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
    $compareArchiveSaveCountCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_verified_archive_save_count_current"
    $compareArchiveSaveCountPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_verified_archive_save_count_previous"
    $compareArchiveSaveCountDelta = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_verified_archive_save_count_delta"
    $compareArchiveSaveCountChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_verified_archive_save_count_changed"
    $compareGameplayAcceptanceStateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_current"
    $compareGameplayAcceptanceStatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_previous"
    $compareGameplayAcceptanceStateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_state_changed"
    $compareGameplayAcceptanceReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_current"
    $compareGameplayAcceptanceReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_previous"
    $compareGameplayAcceptanceReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_gameplay_acceptance_reason_changed"
    $compareStatusCenterStateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_state_current"
    $compareStatusCenterStatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_state_previous"
    $compareStatusCenterStateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_state_changed"
    $compareStatusCenterReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_reason_current"
    $compareStatusCenterReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_reason_previous"
    $compareStatusCenterReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_reason_changed"
    $compareStatusCenterSummaryTextCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_summary_text_current"
    $compareStatusCenterSummaryTextPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_summary_text_previous"
    $compareStatusCenterSummaryTextChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_status_center_summary_text_changed"
    $compareSaveRootResolutionCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_resolution_current"
    $compareSaveRootResolutionPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_resolution_previous"
    $compareSaveRootResolutionChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_resolution_changed"
    $compareSaveRootSourceCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_source_current"
    $compareSaveRootSourcePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_source_previous"
    $compareSaveRootSourceChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_source_changed"
    $compareSaveRootPathCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_path_current"
    $compareSaveRootPathPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_path_previous"
    $compareSaveRootPathChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_path_changed"
    $compareSaveRootCampaignCountCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_campaign_count_current"
    $compareSaveRootCampaignCountPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_campaign_count_previous"
    $compareSaveRootCampaignCountChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_campaign_count_changed"
    $compareSaveRootSaveFileCountCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_save_file_count_current"
    $compareSaveRootSaveFileCountPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_save_file_count_previous"
    $compareSaveRootSaveFileCountChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_save_file_count_changed"
    $compareSaveRootAutosaveAnchorCountCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_autosave_anchor_count_current"
    $compareSaveRootAutosaveAnchorCountPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_autosave_anchor_count_previous"
    $compareSaveRootAutosaveAnchorCountChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_save_root_autosave_anchor_count_changed"
    $compareNextActionCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_current"
    $compareNextActionPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_previous"
    $compareNextActionChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_changed"
    $compareNextActionReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_reason_current"
    $compareNextActionReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_reason_previous"
    $compareNextActionReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_reason_changed"
    $compareNextActionCommandHintCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_current"
    $compareNextActionCommandHintPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_previous"
    $compareNextActionCommandHintChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_changed"
    $compareNextActionCommandHintSourceCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_source_current"
    $compareNextActionCommandHintSourcePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_source_previous"
    $compareNextActionCommandHintSourceChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_next_action_command_hint_source_changed"
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
    $compareMpGameVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_game_version_mismatch_warning_current"
    $compareMpGameVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_game_version_mismatch_warning_previous"
    $compareMpGameVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_game_version_mismatch_warning_changed"
    $compareMpCampaignIdMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_campaign_id_mismatch_warning_current"
    $compareMpCampaignIdMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_campaign_id_mismatch_warning_previous"
    $compareMpCampaignIdMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_campaign_id_mismatch_warning_changed"
    $compareMpOverlayVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_overlay_version_mismatch_warning_current"
    $compareMpOverlayVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_overlay_version_mismatch_warning_previous"
    $compareMpOverlayVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_overlay_version_mismatch_warning_changed"
    $compareMpModVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mod_version_mismatch_warning_current"
    $compareMpModVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mod_version_mismatch_warning_previous"
    $compareMpModVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mod_version_mismatch_warning_changed"
    $compareMpManifestHashMismatchWarningCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_mismatch_warning_current"
    $compareMpManifestHashMismatchWarningPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_mismatch_warning_previous"
    $compareMpManifestHashMismatchWarningChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_mismatch_warning_changed"
    $compareMpMismatchWarningStateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_state_current"
    $compareMpMismatchWarningStatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_state_previous"
    $compareMpMismatchWarningStateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_state_changed"
    $compareMpMismatchWarningReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_reason_current"
    $compareMpMismatchWarningReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_reason_previous"
    $compareMpMismatchWarningReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_reason_changed"
    $compareMpMismatchWarningCodesChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_codes_changed"
    $compareMpMismatchWarningCodesPrevious = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_code_previous"
    $compareMpMismatchWarningCodesCurrent = Get-KeyValueLineValues -Lines $compareLines -Key "real_session_v0_compare_mp_mismatch_warning_code_current"
    $compareMpManifestHashCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_current"
    $compareMpManifestHashPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_previous"
    $compareMpManifestHashChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_manifest_hash_changed"
    $compareMpStatusSnapshotPresentCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_status_snapshot_present_current"
    $compareMpStatusSnapshotPresentPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_status_snapshot_present_previous"
    $compareMpStatusSnapshotPresentChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_status_snapshot_present_changed"
    $compareMpPackageOutputDirCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_current"
    $compareMpPackageOutputDirPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_previous"
    $compareMpPackageOutputDirChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_output_dir_changed"
    $compareMpPackageZipStateCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_state_current"
    $compareMpPackageZipStatePrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_state_previous"
    $compareMpPackageZipStateChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_state_changed"
    $compareMpPackageZipReasonCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_reason_current"
    $compareMpPackageZipReasonPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_reason_previous"
    $compareMpPackageZipReasonChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_reason_changed"
    $compareMpPackageZipSha256Current = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_sha256_current"
    $compareMpPackageZipSha256Previous = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_sha256_previous"
    $compareMpPackageZipSha256Changed = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_sha256_changed"
    $compareMpPackageZipPathCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_path_current"
    $compareMpPackageZipPathPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_path_previous"
    $compareMpPackageZipPathChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_path_changed"
    $compareMpPackageZipBytesCurrent = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_bytes_current"
    $compareMpPackageZipBytesPrevious = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_bytes_previous"
    $compareMpPackageZipBytesChanged = Get-KeyValueLineValue -Lines $compareLines -Key "real_session_v0_compare_mp_package_zip_bytes_changed"
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
    if ([string]::IsNullOrWhiteSpace($compareArchiveSaveCountCurrent)) {
        throw "Compare output is missing real_session_v0_compare_verified_archive_save_count_current."
    }
    if ([string]::IsNullOrWhiteSpace($compareArchiveSaveCountPrevious)) {
        throw "Compare output is missing real_session_v0_compare_verified_archive_save_count_previous."
    }
    if ([string]::IsNullOrWhiteSpace($compareArchiveSaveCountDelta)) {
        throw "Compare output is missing real_session_v0_compare_verified_archive_save_count_delta."
    }
    if ([string]::IsNullOrWhiteSpace($compareArchiveSaveCountChanged)) {
        throw "Compare output is missing real_session_v0_compare_verified_archive_save_count_changed."
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
    Write-Host ("real_session_v0_loop_compare_auto_verified_archive_save_count_current=" + $compareArchiveSaveCountCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_verified_archive_save_count_previous=" + $compareArchiveSaveCountPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_verified_archive_save_count_delta=" + $compareArchiveSaveCountDelta)
    Write-Host ("real_session_v0_loop_compare_auto_verified_archive_save_count_changed=" + $compareArchiveSaveCountChanged)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_current=" + $compareGameplayAcceptanceStateCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_previous=" + $compareGameplayAcceptanceStatePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_state_changed=" + $compareGameplayAcceptanceStateChanged)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_current=" + $compareGameplayAcceptanceReasonCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_previous=" + $compareGameplayAcceptanceReasonPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_gameplay_acceptance_reason_changed=" + $compareGameplayAcceptanceReasonChanged)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_state_current=" + $compareStatusCenterStateCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_state_previous=" + $compareStatusCenterStatePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_state_changed=" + $compareStatusCenterStateChanged)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_reason_current=" + $compareStatusCenterReasonCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_reason_previous=" + $compareStatusCenterReasonPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_reason_changed=" + $compareStatusCenterReasonChanged)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_summary_text_current=" + $compareStatusCenterSummaryTextCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_summary_text_previous=" + $compareStatusCenterSummaryTextPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_status_center_summary_text_changed=" + $compareStatusCenterSummaryTextChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_resolution_current=" + $compareSaveRootResolutionCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_resolution_previous=" + $compareSaveRootResolutionPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_resolution_changed=" + $compareSaveRootResolutionChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_source_current=" + $compareSaveRootSourceCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_source_previous=" + $compareSaveRootSourcePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_source_changed=" + $compareSaveRootSourceChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_path_current=" + $compareSaveRootPathCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_path_previous=" + $compareSaveRootPathPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_path_changed=" + $compareSaveRootPathChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_campaign_count_current=" + $compareSaveRootCampaignCountCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_campaign_count_previous=" + $compareSaveRootCampaignCountPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_campaign_count_changed=" + $compareSaveRootCampaignCountChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_save_file_count_current=" + $compareSaveRootSaveFileCountCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_save_file_count_previous=" + $compareSaveRootSaveFileCountPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_save_file_count_changed=" + $compareSaveRootSaveFileCountChanged)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_autosave_anchor_count_current=" + $compareSaveRootAutosaveAnchorCountCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_autosave_anchor_count_previous=" + $compareSaveRootAutosaveAnchorCountPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_save_root_autosave_anchor_count_changed=" + $compareSaveRootAutosaveAnchorCountChanged)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_current=" + $compareNextActionCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_previous=" + $compareNextActionPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_changed=" + $compareNextActionChanged)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_reason_current=" + $compareNextActionReasonCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_reason_previous=" + $compareNextActionReasonPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_reason_changed=" + $compareNextActionReasonChanged)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_current=" + $compareNextActionCommandHintCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_previous=" + $compareNextActionCommandHintPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_changed=" + $compareNextActionCommandHintChanged)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_source_current=" + $compareNextActionCommandHintSourceCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_source_previous=" + $compareNextActionCommandHintSourcePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_next_action_command_hint_source_changed=" + $compareNextActionCommandHintSourceChanged)
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
    Write-Host ("real_session_v0_loop_compare_auto_mp_status_snapshot_present_previous=" + $compareMpStatusSnapshotPresentPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_status_snapshot_present_current=" + $compareMpStatusSnapshotPresentCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_status_snapshot_present_changed=" + $compareMpStatusSnapshotPresentChanged)
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_previous=" + $compareMpPackageOutputDirPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_current=" + $compareMpPackageOutputDirCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpPackageOutputDirChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_package_output_dir_changed=" + $compareMpPackageOutputDirChanged)
    }
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_state_previous=" + $compareMpPackageZipStatePrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_state_current=" + $compareMpPackageZipStateCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_state_changed=" + $compareMpPackageZipStateChanged)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_reason_previous=" + $compareMpPackageZipReasonPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_reason_current=" + $compareMpPackageZipReasonCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_reason_changed=" + $compareMpPackageZipReasonChanged)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_sha256_previous=" + $compareMpPackageZipSha256Previous)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_sha256_current=" + $compareMpPackageZipSha256Current)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_sha256_changed=" + $compareMpPackageZipSha256Changed)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_path_previous=" + $compareMpPackageZipPathPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_path_current=" + $compareMpPackageZipPathCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_path_changed=" + $compareMpPackageZipPathChanged)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_bytes_previous=" + $compareMpPackageZipBytesPrevious)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_bytes_current=" + $compareMpPackageZipBytesCurrent)
    Write-Host ("real_session_v0_loop_compare_auto_mp_package_zip_bytes_changed=" + $compareMpPackageZipBytesChanged)
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
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_game_version_mismatch_warning_current=" + $compareMpGameVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_game_version_mismatch_warning_previous=" + $compareMpGameVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpGameVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_game_version_mismatch_warning_changed=" + $compareMpGameVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_campaign_id_mismatch_warning_current=" + $compareMpCampaignIdMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_campaign_id_mismatch_warning_previous=" + $compareMpCampaignIdMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpCampaignIdMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_campaign_id_mismatch_warning_changed=" + $compareMpCampaignIdMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_overlay_version_mismatch_warning_current=" + $compareMpOverlayVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_overlay_version_mismatch_warning_previous=" + $compareMpOverlayVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpOverlayVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_overlay_version_mismatch_warning_changed=" + $compareMpOverlayVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mod_version_mismatch_warning_current=" + $compareMpModVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mod_version_mismatch_warning_previous=" + $compareMpModVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpModVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mod_version_mismatch_warning_changed=" + $compareMpModVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_mismatch_warning_current=" + $compareMpManifestHashMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_mismatch_warning_previous=" + $compareMpManifestHashMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpManifestHashMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_manifest_hash_mismatch_warning_changed=" + $compareMpManifestHashMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStateCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_state_current=" + $compareMpMismatchWarningStateCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStatePrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_state_previous=" + $compareMpMismatchWarningStatePrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningStateChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_state_changed=" + $compareMpMismatchWarningStateChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonCurrent)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_reason_current=" + $compareMpMismatchWarningReasonCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonPrevious)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_reason_previous=" + $compareMpMismatchWarningReasonPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningReasonChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_reason_changed=" + $compareMpMismatchWarningReasonChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($compareMpMismatchWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_codes_changed=" + $compareMpMismatchWarningCodesChanged)
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
    foreach ($warningCode in $compareMpMismatchWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $compareMpMismatchWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_compare_auto_mp_mismatch_warning_code_current=" + $warningCode)
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
    Write-Host ("real_session_v0_loop_mp_package_campaign_id_mismatch_warning=" + $mpExportCampaignIdMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_overlay_version_mismatch_warning=" + $mpExportOverlayVersionMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_game_version_mismatch_warning=" + $mpExportGameVersionMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_mod_version_mismatch_warning=" + $mpExportModVersionMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_manifest_hash_mismatch_warning=" + $mpExportManifestHashMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_mismatch_warning_state=" + $mpPackageMismatchWarningState)
    Write-Host ("real_session_v0_loop_mp_package_mismatch_warning_reason=" + $mpPackageMismatchWarningReason)
    foreach ($warningCode in $mpPackageMismatchWarningCodes) {
        Write-Host ("real_session_v0_loop_mp_package_mismatch_warning_code=" + $warningCode)
    }
    foreach ($warningCode in $mpExportWarningCodes) {
        Write-Host ("real_session_v0_loop_mp_package_warning_code=" + $warningCode)
    }
    Write-Host ("real_session_v0_loop_mp_package_zip_state=" + $mpPackageZipState)
    Write-Host ("real_session_v0_loop_mp_package_zip_reason=" + $mpPackageZipReason)
    Write-Host ("real_session_v0_loop_mp_package_zip_path=" + $mpPackageZipPath)
    Write-Host ("real_session_v0_loop_mp_package_zip_sha256=" + $mpPackageZipSha256)
    Write-Host ("real_session_v0_loop_mp_package_zip_bytes=" + $mpPackageZipBytes)
} else {
    Write-Host ("real_session_v0_loop_mp_package_mismatch_warning_state=" + $mpPackageMismatchWarningState)
    Write-Host ("real_session_v0_loop_mp_package_mismatch_warning_reason=" + $mpPackageMismatchWarningReason)
    Write-Host ("real_session_v0_loop_mp_package_zip_state=" + $mpPackageZipState)
    Write-Host ("real_session_v0_loop_mp_package_zip_reason=" + $mpPackageZipReason)
}
Write-Host ("real_session_v0_loop_status_snapshot_with_mp_path=" + $statusWithMpSnapshotPathForOutput)
Write-Host ("real_session_v0_loop_status_snapshot_with_mp_readiness=" + $sncStatusWithMpReadiness)
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
    $trendStatusCenterStateCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_state_current"
    $trendStatusCenterStatePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_state_previous"
    $trendStatusCenterStateChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_state_changed"
    $trendStatusCenterReasonCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_reason_current"
    $trendStatusCenterReasonPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_reason_previous"
    $trendStatusCenterReasonChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_reason_changed"
    $trendStatusCenterSummaryTextCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_summary_text_current"
    $trendStatusCenterSummaryTextPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_summary_text_previous"
    $trendStatusCenterSummaryTextChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_status_center_summary_text_changed"
    $trendSaveRootResolutionCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_resolution_current"
    $trendSaveRootResolutionPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_resolution_previous"
    $trendSaveRootResolutionChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_resolution_changed"
    $trendSaveRootSourceCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_source_current"
    $trendSaveRootSourcePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_source_previous"
    $trendSaveRootSourceChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_source_changed"
    $trendSaveRootPathCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_path_current"
    $trendSaveRootPathPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_path_previous"
    $trendSaveRootPathChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_path_changed"
    $trendSaveRootCampaignCountCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_campaign_count_current"
    $trendSaveRootCampaignCountPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_campaign_count_previous"
    $trendSaveRootCampaignCountChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_campaign_count_changed"
    $trendSaveRootSaveFileCountCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_save_file_count_current"
    $trendSaveRootSaveFileCountPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_save_file_count_previous"
    $trendSaveRootSaveFileCountChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_save_file_count_changed"
    $trendSaveRootAutosaveAnchorCountCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_autosave_anchor_count_current"
    $trendSaveRootAutosaveAnchorCountPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_autosave_anchor_count_previous"
    $trendSaveRootAutosaveAnchorCountChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_save_root_autosave_anchor_count_changed"
    $trendNextActionCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_current"
    $trendNextActionPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_previous"
    $trendNextActionChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_changed"
    $trendNextActionReasonCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_reason_current"
    $trendNextActionReasonPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_reason_previous"
    $trendNextActionReasonChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_reason_changed"
    $trendNextActionCommandHintCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_current"
    $trendNextActionCommandHintPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_previous"
    $trendNextActionCommandHintChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_changed"
    $trendNextActionCommandHintSourceCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_source_current"
    $trendNextActionCommandHintSourcePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_source_previous"
    $trendNextActionCommandHintSourceChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_next_action_command_hint_source_changed"
    $trendMpWarningCountCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_count_current"
    $trendMpWarningCountDelta = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_count_delta"
    $trendMpWarningCodesChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_warning_codes_changed"
    $trendMpManifestHashCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_current"
    $trendMpManifestHashPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_previous"
    $trendMpManifestHashChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_changed"
    $trendMpPackageOutputDirCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_current"
    $trendMpPackageOutputDirPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_previous"
    $trendMpPackageOutputDirChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_output_dir_changed"
    $trendMpPackageZipStateCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_state_current"
    $trendMpPackageZipStatePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_state_previous"
    $trendMpPackageZipStateChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_state_changed"
    $trendMpPackageZipReasonCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_reason_current"
    $trendMpPackageZipReasonPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_reason_previous"
    $trendMpPackageZipReasonChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_reason_changed"
    $trendMpPackageZipSha256Current = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_sha256_current"
    $trendMpPackageZipSha256Previous = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_sha256_previous"
    $trendMpPackageZipSha256Changed = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_sha256_changed"
    $trendMpPackageZipPathCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_path_current"
    $trendMpPackageZipPathPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_path_previous"
    $trendMpPackageZipPathChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_path_changed"
    $trendMpPackageZipBytesCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_bytes_current"
    $trendMpPackageZipBytesPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_bytes_previous"
    $trendMpPackageZipBytesChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_package_zip_bytes_changed"
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
    $trendMpGameVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_game_version_mismatch_warning_current"
    $trendMpGameVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_game_version_mismatch_warning_previous"
    $trendMpGameVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_game_version_mismatch_warning_changed"
    $trendMpCampaignIdMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_campaign_id_mismatch_warning_current"
    $trendMpCampaignIdMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_campaign_id_mismatch_warning_previous"
    $trendMpCampaignIdMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_campaign_id_mismatch_warning_changed"
    $trendMpOverlayVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_overlay_version_mismatch_warning_current"
    $trendMpOverlayVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_overlay_version_mismatch_warning_previous"
    $trendMpOverlayVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_overlay_version_mismatch_warning_changed"
    $trendMpModVersionMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mod_version_mismatch_warning_current"
    $trendMpModVersionMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mod_version_mismatch_warning_previous"
    $trendMpModVersionMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mod_version_mismatch_warning_changed"
    $trendMpManifestHashMismatchWarningCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_mismatch_warning_current"
    $trendMpManifestHashMismatchWarningPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_mismatch_warning_previous"
    $trendMpManifestHashMismatchWarningChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_manifest_hash_mismatch_warning_changed"
    $trendMpMismatchWarningStateCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_state_current"
    $trendMpMismatchWarningStatePrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_state_previous"
    $trendMpMismatchWarningStateChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_state_changed"
    $trendMpMismatchWarningReasonCurrent = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_reason_current"
    $trendMpMismatchWarningReasonPrevious = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_reason_previous"
    $trendMpMismatchWarningReasonChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_reason_changed"
    $trendMpMismatchWarningCodesChanged = Get-KeyValueLineValue -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_codes_changed"
    $trendMpMismatchWarningCodesPrevious = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_code_previous"
    $trendMpMismatchWarningCodesCurrent = Get-KeyValueLineValues -Lines $trendLines -Key "real_session_v0_trend_mp_mismatch_warning_code_current"
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
    Write-Host ("real_session_v0_loop_trend_auto_status_center_state_current=" + $trendStatusCenterStateCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_state_previous=" + $trendStatusCenterStatePrevious)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_state_changed=" + $trendStatusCenterStateChanged)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_reason_current=" + $trendStatusCenterReasonCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_reason_previous=" + $trendStatusCenterReasonPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_reason_changed=" + $trendStatusCenterReasonChanged)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_summary_text_current=" + $trendStatusCenterSummaryTextCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_summary_text_previous=" + $trendStatusCenterSummaryTextPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_status_center_summary_text_changed=" + $trendStatusCenterSummaryTextChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_resolution_current=" + $trendSaveRootResolutionCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_resolution_previous=" + $trendSaveRootResolutionPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_resolution_changed=" + $trendSaveRootResolutionChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_source_current=" + $trendSaveRootSourceCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_source_previous=" + $trendSaveRootSourcePrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_source_changed=" + $trendSaveRootSourceChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_path_current=" + $trendSaveRootPathCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_path_previous=" + $trendSaveRootPathPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_path_changed=" + $trendSaveRootPathChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_campaign_count_current=" + $trendSaveRootCampaignCountCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_campaign_count_previous=" + $trendSaveRootCampaignCountPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_campaign_count_changed=" + $trendSaveRootCampaignCountChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_save_file_count_current=" + $trendSaveRootSaveFileCountCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_save_file_count_previous=" + $trendSaveRootSaveFileCountPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_save_file_count_changed=" + $trendSaveRootSaveFileCountChanged)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_autosave_anchor_count_current=" + $trendSaveRootAutosaveAnchorCountCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_autosave_anchor_count_previous=" + $trendSaveRootAutosaveAnchorCountPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_save_root_autosave_anchor_count_changed=" + $trendSaveRootAutosaveAnchorCountChanged)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_current=" + $trendNextActionCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_previous=" + $trendNextActionPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_changed=" + $trendNextActionChanged)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_reason_current=" + $trendNextActionReasonCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_reason_previous=" + $trendNextActionReasonPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_reason_changed=" + $trendNextActionReasonChanged)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_current=" + $trendNextActionCommandHintCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_previous=" + $trendNextActionCommandHintPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_changed=" + $trendNextActionCommandHintChanged)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_source_current=" + $trendNextActionCommandHintSourceCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_source_previous=" + $trendNextActionCommandHintSourcePrevious)
    Write-Host ("real_session_v0_loop_trend_auto_next_action_command_hint_source_changed=" + $trendNextActionCommandHintSourceChanged)
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
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_state_previous=" + $trendMpPackageZipStatePrevious)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_state_current=" + $trendMpPackageZipStateCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_state_changed=" + $trendMpPackageZipStateChanged)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_reason_previous=" + $trendMpPackageZipReasonPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_reason_current=" + $trendMpPackageZipReasonCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_reason_changed=" + $trendMpPackageZipReasonChanged)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_sha256_previous=" + $trendMpPackageZipSha256Previous)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_sha256_current=" + $trendMpPackageZipSha256Current)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_sha256_changed=" + $trendMpPackageZipSha256Changed)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_path_previous=" + $trendMpPackageZipPathPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_path_current=" + $trendMpPackageZipPathCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_path_changed=" + $trendMpPackageZipPathChanged)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_bytes_previous=" + $trendMpPackageZipBytesPrevious)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_bytes_current=" + $trendMpPackageZipBytesCurrent)
    Write-Host ("real_session_v0_loop_trend_auto_mp_package_zip_bytes_changed=" + $trendMpPackageZipBytesChanged)
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
    if (-not [string]::IsNullOrWhiteSpace($trendMpGameVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_game_version_mismatch_warning_current=" + $trendMpGameVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpGameVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_game_version_mismatch_warning_previous=" + $trendMpGameVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpGameVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_game_version_mismatch_warning_changed=" + $trendMpGameVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpCampaignIdMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_campaign_id_mismatch_warning_current=" + $trendMpCampaignIdMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpCampaignIdMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_campaign_id_mismatch_warning_previous=" + $trendMpCampaignIdMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpCampaignIdMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_campaign_id_mismatch_warning_changed=" + $trendMpCampaignIdMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpOverlayVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_overlay_version_mismatch_warning_current=" + $trendMpOverlayVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpOverlayVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_overlay_version_mismatch_warning_previous=" + $trendMpOverlayVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpOverlayVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_overlay_version_mismatch_warning_changed=" + $trendMpOverlayVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpModVersionMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mod_version_mismatch_warning_current=" + $trendMpModVersionMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpModVersionMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mod_version_mismatch_warning_previous=" + $trendMpModVersionMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpModVersionMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mod_version_mismatch_warning_changed=" + $trendMpModVersionMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashMismatchWarningCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_mismatch_warning_current=" + $trendMpManifestHashMismatchWarningCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashMismatchWarningPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_mismatch_warning_previous=" + $trendMpManifestHashMismatchWarningPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpManifestHashMismatchWarningChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_manifest_hash_mismatch_warning_changed=" + $trendMpManifestHashMismatchWarningChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningStateCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_state_current=" + $trendMpMismatchWarningStateCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningStatePrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_state_previous=" + $trendMpMismatchWarningStatePrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningStateChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_state_changed=" + $trendMpMismatchWarningStateChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningReasonCurrent)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_reason_current=" + $trendMpMismatchWarningReasonCurrent)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningReasonPrevious)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_reason_previous=" + $trendMpMismatchWarningReasonPrevious)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningReasonChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_reason_changed=" + $trendMpMismatchWarningReasonChanged)
    }
    if (-not [string]::IsNullOrWhiteSpace($trendMpMismatchWarningCodesChanged)) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_codes_changed=" + $trendMpMismatchWarningCodesChanged)
    }
    foreach ($warningCode in $trendMpIdentityMismatchWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $trendMpIdentityMismatchWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_identity_mismatch_warning_code_current=" + $warningCode)
    }
    foreach ($warningCode in $trendMpMismatchWarningCodesPrevious) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_code_previous=" + $warningCode)
    }
    foreach ($warningCode in $trendMpMismatchWarningCodesCurrent) {
        Write-Host ("real_session_v0_loop_trend_auto_mp_mismatch_warning_code_current=" + $warningCode)
    }
    Write-Host ("real_session_v0_loop_trend_auto_latest_compare_command_hint=" + $trendLatestCompareCommandHint)
    Write-Host ("real_session_v0_loop_trend_auto_next_session_command_hint=" + $trendNextSessionCommandHint)
    foreach ($warningCode in $trendIdentityRiskWarningCodes) {
        Write-Host ("real_session_v0_loop_trend_auto_identity_risk_warning_code=" + $warningCode)
    }
}

$nextActionSummary = Get-NextActionSummary `
    -MpMismatchWarningState $mpPackageMismatchWarningState `
    -CompareRecommendation (Get-VariableOrDefault -Name "compareRecommendation") `
    -TrendRecommendation (Get-VariableOrDefault -Name "trendRecommendation") `
    -MpExportStrictVerifyCommand $mpExportStrictVerifyCommand `
    -CompareMpStrictVerifyCommandCurrent (Get-VariableOrDefault -Name "compareMpStrictVerifyCommandCurrent") `
    -TrendMpStrictVerifyCommandCurrent (Get-VariableOrDefault -Name "trendMpStrictVerifyCommandCurrent") `
    -CompareCommandHint (Get-VariableOrDefault -Name "compareCommandHintLine") `
    -TrendNextSessionCommandHint (Get-VariableOrDefault -Name "trendNextSessionCommandHint") `
    -DefaultNextSessionCommandHint $nextSessionCommandHint

Write-Host ("real_session_v0_loop_next_action=" + $nextActionSummary.action)
Write-Host ("real_session_v0_loop_next_action_reason=" + $nextActionSummary.reason)
Write-Host ("real_session_v0_loop_next_action_command_hint_source=" + $nextActionSummary.command_hint_source)
if (-not [string]::IsNullOrWhiteSpace($nextActionSummary.command_hint)) {
    Write-Host ("real_session_v0_loop_next_action_command_hint=" + $nextActionSummary.command_hint)
}

$sessionBriefPath = Join-Path $defaultRunRoot "real_session_v0_next_steps.md"
$sessionBriefLines = @(
    "# Strategic Nexus v0 Next Session Brief"
    ""
    "Session ID: $SessionId"
    "Run ID: $realSessionLoopRunId"
    "Generated UTC: $([DateTime]::UtcNow.ToString("o"))"
    "Campaign ID: $effectiveCampaignId"
    "Campaign ID source: $campaignIdSource"
    "Empire ID: $effectiveEmpireId"
    "Empire ID source: $empireIdSource"
    "DSL input path: $effectiveDslInputFull"
    "DSL input source: $dslInputSource"
    ""
    "## Current Outputs"
    "- Session archive: $sessionArchiveDir"
    "- Archive summary: $archiveSummaryPath"
    "- Season delta ledger: $seasonDeltaLedgerPath"
    "- Empire brief: $empireBriefPath"
    "- Entry point analysis: $entryPointAnalysisPath"
    "- Entry point readiness: $entryPointReadiness"
    "- Post-play package: $postPlayPackagePath"
    "- Post-play readiness: $postPlayPackageReadiness"
    "- Decision input package: $decisionInputPackagePath"
    "- Decision input readiness: $decisionInputPackageReadiness"
    "- Candidate decision package: $candidateDecisionPackagePath"
    "- Candidate readiness: $candidateDecisionPackageReadiness"
    "- DSL draft audit: $dslDraftAuditPath"
    "- DSL draft readiness: $dslDraftReadiness"
    "- SNC staged overlay status: $sncGeneratedOverlayStagingStatusPath"
    "- SNC staged overlay readiness: $sncGeneratedOverlayStagingReadiness"
    "- SNC status snapshot: $statusOutputJsonFull"
    "- SNC MP status snapshot: $statusWithMpSnapshotPathForOutput"
    "- SNC MP status readiness: $sncStatusWithMpReadiness"
    "- Session evidence JSON: $sessionEvidenceJsonFull"
    "- Generated overlay dir: $overlayOutputDirFull"
    "- Archive copied save count: $archiveCopiedSaveCount"
    "- Latest archived save: $archiveLastArchivedPath"
    ""
    "## Gameplay Acceptance"
    "- State: $gameplayAcceptanceState"
    "- Reason: $gameplayAcceptanceReason"
    "- Report path: $gameplayAcceptancePath"
    ""
    "## Status Center"
    "- State: $statusCenterState"
    "- Reason: $statusCenterReason"
    ""
    "## Next Action"
    "- Action: $($nextActionSummary.action)"
    "- Reason: $($nextActionSummary.reason)"
    "- Command source: $($nextActionSummary.command_hint_source)"
    "- Command hint: $($nextActionSummary.command_hint)"
    ""
    "## Follow-up Commands"
    "- Compare hint: $compareCommandHint"
    "- Trend hint: $trendCommandHint"
    "- Next session hint: $nextSessionCommandHint"
)
if ($ExportMpPackage) {
    $sessionBriefLines += @(
        ""
        "## MP Package"
        "- Readiness: $mpExportReadiness"
        "- Host readiness: $mpExportHostReadiness"
        "- Client readiness gate: $mpExportClientReadinessGate"
        "- Host next step: $mpExportHostNextStep"
        "- Client next step: $mpExportClientNextStep"
        "- Manifest hash: $mpExportManifestHash"
        "- Warning count: $mpExportWarningCount"
        "- Mismatch warning state: $mpPackageMismatchWarningState"
        "- Mismatch warning reason: $mpPackageMismatchWarningReason"
        "- Identity mismatch warning: $mpExportIdentityMismatchWarning"
        "- Campaign ID mismatch warning: $mpExportCampaignIdMismatchWarning"
        "- Overlay version mismatch warning: $mpExportOverlayVersionMismatchWarning"
        "- Game version mismatch warning: $mpExportGameVersionMismatchWarning"
        "- Mod version mismatch warning: $mpExportModVersionMismatchWarning"
        "- Manifest hash mismatch warning: $mpExportManifestHashMismatchWarning"
        "- Package directory: $mpPackageOutputDirFull"
        "- Package zip: $mpPackageZipPath"
        "- Package zip state: $mpPackageZipState"
        "- Package zip reason: $mpPackageZipReason"
        "- Package zip SHA256: $mpPackageZipSha256"
        "- Package zip bytes: $mpPackageZipBytes"
        "- Verify command: $mpExportVerifyCommand"
        "- Import command: $mpExportImportCommand"
        "- Strict verify command: $mpExportStrictVerifyCommand"
        "- Strict import command: $mpExportStrictImportCommand"
    )
}
$sessionBriefDir = Split-Path -Parent $sessionBriefPath
if (-not [string]::IsNullOrWhiteSpace($sessionBriefDir)) {
    New-Item -ItemType Directory -Force -Path $sessionBriefDir | Out-Null
}
$sessionBriefLines -join "`r`n" | Set-Content -LiteralPath $sessionBriefPath -Encoding utf8
Write-Host ("real_session_v0_loop_next_steps_brief=" + $sessionBriefPath)

$sessionEvidence = [ordered]@{
    evidence_schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    run_id = $realSessionLoopRunId
    save_root = [ordered]@{
        resolution = $saveRootResolution
        source = $saveRootSource
        path = $saveRootFull
        campaign_count = $saveRootAutoCampaignCount
        save_file_count = $saveRootAutoSaveFileCount
        autosave_anchor_count = $saveRootAutoAutosaveAnchorCount
    }
    resolved_identity = [ordered]@{
        campaign_id = $effectiveCampaignId
        campaign_id_source = $campaignIdSource
        empire_id = $effectiveEmpireId
        empire_id_source = $empireIdSource
    }
    dsl_input = [ordered]@{
        path = $effectiveDslInputFull
        source = $dslInputSource
    }
    session_id = $SessionId
    session_archive_dir = $sessionArchiveDir
    archive_summary_path = $archiveSummaryPath
    archive = [ordered]@{
        copied_save_count = $archiveCopiedSaveCount
        last_archived_path = $archiveLastArchivedPath
    }
    entry_point_post_play = [ordered]@{
        entry_point_analysis_path = $entryPointAnalysisPath
        entry_point_readiness = $entryPointReadiness
        entry_point_reason = $entryPointReason
        entry_point_count = $entryPointCount
        entry_point_branch_ambiguity = $entryPointBranchAmbiguity
        post_play_package_path = $postPlayPackagePath
        post_play_package_readiness = $postPlayPackageReadiness
        post_play_package_reason = $postPlayPackageReason
        post_play_decision_ready_entry_count = $postPlayDecisionReadyEntryCount
        post_play_campaign_count = $postPlayCampaignCount
        post_play_ready_campaign_count = $postPlayReadyCampaignCount
        post_play_partial_campaign_count = $postPlayPartialCampaignCount
        post_play_blocked_campaign_count = $postPlayBlockedCampaignCount
        decision_input_package_path = $decisionInputPackagePath
        decision_input_package_readiness = $decisionInputPackageReadiness
        decision_input_package_reason = $decisionInputPackageReason
        decision_input_count = $decisionInputCount
        decision_input_blocked_entry_count = $decisionInputBlockedEntryCount
        candidate_decision_package_path = $candidateDecisionPackagePath
        candidate_decision_package_readiness = $candidateDecisionPackageReadiness
        candidate_decision_package_reason = $candidateDecisionPackageReason
        candidate_decision_count = $candidateDecisionCount
        candidate_decision_blocked_source_entry_count = $candidateDecisionBlockedSourceEntryCount
        candidate_decision_validator_passed = $candidateDecisionValidatorPassed
        dsl_draft_path = $dslDraftPath
        dsl_draft_audit_path = $dslDraftAuditPath
        dsl_draft_readiness = $dslDraftReadiness
        dsl_draft_reason = $dslDraftReason
        dsl_draft_rule_count = $dslDraftRuleCount
        dsl_draft_eligible_candidate_count = $dslDraftEligibleCandidateCount
        dsl_draft_skipped_candidate_count = $dslDraftSkippedCandidateCount
        dsl_draft_validator_passed = $dslDraftValidatorPassed
        generated_overlay_staging_dir = $sncGeneratedOverlayStagingDir
        generated_overlay_staging_status_path = $sncGeneratedOverlayStagingStatusPath
        generated_overlay_staging_readiness = $sncGeneratedOverlayStagingReadiness
        generated_overlay_staging_reason = $sncGeneratedOverlayStagingReason
        generated_overlay_staging_rule_count = $sncGeneratedOverlayStagingRuleCount
        generated_overlay_manifest_verified = $sncGeneratedOverlayManifestVerified
        generated_overlay_publish_allowed = $sncGeneratedOverlayPublishAllowed
    }
    season_delta_ledger_path = $seasonDeltaLedgerPath
    empire_brief_path = $empireBriefPath
    generated_overlay_dir = $overlayOutputDirFull
    status_snapshot_path = $statusOutputJsonFull
    gameplay_acceptance = [ordered]@{
        state = $gameplayAcceptanceState
        reason = $gameplayAcceptanceReason
        path = $gameplayAcceptancePath
    }
    status_center = [ordered]@{
        state = $statusCenterState
        reason = $statusCenterReason
        summary_present = (-not [string]::IsNullOrWhiteSpace($statusCenterSummaryText))
        summary_text = $statusCenterSummaryText
    }
    command_hints = [ordered]@{
        compare = $compareCommandHint
        trend = $trendCommandHint
        next_session_compare_baseline_dir = $defaultRunRoot
        next_session = $nextSessionCommandHint
        next_steps_brief = $sessionBriefPath
        mp_verify = $mpExportVerifyCommand
        mp_import = $mpExportImportCommand
        mp_strict_verify = $mpExportStrictVerifyCommand
        mp_strict_import = $mpExportStrictImportCommand
    }
    next_action = [ordered]@{
        action = $nextActionSummary.action
        reason = $nextActionSummary.reason
        command_hint = $nextActionSummary.command_hint
        command_hint_source = $nextActionSummary.command_hint_source
    }
    mp_export = [ordered]@{
        enabled = [bool]$ExportMpPackage
        output_dir = $mpPackageOutputDirFull
        status_snapshot_with_mp_path = $statusWithMpSnapshotPathForOutput
        status_snapshot_with_mp_readiness = $sncStatusWithMpReadiness
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
        campaign_id_mismatch_warning = $mpExportCampaignIdMismatchWarning
        overlay_version_mismatch_warning = $mpExportOverlayVersionMismatchWarning
        game_version_mismatch_warning = $mpExportGameVersionMismatchWarning
        mod_version_mismatch_warning = $mpExportModVersionMismatchWarning
        manifest_hash_mismatch_warning = $mpExportManifestHashMismatchWarning
        mismatch_warning_state = $mpPackageMismatchWarningState
        mismatch_warning_reason = $mpPackageMismatchWarningReason
        mismatch_warning_codes = @($mpPackageMismatchWarningCodes)
        package_zip_state = $mpPackageZipState
        package_zip_reason = $mpPackageZipReason
        package_zip_path = $mpPackageZipPath
        package_zip_sha256 = $mpPackageZipSha256
        package_zip_bytes = $mpPackageZipBytes
    }
    auto_compare = [ordered]@{
        enabled = -not [string]::IsNullOrWhiteSpace($PreviousSessionDirForCompare)
        output_json = (Get-VariableOrDefault -Name "compareOutputJsonLine")
        command_hint = (Get-VariableOrDefault -Name "compareCommandHintLine")
        recommendation = (Get-VariableOrDefault -Name "compareRecommendation")
        identity_risk_warning = (Get-VariableOrDefault -Name "compareIdentityRiskWarning")
        identity_risk_warning_reason = (Get-VariableOrDefault -Name "compareIdentityRiskWarningReason")
        identity_risk_warning_codes = @(Get-VariableArrayOrDefault -Name "compareIdentityRiskWarningCodes")
        observable_effect_signal = (Get-VariableOrDefault -Name "compareObservableEffectSignal")
        observable_effect_reason = (Get-VariableOrDefault -Name "compareObservableEffectReason")
        verified_archive_save_count_current = (Get-VariableOrDefault -Name "compareArchiveSaveCountCurrent")
        verified_archive_save_count_previous = (Get-VariableOrDefault -Name "compareArchiveSaveCountPrevious")
        verified_archive_save_count_delta = (Get-VariableOrDefault -Name "compareArchiveSaveCountDelta")
        verified_archive_save_count_changed = (Get-VariableOrDefault -Name "compareArchiveSaveCountChanged")
        gameplay_acceptance_state_current = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStateCurrent")
        gameplay_acceptance_state_previous = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStatePrevious")
        gameplay_acceptance_state_changed = (Get-VariableOrDefault -Name "compareGameplayAcceptanceStateChanged")
        gameplay_acceptance_reason_current = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonCurrent")
        gameplay_acceptance_reason_previous = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonPrevious")
        gameplay_acceptance_reason_changed = (Get-VariableOrDefault -Name "compareGameplayAcceptanceReasonChanged")
        status_center_state_current = (Get-VariableOrDefault -Name "compareStatusCenterStateCurrent")
        status_center_state_previous = (Get-VariableOrDefault -Name "compareStatusCenterStatePrevious")
        status_center_state_changed = (Get-VariableOrDefault -Name "compareStatusCenterStateChanged")
        status_center_reason_current = (Get-VariableOrDefault -Name "compareStatusCenterReasonCurrent")
        status_center_reason_previous = (Get-VariableOrDefault -Name "compareStatusCenterReasonPrevious")
        status_center_reason_changed = (Get-VariableOrDefault -Name "compareStatusCenterReasonChanged")
        status_center_summary_text_current = (Get-VariableOrDefault -Name "compareStatusCenterSummaryTextCurrent")
        status_center_summary_text_previous = (Get-VariableOrDefault -Name "compareStatusCenterSummaryTextPrevious")
        status_center_summary_text_changed = (Get-VariableOrDefault -Name "compareStatusCenterSummaryTextChanged")
        save_root_resolution_current = (Get-VariableOrDefault -Name "compareSaveRootResolutionCurrent")
        save_root_resolution_previous = (Get-VariableOrDefault -Name "compareSaveRootResolutionPrevious")
        save_root_resolution_changed = (Get-VariableOrDefault -Name "compareSaveRootResolutionChanged")
        save_root_source_current = (Get-VariableOrDefault -Name "compareSaveRootSourceCurrent")
        save_root_source_previous = (Get-VariableOrDefault -Name "compareSaveRootSourcePrevious")
        save_root_source_changed = (Get-VariableOrDefault -Name "compareSaveRootSourceChanged")
        save_root_path_current = (Get-VariableOrDefault -Name "compareSaveRootPathCurrent")
        save_root_path_previous = (Get-VariableOrDefault -Name "compareSaveRootPathPrevious")
        save_root_path_changed = (Get-VariableOrDefault -Name "compareSaveRootPathChanged")
        save_root_campaign_count_current = (Get-VariableOrDefault -Name "compareSaveRootCampaignCountCurrent")
        save_root_campaign_count_previous = (Get-VariableOrDefault -Name "compareSaveRootCampaignCountPrevious")
        save_root_campaign_count_changed = (Get-VariableOrDefault -Name "compareSaveRootCampaignCountChanged")
        save_root_save_file_count_current = (Get-VariableOrDefault -Name "compareSaveRootSaveFileCountCurrent")
        save_root_save_file_count_previous = (Get-VariableOrDefault -Name "compareSaveRootSaveFileCountPrevious")
        save_root_save_file_count_changed = (Get-VariableOrDefault -Name "compareSaveRootSaveFileCountChanged")
        save_root_autosave_anchor_count_current = (Get-VariableOrDefault -Name "compareSaveRootAutosaveAnchorCountCurrent")
        save_root_autosave_anchor_count_previous = (Get-VariableOrDefault -Name "compareSaveRootAutosaveAnchorCountPrevious")
        save_root_autosave_anchor_count_changed = (Get-VariableOrDefault -Name "compareSaveRootAutosaveAnchorCountChanged")
        next_action_current = (Get-VariableOrDefault -Name "compareNextActionCurrent")
        next_action_previous = (Get-VariableOrDefault -Name "compareNextActionPrevious")
        next_action_changed = (Get-VariableOrDefault -Name "compareNextActionChanged")
        next_action_reason_current = (Get-VariableOrDefault -Name "compareNextActionReasonCurrent")
        next_action_reason_previous = (Get-VariableOrDefault -Name "compareNextActionReasonPrevious")
        next_action_reason_changed = (Get-VariableOrDefault -Name "compareNextActionReasonChanged")
        next_action_command_hint_current = (Get-VariableOrDefault -Name "compareNextActionCommandHintCurrent")
        next_action_command_hint_previous = (Get-VariableOrDefault -Name "compareNextActionCommandHintPrevious")
        next_action_command_hint_changed = (Get-VariableOrDefault -Name "compareNextActionCommandHintChanged")
        next_action_command_hint_source_current = (Get-VariableOrDefault -Name "compareNextActionCommandHintSourceCurrent")
        next_action_command_hint_source_previous = (Get-VariableOrDefault -Name "compareNextActionCommandHintSourcePrevious")
        next_action_command_hint_source_changed = (Get-VariableOrDefault -Name "compareNextActionCommandHintSourceChanged")
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
            status_snapshot_present_current = (Get-VariableOrDefault -Name "compareMpStatusSnapshotPresentCurrent")
            status_snapshot_present_previous = (Get-VariableOrDefault -Name "compareMpStatusSnapshotPresentPrevious")
            status_snapshot_present_changed = (Get-VariableOrDefault -Name "compareMpStatusSnapshotPresentChanged")
            package_output_dir_current = (Get-VariableOrDefault -Name "compareMpPackageOutputDirCurrent")
            package_output_dir_previous = (Get-VariableOrDefault -Name "compareMpPackageOutputDirPrevious")
            package_output_dir_changed = (Get-VariableOrDefault -Name "compareMpPackageOutputDirChanged")
            package_zip_state_current = (Get-VariableOrDefault -Name "compareMpPackageZipStateCurrent")
            package_zip_state_previous = (Get-VariableOrDefault -Name "compareMpPackageZipStatePrevious")
            package_zip_state_changed = (Get-VariableOrDefault -Name "compareMpPackageZipStateChanged")
            package_zip_reason_current = (Get-VariableOrDefault -Name "compareMpPackageZipReasonCurrent")
            package_zip_reason_previous = (Get-VariableOrDefault -Name "compareMpPackageZipReasonPrevious")
            package_zip_reason_changed = (Get-VariableOrDefault -Name "compareMpPackageZipReasonChanged")
            package_zip_sha256_current = (Get-VariableOrDefault -Name "compareMpPackageZipSha256Current")
            package_zip_sha256_previous = (Get-VariableOrDefault -Name "compareMpPackageZipSha256Previous")
            package_zip_sha256_changed = (Get-VariableOrDefault -Name "compareMpPackageZipSha256Changed")
            package_zip_path_current = (Get-VariableOrDefault -Name "compareMpPackageZipPathCurrent")
            package_zip_path_previous = (Get-VariableOrDefault -Name "compareMpPackageZipPathPrevious")
            package_zip_path_changed = (Get-VariableOrDefault -Name "compareMpPackageZipPathChanged")
            package_zip_bytes_current = (Get-VariableOrDefault -Name "compareMpPackageZipBytesCurrent")
            package_zip_bytes_previous = (Get-VariableOrDefault -Name "compareMpPackageZipBytesPrevious")
            package_zip_bytes_changed = (Get-VariableOrDefault -Name "compareMpPackageZipBytesChanged")
            warning_count_current = (Get-VariableOrDefault -Name "compareMpWarningCountCurrent")
            warning_count_delta = (Get-VariableOrDefault -Name "compareMpWarningCountDelta")
            warning_codes_changed = (Get-VariableOrDefault -Name "compareMpWarningCodesChanged")
            warning_codes_current = @(Get-VariableArrayOrDefault -Name "compareMpWarningCodesCurrent")
            warning_codes_previous = @(Get-VariableArrayOrDefault -Name "compareMpWarningCodesPrevious")
            identity_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCurrent")
            identity_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningPrevious")
            identity_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningChanged")
            identity_mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "compareMpIdentityMismatchWarningCodesChanged")
            identity_mismatch_warning_codes_current = @(Get-VariableArrayOrDefault -Name "compareMpIdentityMismatchWarningCodesCurrent")
            identity_mismatch_warning_codes_previous = @(Get-VariableArrayOrDefault -Name "compareMpIdentityMismatchWarningCodesPrevious")
            game_version_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpGameVersionMismatchWarningCurrent")
            game_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpGameVersionMismatchWarningPrevious")
            game_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpGameVersionMismatchWarningChanged")
            campaign_id_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpCampaignIdMismatchWarningCurrent")
            campaign_id_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpCampaignIdMismatchWarningPrevious")
            campaign_id_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpCampaignIdMismatchWarningChanged")
            overlay_version_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpOverlayVersionMismatchWarningCurrent")
            overlay_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpOverlayVersionMismatchWarningPrevious")
            overlay_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpOverlayVersionMismatchWarningChanged")
            mod_version_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpModVersionMismatchWarningCurrent")
            mod_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpModVersionMismatchWarningPrevious")
            mod_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpModVersionMismatchWarningChanged")
            manifest_hash_mismatch_warning_current = (Get-VariableOrDefault -Name "compareMpManifestHashMismatchWarningCurrent")
            manifest_hash_mismatch_warning_previous = (Get-VariableOrDefault -Name "compareMpManifestHashMismatchWarningPrevious")
            manifest_hash_mismatch_warning_changed = (Get-VariableOrDefault -Name "compareMpManifestHashMismatchWarningChanged")
            mismatch_warning_state_current = (Get-VariableOrDefault -Name "compareMpMismatchWarningStateCurrent")
            mismatch_warning_state_previous = (Get-VariableOrDefault -Name "compareMpMismatchWarningStatePrevious")
            mismatch_warning_state_changed = (Get-VariableOrDefault -Name "compareMpMismatchWarningStateChanged")
            mismatch_warning_reason_current = (Get-VariableOrDefault -Name "compareMpMismatchWarningReasonCurrent")
            mismatch_warning_reason_previous = (Get-VariableOrDefault -Name "compareMpMismatchWarningReasonPrevious")
            mismatch_warning_reason_changed = (Get-VariableOrDefault -Name "compareMpMismatchWarningReasonChanged")
            mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "compareMpMismatchWarningCodesChanged")
            mismatch_warning_codes_current = @(Get-VariableArrayOrDefault -Name "compareMpMismatchWarningCodesCurrent")
            mismatch_warning_codes_previous = @(Get-VariableArrayOrDefault -Name "compareMpMismatchWarningCodesPrevious")
        }
    }
    auto_trend = [ordered]@{
        enabled = [bool]$EmitTrendSummary
        output_json = (Get-VariableOrDefault -Name "trendOutputJsonLine")
        session_count = (Get-VariableOrDefault -Name "trendSessionCount")
        recommendation = (Get-VariableOrDefault -Name "trendRecommendation")
        identity_risk_warning = (Get-VariableOrDefault -Name "trendIdentityRiskWarning")
        identity_risk_warning_reason = (Get-VariableOrDefault -Name "trendIdentityRiskWarningReason")
        identity_risk_warning_codes = @(Get-VariableArrayOrDefault -Name "trendIdentityRiskWarningCodes")
        observable_effect_signal = (Get-VariableOrDefault -Name "trendObservableEffectSignal")
        observable_effect_reason = (Get-VariableOrDefault -Name "trendObservableEffectReason")
        gameplay_acceptance_state_current = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStateCurrent")
        gameplay_acceptance_state_previous = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStatePrevious")
        gameplay_acceptance_state_changed = (Get-VariableOrDefault -Name "trendGameplayAcceptanceStateChanged")
        gameplay_acceptance_reason_current = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonCurrent")
        gameplay_acceptance_reason_previous = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonPrevious")
        gameplay_acceptance_reason_changed = (Get-VariableOrDefault -Name "trendGameplayAcceptanceReasonChanged")
        status_center_state_current = (Get-VariableOrDefault -Name "trendStatusCenterStateCurrent")
        status_center_state_previous = (Get-VariableOrDefault -Name "trendStatusCenterStatePrevious")
        status_center_state_changed = (Get-VariableOrDefault -Name "trendStatusCenterStateChanged")
        status_center_reason_current = (Get-VariableOrDefault -Name "trendStatusCenterReasonCurrent")
        status_center_reason_previous = (Get-VariableOrDefault -Name "trendStatusCenterReasonPrevious")
        status_center_reason_changed = (Get-VariableOrDefault -Name "trendStatusCenterReasonChanged")
        status_center_summary_text_current = (Get-VariableOrDefault -Name "trendStatusCenterSummaryTextCurrent")
        status_center_summary_text_previous = (Get-VariableOrDefault -Name "trendStatusCenterSummaryTextPrevious")
        status_center_summary_text_changed = (Get-VariableOrDefault -Name "trendStatusCenterSummaryTextChanged")
        save_root_resolution_current = (Get-VariableOrDefault -Name "trendSaveRootResolutionCurrent")
        save_root_resolution_previous = (Get-VariableOrDefault -Name "trendSaveRootResolutionPrevious")
        save_root_resolution_changed = (Get-VariableOrDefault -Name "trendSaveRootResolutionChanged")
        save_root_source_current = (Get-VariableOrDefault -Name "trendSaveRootSourceCurrent")
        save_root_source_previous = (Get-VariableOrDefault -Name "trendSaveRootSourcePrevious")
        save_root_source_changed = (Get-VariableOrDefault -Name "trendSaveRootSourceChanged")
        save_root_path_current = (Get-VariableOrDefault -Name "trendSaveRootPathCurrent")
        save_root_path_previous = (Get-VariableOrDefault -Name "trendSaveRootPathPrevious")
        save_root_path_changed = (Get-VariableOrDefault -Name "trendSaveRootPathChanged")
        save_root_campaign_count_current = (Get-VariableOrDefault -Name "trendSaveRootCampaignCountCurrent")
        save_root_campaign_count_previous = (Get-VariableOrDefault -Name "trendSaveRootCampaignCountPrevious")
        save_root_campaign_count_changed = (Get-VariableOrDefault -Name "trendSaveRootCampaignCountChanged")
        save_root_save_file_count_current = (Get-VariableOrDefault -Name "trendSaveRootSaveFileCountCurrent")
        save_root_save_file_count_previous = (Get-VariableOrDefault -Name "trendSaveRootSaveFileCountPrevious")
        save_root_save_file_count_changed = (Get-VariableOrDefault -Name "trendSaveRootSaveFileCountChanged")
        save_root_autosave_anchor_count_current = (Get-VariableOrDefault -Name "trendSaveRootAutosaveAnchorCountCurrent")
        save_root_autosave_anchor_count_previous = (Get-VariableOrDefault -Name "trendSaveRootAutosaveAnchorCountPrevious")
        save_root_autosave_anchor_count_changed = (Get-VariableOrDefault -Name "trendSaveRootAutosaveAnchorCountChanged")
        next_action_current = (Get-VariableOrDefault -Name "trendNextActionCurrent")
        next_action_previous = (Get-VariableOrDefault -Name "trendNextActionPrevious")
        next_action_changed = (Get-VariableOrDefault -Name "trendNextActionChanged")
        next_action_reason_current = (Get-VariableOrDefault -Name "trendNextActionReasonCurrent")
        next_action_reason_previous = (Get-VariableOrDefault -Name "trendNextActionReasonPrevious")
        next_action_reason_changed = (Get-VariableOrDefault -Name "trendNextActionReasonChanged")
        next_action_command_hint_current = (Get-VariableOrDefault -Name "trendNextActionCommandHintCurrent")
        next_action_command_hint_previous = (Get-VariableOrDefault -Name "trendNextActionCommandHintPrevious")
        next_action_command_hint_changed = (Get-VariableOrDefault -Name "trendNextActionCommandHintChanged")
        next_action_command_hint_source_current = (Get-VariableOrDefault -Name "trendNextActionCommandHintSourceCurrent")
        next_action_command_hint_source_previous = (Get-VariableOrDefault -Name "trendNextActionCommandHintSourcePrevious")
        next_action_command_hint_source_changed = (Get-VariableOrDefault -Name "trendNextActionCommandHintSourceChanged")
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
            package_zip_state_current = (Get-VariableOrDefault -Name "trendMpPackageZipStateCurrent")
            package_zip_state_previous = (Get-VariableOrDefault -Name "trendMpPackageZipStatePrevious")
            package_zip_state_changed = (Get-VariableOrDefault -Name "trendMpPackageZipStateChanged")
            package_zip_reason_current = (Get-VariableOrDefault -Name "trendMpPackageZipReasonCurrent")
            package_zip_reason_previous = (Get-VariableOrDefault -Name "trendMpPackageZipReasonPrevious")
            package_zip_reason_changed = (Get-VariableOrDefault -Name "trendMpPackageZipReasonChanged")
            package_zip_sha256_current = (Get-VariableOrDefault -Name "trendMpPackageZipSha256Current")
            package_zip_sha256_previous = (Get-VariableOrDefault -Name "trendMpPackageZipSha256Previous")
            package_zip_sha256_changed = (Get-VariableOrDefault -Name "trendMpPackageZipSha256Changed")
            package_zip_path_current = (Get-VariableOrDefault -Name "trendMpPackageZipPathCurrent")
            package_zip_path_previous = (Get-VariableOrDefault -Name "trendMpPackageZipPathPrevious")
            package_zip_path_changed = (Get-VariableOrDefault -Name "trendMpPackageZipPathChanged")
            package_zip_bytes_current = (Get-VariableOrDefault -Name "trendMpPackageZipBytesCurrent")
            package_zip_bytes_previous = (Get-VariableOrDefault -Name "trendMpPackageZipBytesPrevious")
            package_zip_bytes_changed = (Get-VariableOrDefault -Name "trendMpPackageZipBytesChanged")
            warning_count_current = (Get-VariableOrDefault -Name "trendMpWarningCountCurrent")
            warning_count_delta = (Get-VariableOrDefault -Name "trendMpWarningCountDelta")
            warning_codes_changed = (Get-VariableOrDefault -Name "trendMpWarningCodesChanged")
            warning_codes_current = @(Get-VariableArrayOrDefault -Name "trendMpWarningCodesCurrent")
            warning_codes_previous = @(Get-VariableArrayOrDefault -Name "trendMpWarningCodesPrevious")
            identity_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCurrent")
            identity_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningPrevious")
            identity_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningChanged")
            identity_mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "trendMpIdentityMismatchWarningCodesChanged")
            identity_mismatch_warning_codes_current = @(Get-VariableArrayOrDefault -Name "trendMpIdentityMismatchWarningCodesCurrent")
            identity_mismatch_warning_codes_previous = @(Get-VariableArrayOrDefault -Name "trendMpIdentityMismatchWarningCodesPrevious")
            game_version_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpGameVersionMismatchWarningCurrent")
            game_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpGameVersionMismatchWarningPrevious")
            game_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpGameVersionMismatchWarningChanged")
            campaign_id_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpCampaignIdMismatchWarningCurrent")
            campaign_id_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpCampaignIdMismatchWarningPrevious")
            campaign_id_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpCampaignIdMismatchWarningChanged")
            overlay_version_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpOverlayVersionMismatchWarningCurrent")
            overlay_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpOverlayVersionMismatchWarningPrevious")
            overlay_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpOverlayVersionMismatchWarningChanged")
            mod_version_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpModVersionMismatchWarningCurrent")
            mod_version_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpModVersionMismatchWarningPrevious")
            mod_version_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpModVersionMismatchWarningChanged")
            manifest_hash_mismatch_warning_current = (Get-VariableOrDefault -Name "trendMpManifestHashMismatchWarningCurrent")
            manifest_hash_mismatch_warning_previous = (Get-VariableOrDefault -Name "trendMpManifestHashMismatchWarningPrevious")
            manifest_hash_mismatch_warning_changed = (Get-VariableOrDefault -Name "trendMpManifestHashMismatchWarningChanged")
            mismatch_warning_state_current = (Get-VariableOrDefault -Name "trendMpMismatchWarningStateCurrent")
            mismatch_warning_state_previous = (Get-VariableOrDefault -Name "trendMpMismatchWarningStatePrevious")
            mismatch_warning_state_changed = (Get-VariableOrDefault -Name "trendMpMismatchWarningStateChanged")
            mismatch_warning_reason_current = (Get-VariableOrDefault -Name "trendMpMismatchWarningReasonCurrent")
            mismatch_warning_reason_previous = (Get-VariableOrDefault -Name "trendMpMismatchWarningReasonPrevious")
            mismatch_warning_reason_changed = (Get-VariableOrDefault -Name "trendMpMismatchWarningReasonChanged")
            mismatch_warning_codes_changed = (Get-VariableOrDefault -Name "trendMpMismatchWarningCodesChanged")
            mismatch_warning_codes_current = @(Get-VariableArrayOrDefault -Name "trendMpMismatchWarningCodesCurrent")
            mismatch_warning_codes_previous = @(Get-VariableArrayOrDefault -Name "trendMpMismatchWarningCodesPrevious")
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
