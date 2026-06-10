$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist/strategic_nexus_app_test.exe"

if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "dist"))) {
    New-Item -ItemType Directory -Force -Path (Join-Path $repoRoot "dist") | Out-Null
}

function Invoke-WithDeveloperShell {
    param(
        [string]$ScriptPath
    )

    $knownVsDevCmdPaths = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    )
    $vsDevCmd = $knownVsDevCmdPaths |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1

    if (-not $vsDevCmd) {
        $vsDevCmd = Get-ChildItem -Path `
            "C:\Program Files\Microsoft Visual Studio", `
            "C:\Program Files (x86)\Microsoft Visual Studio" `
            -Recurse `
            -Filter "VsDevCmd.bat" `
            -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (-not $vsDevCmd) {
        throw "cl.exe was not found and VsDevCmd.bat could not be located. Install Visual Studio C++ tools or run from a Developer PowerShell."
    }

    & cmd.exe /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && powershell -NoProfile -ExecutionPolicy Bypass -File `"$ScriptPath`""
    exit $LASTEXITCODE
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Invoke-WithDeveloperShell -ScriptPath $MyInvocation.MyCommand.Path
}

$sourceFiles = @((Join-Path $repoRoot "Strategic Nexus.cpp"))
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src") -Filter "*.cpp" |
    Where-Object { $_.Name -ne "SncTrayApp.cpp" } |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/common") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/bridge_core") -Filter "*.cpp" |
    Where-Object { $_.Name -ne "main.cpp" } |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/generated_overlay") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/strategic_pipeline") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

$cabinetContractExePath = Join-Path $repoRoot "dist/v0_cabinet_contract_test.exe"
$autosaveArchiverExePath = Join-Path $repoRoot "dist/autosave_archiver_test.exe"
$autosaveArchiverSourceFiles = @(
    (Join-Path $repoRoot "tests/autosave_archiver_test.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp")
)
$autosaveArchiveVerifierExePath = Join-Path $repoRoot "dist/autosave_archive_verifier_test.exe"
$autosaveArchiveVerifierSourceFiles = @(
    (Join-Path $repoRoot "tests/autosave_archive_verifier_test.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$autosaveArchiveSummarizerExePath = Join-Path $repoRoot "dist/autosave_archive_summarizer_test.exe"
$autosaveArchiveSummarizerSourceFiles = @(
    (Join-Path $repoRoot "tests/autosave_archive_summarizer_test.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$archiveMinistryInputBuilderExePath = Join-Path $repoRoot "dist/archive_ministry_input_builder_test.exe"
$archiveMinistryInputBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/archive_ministry_input_builder_test.cpp"),
    (Join-Path $repoRoot "src/ArchiveMinistryInputBuilder.cpp")
)
$seasonDeltaLedgerBuilderExePath = Join-Path $repoRoot "dist/season_delta_ledger_builder_test.exe"
$seasonDeltaLedgerBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/season_delta_ledger_builder_test.cpp"),
    (Join-Path $repoRoot "src/SeasonDeltaLedgerBuilder.cpp")
)
$seasonEmpireBriefBuilderExePath = Join-Path $repoRoot "dist/season_empire_brief_builder_test.exe"
$seasonEmpireBriefBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/season_empire_brief_builder_test.cpp"),
    (Join-Path $repoRoot "src/SeasonEmpireBriefBuilder.cpp")
)
$observerTargetProfileBuilderExePath = Join-Path $repoRoot "dist/observer_target_profile_builder_test.exe"
$observerTargetProfileBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/observer_target_profile_builder_test.cpp"),
    (Join-Path $repoRoot "src/ObserverTargetProfileBuilder.cpp"),
    (Join-Path $repoRoot "src/SeasonEmpireBriefBuilder.cpp")
)
$integratedEmpireStateBuilderExePath = Join-Path $repoRoot "dist/integrated_empire_state_builder_test.exe"
$integratedEmpireStateBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/integrated_empire_state_builder_test.cpp"),
    (Join-Path $repoRoot "src/IntegratedEmpireStateBuilder.cpp"),
    (Join-Path $repoRoot "src/SeasonEmpireBriefBuilder.cpp")
)
$personalityProfileStoreExePath = Join-Path $repoRoot "dist/personality_profile_store_test.exe"
$personalityProfileStoreSourceFiles = @(
    (Join-Path $repoRoot "tests/personality_profile_store_test.cpp"),
    (Join-Path $repoRoot "src/PersonalityProfileStore.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$personalityEngineExePath = Join-Path $repoRoot "dist/personality_engine_test.exe"
$personalityEngineSourceFiles = @(
    (Join-Path $repoRoot "tests/personality_engine_test.cpp"),
    (Join-Path $repoRoot "src/PersonalityEngine.cpp"),
    (Join-Path $repoRoot "src/DoctrinePlanner.cpp"),
    (Join-Path $repoRoot "src/LlmClient.cpp"),
    (Join-Path $repoRoot "src/StrategicRequest.cpp"),
    (Join-Path $repoRoot "src/BridgeContract.cpp"),
    (Join-Path $repoRoot "src/ModIntegration.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp")
)
$cabinetContractSourceFiles = @(
    (Join-Path $repoRoot "tests/v0_cabinet_contract_test.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/LightweightCabinet.cpp"),
    (Join-Path $repoRoot "src/bridge_core/BridgeState.cpp")
)
$priorityScoreExePath = Join-Path $repoRoot "dist/v0_priority_score_test.exe"
$priorityScoreSourceFiles = @(
    (Join-Path $repoRoot "tests/v0_priority_score_test.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/ProcessingPriorityScorer.cpp")
)
$processingQueueExePath = Join-Path $repoRoot "dist/v0_processing_queue_test.exe"
$processingQueueSourceFiles = @(
    (Join-Path $repoRoot "tests/v0_processing_queue_test.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/EmpireProcessingQueue.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/ProcessingPriorityScorer.cpp")
)
$ministryInputReaderExePath = Join-Path $repoRoot "dist/v0_ministry_input_reader_test.exe"
$ministryInputReaderSourceFiles = @(
    (Join-Path $repoRoot "tests/v0_ministry_input_reader_test.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/MinistryInputReader.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$empireContextIsolationExePath = Join-Path $repoRoot "dist/v0_empire_context_isolation_test.exe"
$empireContextIsolationSourceFiles = @(
    (Join-Path $repoRoot "tests/v0_empire_context_isolation_test.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/V0StrategicPipeline.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/MinistryInputReader.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/ProcessingPriorityScorer.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/DeterministicMinistry.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/StatelessClerk.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/LightweightCabinet.cpp"),
    (Join-Path $repoRoot "src/strategic_pipeline/PipelineJsonWriter.cpp"),
    (Join-Path $repoRoot "src/bridge_core/BridgeState.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$generatedOverlayContractExePath = Join-Path $repoRoot "dist/generated_overlay_contract_test.exe"
$generatedOverlayContractSourceFiles = @(
    (Join-Path $repoRoot "tests/generated_overlay_contract_test.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp")
)
$generatedOverlayVerifierExePath = Join-Path $repoRoot "dist/generated_overlay_verifier_test.exe"
$generatedOverlayVerifierSourceFiles = @(
    (Join-Path $repoRoot "tests/generated_overlay_verifier_test.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/MpOverlayPackage.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp")
)
$generatedOverlayPublisherExePath = Join-Path $repoRoot "dist/generated_overlay_publisher_test.exe"
$generatedOverlayPublisherSourceFiles = @(
    (Join-Path $repoRoot "tests/generated_overlay_publisher_test.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/GeneratedOverlayPublisher.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp")
)
$mpOverlayPackageVerifierExePath = Join-Path $repoRoot "dist/mp_overlay_package_verifier_test.exe"
$mpOverlayPackageVerifierSourceFiles = @(
    (Join-Path $repoRoot "tests/mp_overlay_package_verifier_test.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/MpOverlayPackage.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp")
)
$campaignSaveScannerExePath = Join-Path $repoRoot "dist/campaign_save_scanner_test.exe"
$campaignSaveScannerSourceFiles = @(
    (Join-Path $repoRoot "tests/campaign_save_scanner_test.cpp"),
    (Join-Path $repoRoot "src/CampaignSaveScanner.cpp")
)
$campaignLibraryPlannerExePath = Join-Path $repoRoot "dist/campaign_library_planner_test.exe"
$campaignLibraryPlannerSourceFiles = @(
    (Join-Path $repoRoot "tests/campaign_library_planner_test.cpp"),
    (Join-Path $repoRoot "src/CampaignLibraryPlanner.cpp")
)
$stellarisSavePathResolverExePath = Join-Path $repoRoot "dist/stellaris_save_path_resolver_test.exe"
$stellarisSavePathResolverSourceFiles = @(
    (Join-Path $repoRoot "tests/stellaris_save_path_resolver_test.cpp"),
    (Join-Path $repoRoot "src/StellarisSavePathResolver.cpp")
)
$stellarisProcessDetectorExePath = Join-Path $repoRoot "dist/stellaris_process_detector_test.exe"
$stellarisProcessDetectorSourceFiles = @(
    (Join-Path $repoRoot "tests/stellaris_process_detector_test.cpp"),
    (Join-Path $repoRoot "src/StellarisProcessDetector.cpp")
)
$saveParserExePath = Join-Path $repoRoot "dist/save_parser_test.exe"
$saveParserSourceFiles = @(
    (Join-Path $repoRoot "tests/save_parser_test.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp")
)
$saveEntryPointAnalyzerExePath = Join-Path $repoRoot "dist/save_entry_point_analyzer_test.exe"
$saveEntryPointAnalyzerSourceFiles = @(
    (Join-Path $repoRoot "tests/save_entry_point_analyzer_test.cpp"),
    (Join-Path $repoRoot "src/SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$postPlayPackageBuilderExePath = Join-Path $repoRoot "dist/post_play_package_builder_test.exe"
$postPlayPackageBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/post_play_package_builder_test.cpp"),
    (Join-Path $repoRoot "src/PostPlayPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncDecisionInputPackageBuilderExePath = Join-Path $repoRoot "dist/snc_decision_input_package_builder_test.exe"
$sncDecisionInputPackageBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_decision_input_package_builder_test.cpp"),
    (Join-Path $repoRoot "src/SncDecisionInputPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncCandidateDecisionPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/PostPlayPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncCandidateDecisionPackageBuilderExePath = Join-Path $repoRoot "dist/snc_candidate_decision_package_builder_test.exe"
$sncCandidateDecisionPackageBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_candidate_decision_package_builder_test.cpp"),
    (Join-Path $repoRoot "src/SncCandidateDecisionPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncDslDraftPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncDecisionInputPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/PostPlayPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncDslDraftPackageBuilderExePath = Join-Path $repoRoot "dist/snc_dsl_draft_package_builder_test.exe"
$sncDslDraftPackageBuilderSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_dsl_draft_package_builder_test.cpp"),
    (Join-Path $repoRoot "src/SncDslDraftPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncCandidateDecisionPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncDecisionInputPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/PostPlayPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/SaveParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncGeneratedOverlayStagerExePath = Join-Path $repoRoot "dist/snc_generated_overlay_stager_test.exe"
$sncGeneratedOverlayStagerSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_generated_overlay_stager_test.cpp"),
    (Join-Path $repoRoot "src/SncGeneratedOverlayStager.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncGeneratedOverlayPublishGateExePath = Join-Path $repoRoot "dist/snc_generated_overlay_publish_gate_test.exe"
$sncGeneratedOverlayPublishGateSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_generated_overlay_publish_gate_test.cpp"),
    (Join-Path $repoRoot "src/SncGeneratedOverlayPublishGate.cpp"),
    (Join-Path $repoRoot "src/SncGeneratedOverlayStager.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/GeneratedOverlayPublisher.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncPostPlayArtifactBackfillerExePath = Join-Path $repoRoot "dist/snc_post_play_artifact_backfiller_test.exe"
$sncPostPlayArtifactBackfillerSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_post_play_artifact_backfiller_test.cpp"),
    (Join-Path $repoRoot "src/SncPostPlayArtifactBackfiller.cpp"),
    (Join-Path $repoRoot "src/SncCandidateDecisionPackageBuilder.cpp"),
    (Join-Path $repoRoot "src/SncGeneratedOverlayStager.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslParser.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/DslValidator.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/MpOverlayPackage.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/OverlayCompiler.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)
$sncFriendPackageExePath = Join-Path $repoRoot "dist/snc_friend_package_test.exe"
$sncFriendPackageSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_friend_package_test.cpp"),
    (Join-Path $repoRoot "src/SncFriendPackage.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp")
)
$sncTrayStartupShortcutActionExePath = Join-Path $repoRoot "dist/snc_tray_startup_shortcut_action_test.exe"
$sncTrayStartupShortcutActionSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_tray_startup_shortcut_action_test.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp")
)
$sncTraySupportReportActionExePath = Join-Path $repoRoot "dist/snc_tray_support_report_action_test.exe"
$sncTraySupportReportActionSourceFiles = @(
    (Join-Path $repoRoot "tests/snc_tray_support_report_action_test.cpp")
)
$strategicNexusCompanionExePath = Join-Path $repoRoot "dist/strategic_nexus_companion_test.exe"
$strategicNexusCompanionSourceFiles = @(
    (Join-Path $repoRoot "tests/strategic_nexus_companion_test.cpp"),
    (Join-Path $repoRoot "src/LocalLlmModelManager.cpp"),
    (Join-Path $repoRoot "src/LocalLlmRuntimeAdapter.cpp"),
    (Join-Path $repoRoot "src/StrategicNexusCompanion.cpp"),
    (Join-Path $repoRoot "src/SncFriendPackage.cpp"),
    (Join-Path $repoRoot "src/AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src/StellarisProcessDetector.cpp"),
    (Join-Path $repoRoot "src/StellarisSavePathResolver.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src/generated_overlay/MpOverlayPackage.cpp"),
    (Join-Path $repoRoot "src/common/FileUtil.cpp"),
    (Join-Path $repoRoot "src/common/JsonExtract.cpp"),
    (Join-Path $repoRoot "src/common/JsonSanity.cpp")
)

function Invoke-ClCompile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string[]]$SourceFiles,

        [Parameter(Mandatory = $true)]
        [string]$OutputPath
    )

    $objectRoot = Join-Path $repoRoot "dist\obj"
    if (-not (Test-Path -LiteralPath $objectRoot)) {
        New-Item -ItemType Directory -Force -Path $objectRoot | Out-Null
    }

    $safeName = ($Name -replace '[^A-Za-z0-9_.-]+', '_')
    $objectDir = Join-Path $objectRoot $safeName
    if (-not (Test-Path -LiteralPath $objectDir)) {
        New-Item -ItemType Directory -Force -Path $objectDir | Out-Null
    }
    Get-ChildItem -LiteralPath $objectDir -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    $objectOutputArg = "/Fo$objectDir\\"

    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $objectOutputArg $SourceFiles /Fe:$OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "$Name compile failed (exit code $LASTEXITCODE)."
    }
}

Push-Location $repoRoot
try {
    Invoke-ClCompile -Name "strategic_nexus_app_test" -SourceFiles $sourceFiles -OutputPath $exePath
    Invoke-ClCompile -Name "autosave_archiver_test" -SourceFiles $autosaveArchiverSourceFiles -OutputPath $autosaveArchiverExePath
    Invoke-ClCompile -Name "autosave_archive_verifier_test" -SourceFiles $autosaveArchiveVerifierSourceFiles -OutputPath $autosaveArchiveVerifierExePath
    Invoke-ClCompile -Name "autosave_archive_summarizer_test" -SourceFiles $autosaveArchiveSummarizerSourceFiles -OutputPath $autosaveArchiveSummarizerExePath
    Invoke-ClCompile -Name "archive_ministry_input_builder_test" -SourceFiles $archiveMinistryInputBuilderSourceFiles -OutputPath $archiveMinistryInputBuilderExePath
    Invoke-ClCompile -Name "season_delta_ledger_builder_test" -SourceFiles $seasonDeltaLedgerBuilderSourceFiles -OutputPath $seasonDeltaLedgerBuilderExePath
    Invoke-ClCompile -Name "season_empire_brief_builder_test" -SourceFiles $seasonEmpireBriefBuilderSourceFiles -OutputPath $seasonEmpireBriefBuilderExePath
    Invoke-ClCompile -Name "observer_target_profile_builder_test" -SourceFiles $observerTargetProfileBuilderSourceFiles -OutputPath $observerTargetProfileBuilderExePath
    Invoke-ClCompile -Name "integrated_empire_state_builder_test" -SourceFiles $integratedEmpireStateBuilderSourceFiles -OutputPath $integratedEmpireStateBuilderExePath
    Invoke-ClCompile -Name "personality_profile_store_test" -SourceFiles $personalityProfileStoreSourceFiles -OutputPath $personalityProfileStoreExePath
    Invoke-ClCompile -Name "personality_engine_test" -SourceFiles $personalityEngineSourceFiles -OutputPath $personalityEngineExePath
    Invoke-ClCompile -Name "v0_cabinet_contract_test" -SourceFiles $cabinetContractSourceFiles -OutputPath $cabinetContractExePath
    Invoke-ClCompile -Name "v0_priority_score_test" -SourceFiles $priorityScoreSourceFiles -OutputPath $priorityScoreExePath
    Invoke-ClCompile -Name "v0_processing_queue_test" -SourceFiles $processingQueueSourceFiles -OutputPath $processingQueueExePath
    Invoke-ClCompile -Name "v0_ministry_input_reader_test" -SourceFiles $ministryInputReaderSourceFiles -OutputPath $ministryInputReaderExePath
    Invoke-ClCompile -Name "v0_empire_context_isolation_test" -SourceFiles $empireContextIsolationSourceFiles -OutputPath $empireContextIsolationExePath
    Invoke-ClCompile -Name "generated_overlay_contract_test" -SourceFiles $generatedOverlayContractSourceFiles -OutputPath $generatedOverlayContractExePath
    Invoke-ClCompile -Name "generated_overlay_verifier_test" -SourceFiles $generatedOverlayVerifierSourceFiles -OutputPath $generatedOverlayVerifierExePath
    Invoke-ClCompile -Name "generated_overlay_publisher_test" -SourceFiles $generatedOverlayPublisherSourceFiles -OutputPath $generatedOverlayPublisherExePath
    Invoke-ClCompile -Name "mp_overlay_package_verifier_test" -SourceFiles $mpOverlayPackageVerifierSourceFiles -OutputPath $mpOverlayPackageVerifierExePath
    Invoke-ClCompile -Name "campaign_save_scanner_test" -SourceFiles $campaignSaveScannerSourceFiles -OutputPath $campaignSaveScannerExePath
    Invoke-ClCompile -Name "campaign_library_planner_test" -SourceFiles $campaignLibraryPlannerSourceFiles -OutputPath $campaignLibraryPlannerExePath
    Invoke-ClCompile -Name "stellaris_save_path_resolver_test" -SourceFiles $stellarisSavePathResolverSourceFiles -OutputPath $stellarisSavePathResolverExePath
    Invoke-ClCompile -Name "stellaris_process_detector_test" -SourceFiles $stellarisProcessDetectorSourceFiles -OutputPath $stellarisProcessDetectorExePath
    Invoke-ClCompile -Name "save_parser_test" -SourceFiles $saveParserSourceFiles -OutputPath $saveParserExePath
    Invoke-ClCompile -Name "save_entry_point_analyzer_test" -SourceFiles $saveEntryPointAnalyzerSourceFiles -OutputPath $saveEntryPointAnalyzerExePath
    Invoke-ClCompile -Name "post_play_package_builder_test" -SourceFiles $postPlayPackageBuilderSourceFiles -OutputPath $postPlayPackageBuilderExePath
    Invoke-ClCompile -Name "snc_decision_input_package_builder_test" -SourceFiles $sncDecisionInputPackageBuilderSourceFiles -OutputPath $sncDecisionInputPackageBuilderExePath
    Invoke-ClCompile -Name "snc_candidate_decision_package_builder_test" -SourceFiles $sncCandidateDecisionPackageBuilderSourceFiles -OutputPath $sncCandidateDecisionPackageBuilderExePath
    Invoke-ClCompile -Name "snc_dsl_draft_package_builder_test" -SourceFiles $sncDslDraftPackageBuilderSourceFiles -OutputPath $sncDslDraftPackageBuilderExePath
    Invoke-ClCompile -Name "snc_generated_overlay_stager_test" -SourceFiles $sncGeneratedOverlayStagerSourceFiles -OutputPath $sncGeneratedOverlayStagerExePath
    Invoke-ClCompile -Name "snc_generated_overlay_publish_gate_test" -SourceFiles $sncGeneratedOverlayPublishGateSourceFiles -OutputPath $sncGeneratedOverlayPublishGateExePath
    Invoke-ClCompile -Name "snc_post_play_artifact_backfiller_test" -SourceFiles $sncPostPlayArtifactBackfillerSourceFiles -OutputPath $sncPostPlayArtifactBackfillerExePath
    Invoke-ClCompile -Name "snc_friend_package_test" -SourceFiles $sncFriendPackageSourceFiles -OutputPath $sncFriendPackageExePath
    Invoke-ClCompile -Name "snc_tray_startup_shortcut_action_test" -SourceFiles $sncTrayStartupShortcutActionSourceFiles -OutputPath $sncTrayStartupShortcutActionExePath
    Invoke-ClCompile -Name "snc_tray_support_report_action_test" -SourceFiles $sncTraySupportReportActionSourceFiles -OutputPath $sncTraySupportReportActionExePath
    Invoke-ClCompile -Name "strategic_nexus_companion_test" -SourceFiles $strategicNexusCompanionSourceFiles -OutputPath $strategicNexusCompanionExePath
} finally {
    Pop-Location
}

function Assert-Contains {
    param(
        [string]$Name,
        [string]$Text,
        [string]$Expected
    )

    if ($Text -notmatch [regex]::Escape($Expected)) {
        throw "$Name failed. Missing expected output '$Expected'. Actual output:`n$Text"
    }
}

function Assert-NotContains {
    param(
        [string]$Name,
        [string]$Text,
        [string]$Unexpected
    )

    if ($Text -like "*$Unexpected*") {
        throw "$Name unexpectedly contained: $Unexpected`nActual:`n$Text"
    }
}

function Assert-LastExitCodeOk {
    param([Parameter(Mandatory = $true)][string]$StepName)

    if ($LASTEXITCODE -ne 0) {
        throw "$StepName failed (exit code $LASTEXITCODE)."
    }
}

function Invoke-V0PipelineCase {
    param(
        [string]$Name,
        [string]$InputFixture,
        [string]$DecisionOutput,
        [int64]$SequenceId,
        [int64]$NowUnixMs,
        [int]$ExpectedAppExitCode = 0,
        [string[]]$ExpectedAppOutput,
        [string[]]$ExpectedDecisionJson,
        [string[]]$ExpectedAuditJson = @(),
        [string[]]$UnexpectedAuditJson = @(),
        [string[]]$ExpectedBridgeOutput,
        [string[]]$ExpectedEffectBatch = @(),
        [switch]$ExpectNoEffectBatch
    )

    $decisionPath = Join-Path $repoRoot $DecisionOutput
    $auditPath = Join-Path $repoRoot "dist/$Name.audit.json"
    $effectBatchPath = Join-Path $repoRoot "dist/$Name.effect.txt"
    $sequenceStatePath = Join-Path $repoRoot "dist/$Name.sequence.txt"

    Remove-Item -LiteralPath $decisionPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $auditPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $effectBatchPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $sequenceStatePath -ErrorAction SilentlyContinue

    $appOutput = & $exePath `
        --v0-pipeline `
        (Join-Path $repoRoot $InputFixture) `
        $decisionPath `
        $SequenceId `
        $NowUnixMs `
        30000 `
        $auditPath
    $appExitCode = $LASTEXITCODE
    $appText = $appOutput -join "`n"

    if ($appExitCode -ne $ExpectedAppExitCode) {
        throw "$Name app failed. Expected exit code $ExpectedAppExitCode, got $appExitCode. Actual output:`n$appText"
    }

    foreach ($expected in $ExpectedAppOutput) {
        Assert-Contains -Name "$Name app" -Text $appText -Expected $expected
    }

    $decisionText = Get-Content -Raw -LiteralPath $decisionPath
    $null = $decisionText | ConvertFrom-Json
    foreach ($expected in $ExpectedDecisionJson) {
        Assert-Contains -Name "$Name decision" -Text $decisionText -Expected $expected
    }

    $auditText = Get-Content -Raw -LiteralPath $auditPath
    $null = $auditText | ConvertFrom-Json
    foreach ($expected in @(
        '"ministry_input":',
        '"processing_priority":',
        '"ministry_output":',
        '"clerk_input_brief":',
        '"final_payload":',
        '"sequence_id": ' + $SequenceId
    )) {
        Assert-Contains -Name "$Name audit" -Text $auditText -Expected $expected
    }
    foreach ($expected in $ExpectedAuditJson) {
        Assert-Contains -Name "$Name audit" -Text $auditText -Expected $expected
    }
    foreach ($unexpected in $UnexpectedAuditJson) {
        if ($auditText -match [regex]::Escape($unexpected)) {
            throw "$Name audit failed. Unexpected output '$unexpected'. Actual output:`n$auditText"
        }
    }

    $bridgeOutput = & $exePath `
        --bridge-pipeline `
        $decisionPath `
        $effectBatchPath `
        $sequenceStatePath `
        $NowUnixMs
    $bridgeExitCode = $LASTEXITCODE
    $bridgeText = $bridgeOutput -join "`n"

    foreach ($expected in $ExpectedBridgeOutput) {
        Assert-Contains -Name "$Name bridge" -Text $bridgeText -Expected $expected
    }

    if ($ExpectNoEffectBatch) {
        if (Test-Path -LiteralPath $effectBatchPath) {
            throw "$Name expected no effect batch file, but '$effectBatchPath' exists."
        }
        Write-Host "[PASS] $Name"
        return
    }

    $effectText = Get-Content -Raw -LiteralPath $effectBatchPath
    foreach ($expected in $ExpectedEffectBatch) {
        Assert-Contains -Name "$Name effect batch" -Text $effectText -Expected $expected
    }

    Write-Host "[PASS] $Name"
}

function Invoke-V0PipelineReadFailureCase {
    param(
        [string]$Name,
        [string]$InputFixture,
        [string]$ExpectedReason
    )

    $decisionPath = Join-Path $repoRoot "dist/$Name.decision.json"
    $auditPath = Join-Path $repoRoot "dist/$Name.audit.json"

    Remove-Item -LiteralPath $decisionPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $auditPath -ErrorAction SilentlyContinue

    $appOutput = & $exePath `
        --v0-pipeline `
        (Join-Path $repoRoot $InputFixture) `
        $decisionPath `
        201 `
        10000 `
        30000 `
        $auditPath
    $appExitCode = $LASTEXITCODE
    $appText = $appOutput -join "`n"

    if ($appExitCode -ne 1) {
        throw "$Name app failed. Expected exit code 1, got $appExitCode. Actual output:`n$appText"
    }

    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_success=false"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_reason=$ExpectedReason"

    if (Test-Path -LiteralPath $decisionPath) {
        throw "$Name expected no decision file, but '$decisionPath' exists."
    }
    if (Test-Path -LiteralPath $auditPath) {
        throw "$Name expected no audit file, but '$auditPath' exists."
    }

    Write-Host "[PASS] $Name"
}

function Invoke-V0PipelineOutputFailureCase {
    param(
        [string]$Name,
        [string]$InputFixture,
        [string]$ExpectedReason
    )

    $decisionPath = Join-Path $repoRoot "dist/$Name.output_dir"
    $auditPath = Join-Path $repoRoot "dist/$Name.audit.json"

    Remove-Item -LiteralPath $auditPath -ErrorAction SilentlyContinue
    if (-not (Test-Path -LiteralPath $decisionPath)) {
        New-Item -ItemType Directory -Force -Path $decisionPath | Out-Null
    }

    $appOutput = & $exePath `
        --v0-pipeline `
        (Join-Path $repoRoot $InputFixture) `
        $decisionPath `
        301 `
        10000 `
        30000 `
        $auditPath
    $appExitCode = $LASTEXITCODE
    $appText = $appOutput -join "`n"

    if ($appExitCode -ne 1) {
        throw "$Name app failed. Expected exit code 1, got $appExitCode. Actual output:`n$appText"
    }

    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_success=false"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_reason=$ExpectedReason"

    if (-not (Test-Path -LiteralPath $decisionPath -PathType Container)) {
        throw "$Name failed. Output directory was removed unexpectedly."
    }
    if (Test-Path -LiteralPath $auditPath) {
        throw "$Name expected no audit file, but '$auditPath' exists."
    }

    Write-Host "[PASS] $Name"
}

function Invoke-V0PipelineAuditFailureCase {
    param(
        [string]$Name,
        [string]$InputFixture
    )

    $decisionPath = Join-Path $repoRoot "dist/$Name.decision.json"
    $auditPath = Join-Path $repoRoot "dist/$Name.audit_dir"

    Remove-Item -LiteralPath $decisionPath -ErrorAction SilentlyContinue
    if (-not (Test-Path -LiteralPath $auditPath)) {
        New-Item -ItemType Directory -Force -Path $auditPath | Out-Null
    }

    $appOutput = & $exePath `
        --v0-pipeline `
        (Join-Path $repoRoot $InputFixture) `
        $decisionPath `
        302 `
        10000 `
        30000 `
        $auditPath
    $appExitCode = $LASTEXITCODE
    $appText = $appOutput -join "`n"

    if ($appExitCode -ne 0) {
        throw "$Name app failed. Expected exit code 0, got $appExitCode. Actual output:`n$appText"
    }

    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_success=true"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_decision_written=true"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_audit_requested=true"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_audit_written=false"
    Assert-Contains -Name "$Name app" -Text $appText -Expected "v0_pipeline_audit_reason=failed to write audit output"

    if (-not (Test-Path -LiteralPath $decisionPath -PathType Leaf)) {
        throw "$Name expected decision file '$decisionPath' to exist."
    }
    if (-not (Test-Path -LiteralPath $auditPath -PathType Container)) {
        throw "$Name failed. Audit directory was removed unexpectedly."
    }

    Write-Host "[PASS] $Name"
}

function Invoke-V0PriorityQueueCase {
    $queueOutputPath = Join-Path $repoRoot "dist/v0_priority_queue.json"
    Remove-Item -LiteralPath $queueOutputPath -ErrorAction SilentlyContinue

    $queueOutput = & $exePath `
        --v0-priority-queue `
        $queueOutputPath `
        (Join-Path $repoRoot "resources/ministry_context_military_defensive.json") `
        (Join-Path $repoRoot "resources/ministry_context_military_aggressive.json") `
        (Join-Path $repoRoot "resources/ministry_context_malformed.json")
    $queueExitCode = $LASTEXITCODE
    $queueText = $queueOutput -join "`n"

    if ($queueExitCode -ne 0) {
        throw "v0_priority_queue app failed. Actual output:`n$queueText"
    }

    foreach ($expected in @(
        "v0_priority_queue_success=true",
        "v0_priority_queue_entries=2",
        "v0_priority_queue_invalid_inputs=1",
        "v0_priority_queue_output_written=true"
    )) {
        Assert-Contains -Name "v0_priority_queue app" -Text $queueText -Expected $expected
    }

    $queueJson = Get-Content -Raw -LiteralPath $queueOutputPath
    $null = $queueJson | ConvertFrom-Json

    foreach ($expected in @(
        '"valid_entry_count": 2',
        '"empire_id": "empire_001"',
        '"tier": "critical"',
        'ministry_context_malformed.json: malformed ministry input context'
    )) {
        Assert-Contains -Name "v0_priority_queue json" -Text $queueJson -Expected $expected
    }

    Write-Host "[PASS] v0_priority_queue"
}

function Invoke-GeneratedOverlayCompileCase {
    $overlayOutputPath = Join-Path $repoRoot "dist/generated_overlay_valid"
    Remove-Item -LiteralPath $overlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    $compileOutput = & $exePath `
        --compile-generated-overlay `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $overlayOutputPath
    $compileExitCode = $LASTEXITCODE
    $compileText = $compileOutput -join "`n"

    if ($compileExitCode -ne 0) {
        throw "generated_overlay_compile app failed. Actual output:`n$compileText"
    }

    foreach ($expected in @(
        "generated_overlay_success=true",
        "generated_overlay_rule_count=1",
        "generated_overlay_output_path=$($overlayOutputPath.Replace('\','/'))",
        "generated_overlay_events_written=true",
        "generated_overlay_effects_written=true",
        "generated_overlay_triggers_written=true",
        "generated_overlay_manifest_written=true"
    )) {
        Assert-Contains -Name "generated_overlay_compile app" -Text $compileText -Expected $expected
    }

    $eventsText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "events/strategic_nexus_generated_events.txt")
    $effectsText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "common/scripted_effects/strategic_nexus_generated_effects.txt")
    $triggersText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "common/scripted_triggers/strategic_nexus_generated_triggers.txt")
    $manifestText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "strategic_nexus_generated_manifest.json")
    $baseKernelEventText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "mod/strategic_nexus_poc/events/strategic_nexus_poc_events.txt")
    $baseKernelOnActionsText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "mod/strategic_nexus_poc/common/on_actions/strategic_nexus_poc_on_actions.txt")
    $null = $manifestText | ConvertFrom-Json

    Assert-Contains -Name "generated_overlay_compile events" -Text $eventsText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = yes"
    Assert-Contains -Name "generated_overlay_compile effects" -Text $effectsText -Expected "set_country_flag = strategic_nexus_pref_military_posture_defensive"
    Assert-Contains -Name "generated_overlay_compile effects" -Text $effectsText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = {"
    Assert-Contains -Name "generated_overlay_compile triggers" -Text $triggersText -Expected "has_global_flag = strategic_nexus_campaign_campaign_001"
    Assert-Contains -Name "generated_overlay_compile base kernel" -Text $baseKernelEventText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = yes"
    Assert-Contains -Name "generated_overlay_compile base kernel" -Text $baseKernelEventText -Expected "strategic_nexus.1451"
    Assert-Contains -Name "generated_overlay_compile base kernel" -Text $baseKernelEventText -Expected "strategic_nexus_generated_war_started_dispatch = yes"
    Assert-Contains -Name "generated_overlay_compile base kernel" -Text $baseKernelEventText -Expected "strategic_nexus.1452"
    Assert-Contains -Name "generated_overlay_compile base kernel" -Text $baseKernelEventText -Expected "strategic_nexus_generated_country_attacked_dispatch = yes"
    Assert-Contains -Name "generated_overlay_compile base on_actions" -Text $baseKernelOnActionsText -Expected "strategic_nexus.1451"
    Assert-Contains -Name "generated_overlay_compile base on_actions" -Text $baseKernelOnActionsText -Expected "strategic_nexus.1452"
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"snapshot_kind": "complete_replacement"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"multiplayer_requirement": "byte_identical_gameplay_affecting_files"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"path": "common/scripted_effects/strategic_nexus_generated_effects.txt"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"hash_algorithm": "fnv1a64"'

    $monthlyReactiveCasePath = Join-Path $repoRoot "dist/generated_overlay_compile_monthly_reactive.dsl"
    $monthlyReactiveOutputPath = Join-Path $repoRoot "dist/generated_overlay_compile_monthly_reactive"
    @'
campaign "campaign_monthly" {
  empire "empire_monthly" {
    rule "monthly_dispatch_probe" {
      ministry = military_ministry
      event_family = monthly_strategy_tick
      when campaign_marker = campaign_monthly
      prefer military_posture defensive intensity 0.7
      duration = next_session
      confidence = 0.8
      rationale = "Monthly reactive dispatcher proof."
    }
  }
}
'@ | Set-Content -LiteralPath $monthlyReactiveCasePath -Encoding UTF8
    Remove-Item -LiteralPath $monthlyReactiveOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    $monthlyReactiveCompileOutput = & $exePath `
        --compile-generated-overlay `
        $monthlyReactiveCasePath `
        $monthlyReactiveOutputPath
    $monthlyReactiveCompileExitCode = $LASTEXITCODE
    $monthlyReactiveCompileText = $monthlyReactiveCompileOutput -join "`n"
    if ($monthlyReactiveCompileExitCode -ne 0) {
        throw "generated_overlay_compile monthly reactive app failed. Actual output:`n$monthlyReactiveCompileText"
    }

    $monthlyReactiveEventsText = Get-Content -Raw -LiteralPath (Join-Path $monthlyReactiveOutputPath "events/strategic_nexus_generated_events.txt")
    $monthlyReactiveEffectsText = Get-Content -Raw -LiteralPath (Join-Path $monthlyReactiveOutputPath "common/scripted_effects/strategic_nexus_generated_effects.txt")
    $monthlyReactiveManifestText = Get-Content -Raw -LiteralPath (Join-Path $monthlyReactiveOutputPath "strategic_nexus_generated_manifest.json")
    Assert-Contains -Name "generated_overlay_compile monthly events" -Text $monthlyReactiveEventsText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = yes"
    Assert-Contains -Name "generated_overlay_compile monthly effects" -Text $monthlyReactiveEffectsText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = {"
    Assert-Contains -Name "generated_overlay_compile monthly effects" -Text $monthlyReactiveEffectsText -Expected "strategic_nexus_generated_effect_campaign_monthly_empire_monthly_monthly_dispatch_probe = yes"
    Assert-Contains -Name "generated_overlay_compile monthly manifest" -Text $monthlyReactiveManifestText -Expected '"event_families": ["monthly_strategy_tick"]'

    $verifyOutput = & $exePath `
        --verify-generated-overlay `
        $overlayOutputPath
    $verifyExitCode = $LASTEXITCODE
    $verifyText = $verifyOutput -join "`n"

    if ($verifyExitCode -ne 0) {
        throw "generated_overlay_verify app failed. Actual output:`n$verifyText"
    }

    Assert-Contains -Name "generated_overlay_verify app" -Text $verifyText -Expected "generated_overlay_manifest_ok=true"
    Assert-Contains -Name "generated_overlay_verify app" -Text $verifyText -Expected "generated_overlay_manifest_reason=accepted"
    Assert-Contains -Name "generated_overlay_verify app" -Text $verifyText -Expected "generated_overlay_manifest_hash="
    Assert-Contains -Name "generated_overlay_verify app" -Text $verifyText -Expected "generated_overlay_manifest_file_count=3"

    $activeOverlayPath = Join-Path $repoRoot "dist/generated_overlay_active"
    Remove-Item -LiteralPath $activeOverlayPath -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $activeOverlayPath "stale") | Out-Null
    Set-Content -LiteralPath (Join-Path $activeOverlayPath "stale/old.txt") -Encoding UTF8 -Value "old"

    $blockedPublishOutput = & $exePath `
        --publish-generated-overlay `
        $overlayOutputPath `
        (Join-Path $repoRoot "dist/generated_overlay_blocked_active") `
        true
    $blockedPublishExitCode = $LASTEXITCODE
    $blockedPublishText = $blockedPublishOutput -join "`n"
    if ($blockedPublishExitCode -ne 1) {
        throw "generated_overlay_publish blocked app failed. Expected exit code 1, got $blockedPublishExitCode. Actual output:`n$blockedPublishText"
    }
    Assert-Contains -Name "generated_overlay_publish blocked app" -Text $blockedPublishText -Expected "generated_overlay_publish_ok=false"
    Assert-Contains -Name "generated_overlay_publish blocked app" -Text $blockedPublishText -Expected "generated_overlay_publish_reason=Stellaris is running; generated overlay publish deferred"

    $publishOutput = & $exePath `
        --publish-generated-overlay `
        $overlayOutputPath `
        $activeOverlayPath `
        false
    $publishExitCode = $LASTEXITCODE
    $publishText = $publishOutput -join "`n"
    if ($publishExitCode -ne 0) {
        throw "generated_overlay_publish app failed. Actual output:`n$publishText"
    }
    Assert-Contains -Name "generated_overlay_publish app" -Text $publishText -Expected "generated_overlay_publish_ok=true"
    Assert-Contains -Name "generated_overlay_publish app" -Text $publishText -Expected "generated_overlay_publish_reason=published"
    Assert-Contains -Name "generated_overlay_publish app" -Text $publishText -Expected "generated_overlay_publish_manifest_hash="
    Assert-Contains -Name "generated_overlay_publish app" -Text $publishText -Expected "generated_overlay_publish_file_count=3"
    if (Test-Path -LiteralPath (Join-Path $activeOverlayPath "stale/old.txt")) {
        throw "generated_overlay_publish should replace stale active overlay files."
    }

    $mpPackagePath = Join-Path $repoRoot "dist/generated_overlay_mp_package_cli"
    $mpImportedOverlayPath = Join-Path $repoRoot "dist/generated_overlay_mp_package_imported"
    Remove-Item -LiteralPath $mpPackagePath -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $mpImportedOverlayPath -Recurse -Force -ErrorAction SilentlyContinue

    $mpExportOutput = & $exePath `
        --export-mp-overlay-package `
        $overlayOutputPath `
        "campaign_cli" `
        "overlay_cli_v1" `
        "stellaris_4.x" `
        "strategic_nexus_v0" `
        $mpPackagePath `
        false
    $mpExportExitCode = $LASTEXITCODE
    $mpExportText = $mpExportOutput -join "`n"
    if ($mpExportExitCode -ne 0) {
        throw "mp_overlay_package_export app failed. Actual output:`n$mpExportText"
    }
    Assert-Contains -Name "mp_overlay_package_export app" -Text $mpExportText -Expected "mp_overlay_package_export_ok=true"
    Assert-Contains -Name "mp_overlay_package_export app" -Text $mpExportText -Expected "mp_overlay_package_export_reason=accepted_degraded_previous_host_unavailable"
    Assert-Contains -Name "mp_overlay_package_export app" -Text $mpExportText -Expected "mp_overlay_package_export_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_export app" -Text $mpExportText -Expected "mp_overlay_package_export_manifest_hash="
    Assert-Contains -Name "mp_overlay_package_export app command" -Text $mpExportText -Expected "mp_overlay_package_export_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_export app command" -Text $mpExportText -Expected "mp_overlay_package_export_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_export app command" -Text $mpExportText -Expected "mp_overlay_package_export_strict_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_export app command" -Text $mpExportText -Expected "mp_overlay_package_export_strict_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_export app readiness" -Text $mpExportText -Expected "mp_overlay_package_export_host_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_export app readiness" -Text $mpExportText -Expected "mp_overlay_package_export_client_readiness_gate=import_and_verify_before_join"
    Assert-Contains -Name "mp_overlay_package_export app next_step" -Text $mpExportText -Expected "mp_overlay_package_export_host_next_step=share this package and package_manifest_hash with every joining player"
    Assert-Contains -Name "mp_overlay_package_export app next_step" -Text $mpExportText -Expected "mp_overlay_package_export_client_next_step=import package, verify package_manifest_hash, then join lobby"
    Assert-Contains -Name "mp_overlay_package_export app recovery" -Text $mpExportText -Expected "mp_overlay_package_export_handoff_recovery_hint="
    Assert-Contains -Name "mp_overlay_package_export app identity warning" -Text $mpExportText -Expected "mp_overlay_package_export_identity_mismatch_warning=false"
    Assert-Contains -Name "mp_overlay_package_export app warning count" -Text $mpExportText -Expected "mp_overlay_package_export_warning_count=0"

    $mpVerifyOutput = & $exePath `
        --verify-mp-overlay-package `
        $mpPackagePath
    $mpVerifyExitCode = $LASTEXITCODE
    $mpVerifyText = $mpVerifyOutput -join "`n"
    if ($mpVerifyExitCode -ne 0) {
        throw "mp_overlay_package_verify app failed. Actual output:`n$mpVerifyText"
    }
    Assert-Contains -Name "mp_overlay_package_verify app" -Text $mpVerifyText -Expected "mp_overlay_package_ok=true"
    Assert-Contains -Name "mp_overlay_package_verify app" -Text $mpVerifyText -Expected "mp_overlay_package_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_verify app" -Text $mpVerifyText -Expected "mp_overlay_package_manifest_hash="
    Assert-Contains -Name "mp_overlay_package_verify app command" -Text $mpVerifyText -Expected "mp_overlay_package_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_verify app command" -Text $mpVerifyText -Expected "mp_overlay_package_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_verify app command" -Text $mpVerifyText -Expected "mp_overlay_package_strict_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_verify app command" -Text $mpVerifyText -Expected "mp_overlay_package_strict_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_verify app readiness" -Text $mpVerifyText -Expected "mp_overlay_package_host_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_verify app readiness" -Text $mpVerifyText -Expected "mp_overlay_package_client_readiness_gate=import_and_verify_before_join"
    Assert-Contains -Name "mp_overlay_package_verify app next_step" -Text $mpVerifyText -Expected "mp_overlay_package_host_next_step=share this package and package_manifest_hash with every joining player"
    Assert-Contains -Name "mp_overlay_package_verify app next_step" -Text $mpVerifyText -Expected "mp_overlay_package_client_next_step=import package, verify package_manifest_hash, then join lobby"
    Assert-Contains -Name "mp_overlay_package_verify app recovery" -Text $mpVerifyText -Expected "mp_overlay_package_handoff_recovery_hint="
    Assert-Contains -Name "mp_overlay_package_verify app identity warning" -Text $mpVerifyText -Expected "mp_overlay_package_identity_mismatch_warning=false"
    $mpManifestHash = [regex]::Match($mpVerifyText, "mp_overlay_package_manifest_hash=([^\r\n]+)").Groups[1].Value.Trim()
    if ([string]::IsNullOrWhiteSpace($mpManifestHash)) {
        throw "mp_overlay_package_verify app did not provide manifest hash value."
    }

    $mpVerifyMismatchOutput = & $exePath `
        --verify-mp-overlay-package `
        $mpPackagePath `
        "campaign_cli" `
        "overlay_cli_v1" `
        "stellaris_4.x" `
        "strategic_nexus_v0" `
        "wrong_manifest_hash"
    $mpVerifyMismatchExitCode = $LASTEXITCODE
    $mpVerifyMismatchText = $mpVerifyMismatchOutput -join "`n"
    if ($mpVerifyMismatchExitCode -ne 0) {
        throw "mp_overlay_package_verify mismatch app failed. Actual output:`n$mpVerifyMismatchText"
    }
    Assert-Contains -Name "mp_overlay_package_verify mismatch app" -Text $mpVerifyMismatchText -Expected "mp_overlay_package_ok=true"
    Assert-Contains -Name "mp_overlay_package_verify mismatch app" -Text $mpVerifyMismatchText -Expected "mp_overlay_package_warning=package_manifest_hash_mismatch"
    Assert-Contains -Name "mp_overlay_package_verify mismatch app warning_code" -Text $mpVerifyMismatchText -Expected "mp_overlay_package_warning_code=package_manifest_hash_mismatch"
    Assert-Contains -Name "mp_overlay_package_verify mismatch identity warning" -Text $mpVerifyMismatchText -Expected "mp_overlay_package_identity_mismatch_warning=true"
    Assert-Contains -Name "mp_overlay_package_verify mismatch identity warning_code" -Text $mpVerifyMismatchText -Expected "mp_overlay_package_identity_mismatch_warning_code=package_manifest_hash_mismatch"

    $mpImportOutput = & $exePath `
        --import-mp-overlay-package `
        $mpPackagePath `
        $mpImportedOverlayPath
    $mpImportExitCode = $LASTEXITCODE
    $mpImportText = $mpImportOutput -join "`n"
    if ($mpImportExitCode -ne 0) {
        throw "mp_overlay_package_import app failed. Actual output:`n$mpImportText"
    }
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_ok=true"
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_reason=accepted"
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_campaign_id=campaign_cli"
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_overlay_version=overlay_cli_v1"
    Assert-Contains -Name "mp_overlay_package_import app" -Text $mpImportText -Expected "mp_overlay_package_import_warning_count=0"
    Assert-Contains -Name "mp_overlay_package_import app command" -Text $mpImportText -Expected "mp_overlay_package_import_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_import app command" -Text $mpImportText -Expected "mp_overlay_package_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_import app command" -Text $mpImportText -Expected "mp_overlay_package_import_strict_verify_command=Strategic Nexus.exe --verify-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_import app command" -Text $mpImportText -Expected "mp_overlay_package_import_strict_import_command=Strategic Nexus.exe --import-mp-overlay-package "
    Assert-Contains -Name "mp_overlay_package_import app readiness" -Text $mpImportText -Expected "mp_overlay_package_import_host_readiness=ready_for_mp"
    Assert-Contains -Name "mp_overlay_package_import app readiness" -Text $mpImportText -Expected "mp_overlay_package_import_client_readiness_gate=import_and_verify_before_join"
    Assert-Contains -Name "mp_overlay_package_import app next_step" -Text $mpImportText -Expected "mp_overlay_package_import_host_next_step=share this package and package_manifest_hash with every joining player"
    Assert-Contains -Name "mp_overlay_package_import app next_step" -Text $mpImportText -Expected "mp_overlay_package_import_client_next_step=import package, verify package_manifest_hash, then join lobby"
    Assert-Contains -Name "mp_overlay_package_import app recovery" -Text $mpImportText -Expected "mp_overlay_package_import_handoff_recovery_hint="
    Assert-Contains -Name "mp_overlay_package_import app identity warning" -Text $mpImportText -Expected "mp_overlay_package_import_identity_mismatch_warning=false"

    $mpImportMismatchOutput = & $exePath `
        --import-mp-overlay-package `
        $mpPackagePath `
        $mpImportedOverlayPath `
        "campaign_cli" `
        "overlay_cli_v1" `
        "stellaris_4.x" `
        "strategic_nexus_v0" `
        "wrong_manifest_hash"
    $mpImportMismatchExitCode = $LASTEXITCODE
    $mpImportMismatchText = $mpImportMismatchOutput -join "`n"
    if ($mpImportMismatchExitCode -ne 0) {
        throw "mp_overlay_package_import mismatch app failed. Actual output:`n$mpImportMismatchText"
    }
    Assert-Contains -Name "mp_overlay_package_import mismatch app" -Text $mpImportMismatchText -Expected "mp_overlay_package_import_ok=true"
    Assert-Contains -Name "mp_overlay_package_import mismatch app" -Text $mpImportMismatchText -Expected "mp_overlay_package_import_warning=package_manifest_hash_mismatch"
    Assert-Contains -Name "mp_overlay_package_import mismatch app warning_code" -Text $mpImportMismatchText -Expected "mp_overlay_package_import_warning_code=package_manifest_hash_mismatch"
    Assert-Contains -Name "mp_overlay_package_import mismatch identity warning" -Text $mpImportMismatchText -Expected "mp_overlay_package_import_identity_mismatch_warning=true"
    Assert-Contains -Name "mp_overlay_package_import mismatch identity warning_code" -Text $mpImportMismatchText -Expected "mp_overlay_package_import_identity_mismatch_warning_code=package_manifest_hash_mismatch"

    $mpImportedVerifyOutput = & $exePath `
        --verify-generated-overlay `
        $mpImportedOverlayPath
    $mpImportedVerifyExitCode = $LASTEXITCODE
    $mpImportedVerifyText = $mpImportedVerifyOutput -join "`n"
    if ($mpImportedVerifyExitCode -ne 0) {
        throw "mp imported generated_overlay_verify app failed. Actual output:`n$mpImportedVerifyText"
    }
    Assert-Contains -Name "mp imported generated_overlay_verify app" -Text $mpImportedVerifyText -Expected "generated_overlay_manifest_ok=true"
    Assert-Contains -Name "mp imported generated_overlay_verify app" -Text $mpImportedVerifyText -Expected "generated_overlay_manifest_reason=accepted"

    Write-Host "[PASS] generated_overlay_compile"
}

function Invoke-GeneratedOverlayVerifyMismatchCase {
    $overlayOutputPath = Join-Path $repoRoot "dist/generated_overlay_verify_mismatch"
    Remove-Item -LiteralPath $overlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    $compileOutput = & $exePath `
        --compile-generated-overlay `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $overlayOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "generated_overlay_verify_mismatch compile failed. Actual output:`n$($compileOutput -join "`n")"
    }

    Add-Content -LiteralPath (Join-Path $overlayOutputPath "events/strategic_nexus_generated_events.txt") -Value "# drift"

    $verifyOutput = & $exePath `
        --verify-generated-overlay `
        $overlayOutputPath
    $verifyExitCode = $LASTEXITCODE
    $verifyText = $verifyOutput -join "`n"

    if ($verifyExitCode -ne 1) {
        throw "generated_overlay_verify_mismatch app failed. Expected exit code 1, got $verifyExitCode. Actual output:`n$verifyText"
    }

    Assert-Contains -Name "generated_overlay_verify_mismatch app" -Text $verifyText -Expected "generated_overlay_manifest_ok=false"
    Assert-Contains -Name "generated_overlay_verify_mismatch app" -Text $verifyText -Expected "generated_overlay_manifest_reason=generated overlay files do not match manifest"
    Assert-Contains -Name "generated_overlay_verify_mismatch app" -Text $verifyText -Expected "events/strategic_nexus_generated_events.txt;exists=true;hash_matches=false"

    Write-Host "[PASS] generated_overlay_verify_mismatch"
}

function Invoke-GeneratedOverlayInvalidCase {
    $overlayOutputPath = Join-Path $repoRoot "dist/generated_overlay_invalid"
    Remove-Item -LiteralPath $overlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    $compileOutput = & $exePath `
        --compile-generated-overlay `
        (Join-Path $repoRoot "resources/generated_overlay_invalid.dsl") `
        $overlayOutputPath
    $compileExitCode = $LASTEXITCODE
    $compileText = $compileOutput -join "`n"

    if ($compileExitCode -ne 1) {
        throw "generated_overlay_invalid app failed. Expected exit code 1, got $compileExitCode. Actual output:`n$compileText"
    }

    Assert-Contains -Name "generated_overlay_invalid app" -Text $compileText -Expected "generated_overlay_success=false"
    Assert-Contains -Name "generated_overlay_invalid app" -Text $compileText -Expected "generated_overlay_reason=validation failed"
    Assert-Contains -Name "generated_overlay_invalid app" -Text $compileText -Expected "unsupported preference"

    if (Test-Path -LiteralPath $overlayOutputPath) {
        throw "generated_overlay_invalid should not write an overlay directory."
    }

    Write-Host "[PASS] generated_overlay_invalid"
}

function Invoke-CampaignSaveScanCase {
    $saveRoot = Join-Path $repoRoot "dist/campaign_save_scan_cli_fixture"
    $inventoryPath = Join-Path $repoRoot "dist/campaign_save_inventory.json"
    Remove-Item -LiteralPath $saveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $inventoryPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path (Join-Path $saveRoot "Gamma Campaign") | Out-Null
    Set-Content -LiteralPath (Join-Path $saveRoot "Gamma Campaign/autosave_2230.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $saveRoot "Loose Campaign.sav") -Value "fixture" -Encoding UTF8

    $scanOutput = & $exePath `
        --scan-save-campaigns `
        $saveRoot `
        $inventoryPath
    $scanExitCode = $LASTEXITCODE
    $scanText = $scanOutput -join "`n"

    if ($scanExitCode -ne 0) {
        throw "campaign_save_scan app failed. Actual output:`n$scanText"
    }

    Assert-Contains -Name "campaign_save_scan app" -Text $scanText -Expected "save_campaign_scan_success=true"
    Assert-Contains -Name "campaign_save_scan app" -Text $scanText -Expected "save_campaign_scan_root_exists=true"
    Assert-Contains -Name "campaign_save_scan app" -Text $scanText -Expected "save_campaign_scan_campaign_count=2"

    $inventoryText = Get-Content -Raw -LiteralPath $inventoryPath
    $null = $inventoryText | ConvertFrom-Json
    Assert-Contains -Name "campaign_save_scan inventory" -Text $inventoryText -Expected '"campaign_key": "gamma_campaign"'
    Assert-Contains -Name "campaign_save_scan inventory" -Text $inventoryText -Expected '"campaign_key": "loose_campaign"'

    Write-Host "[PASS] campaign_save_scan"
}

function Invoke-CampaignSaveDiffCase {
    $previousRoot = Join-Path $repoRoot "dist/campaign_save_diff_previous_fixture"
    $currentRoot = Join-Path $repoRoot "dist/campaign_save_diff_current_fixture"
    $diffPath = Join-Path $repoRoot "dist/campaign_save_inventory_diff.json"
    Remove-Item -LiteralPath $previousRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $currentRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $diffPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path (Join-Path $previousRoot "Alpha Campaign") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $currentRoot "Alpha Campaign Renamed") | Out-Null
    Set-Content -LiteralPath (Join-Path $previousRoot "Alpha Campaign/autosave_2230.sav") -Value "alpha-anchor" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Alpha Campaign Renamed/autosave_2230.sav") -Value "alpha-anchor" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $previousRoot "Removed.sav") -Value "removed-loose" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Added.sav") -Value "added-loose" -Encoding UTF8

    $diffOutput = & $exePath `
        --diff-save-campaigns `
        $previousRoot `
        $currentRoot `
        $diffPath
    $diffExitCode = $LASTEXITCODE
    $diffText = $diffOutput -join "`n"

    if ($diffExitCode -ne 0) {
        throw "campaign_save_diff app failed. Actual output:`n$diffText"
    }

    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_success=true"
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_added=1"
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_removed=1"
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_renamed=1"
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_restored=0"
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_changed=0"

    $inventoryDiffText = Get-Content -Raw -LiteralPath $diffPath
    $null = $inventoryDiffText | ConvertFrom-Json
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "added"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "removed"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "renamed"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"previous_relative_path": "Alpha Campaign"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"current_relative_path": "Alpha Campaign Renamed"'

    Write-Host "[PASS] campaign_save_diff"
}

function Invoke-CampaignSaveRestoreContinuityCase {
    $previousRoot = Join-Path $repoRoot "dist/campaign_save_diff_previous_restore"
    $currentRoot = Join-Path $repoRoot "dist/campaign_save_diff_current_restore"
    $diffPath = Join-Path $repoRoot "dist/campaign_save_diff_restore.json"
    Remove-Item -LiteralPath $previousRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $currentRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $diffPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path (Join-Path $previousRoot "Alpha Campaign") | Out-Null
    New-Item -ItemType Directory -Force -Path $currentRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $previousRoot "Alpha Campaign/autosave_2230.sav") -Value "alpha-anchor" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $previousRoot "Alpha Campaign/ironman.sav") -Value "alpha-ironman" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $previousRoot "Removed.sav") -Value "removed-loose" -Encoding UTF8

    Set-Content -LiteralPath (Join-Path $currentRoot "Alpha Campaign Restored.sav") -Value "alpha-anchor" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Gamma.sav") -Value "gamma-loose" -Encoding UTF8

    $diffOutput = & $exePath `
        --diff-save-campaigns `
        $previousRoot `
        $currentRoot `
        $diffPath
    $diffExitCode = $LASTEXITCODE
    $diffText = $diffOutput -join "`n"

    if ($diffExitCode -ne 0) {
        throw "campaign_save_restore_diff app failed. Actual output:`n$diffText"
    }

    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_success=true"
    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_added=1"
    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_removed=1"
    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_renamed=0"
    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_restored=1"
    Assert-Contains -Name "campaign_save_restore_diff app" -Text $diffText -Expected "save_campaign_diff_changed=0"

    $inventoryDiffText = Get-Content -Raw -LiteralPath $diffPath
    $null = $inventoryDiffText | ConvertFrom-Json
    Assert-Contains -Name "campaign_save_restore_diff json" -Text $inventoryDiffText -Expected '"restored_count": 1'
    Assert-Contains -Name "campaign_save_restore_diff json" -Text $inventoryDiffText -Expected '"change_kind": "restored"'
    Assert-Contains -Name "campaign_save_restore_diff json" -Text $inventoryDiffText -Expected '"previous_source_kind": "campaign_directory"'
    Assert-Contains -Name "campaign_save_restore_diff json" -Text $inventoryDiffText -Expected '"current_source_kind": "loose_save"'

    Write-Host "[PASS] campaign_save_restore_diff"
}

function Invoke-CampaignLibraryPlanCase {
    $saveRoot = Join-Path $repoRoot "dist/campaign_library_plan_fixture"
    $planPath = Join-Path $repoRoot "dist/campaign_library_plan.json"
    Remove-Item -LiteralPath $saveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $planPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $saveRoot "Alpha.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $saveRoot "Beta.sav") -Value "fixture" -Encoding UTF8

    $planOutput = & $exePath `
        --plan-campaign-library `
        $saveRoot `
        1 `
        $planPath
    $planExitCode = $LASTEXITCODE
    $planText = $planOutput -join "`n"

    if ($planExitCode -ne 0) {
        throw "campaign_library_plan app failed. Actual output:`n$planText"
    }

    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_success=true"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_root_exists=true"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_limit_reached=true"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_included=1"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_skipped=1"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_skipped_due_to_limit=1"

    $planJson = Get-Content -Raw -LiteralPath $planPath
    $null = $planJson | ConvertFrom-Json
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"save_root_available": true'
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"limit_reached": true'
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"status": "included"'
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"skipped_due_to_limit_count": 1'
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"reason": "active_library_limit"'

    Write-Host "[PASS] campaign_library_plan"
}

function Invoke-CampaignLibraryOverlayCase {
    $saveRoot = Join-Path $repoRoot "dist/campaign_library_overlay_saves"
    $dslPath = Join-Path $repoRoot "dist/campaign_library_overlay.dsl"
    $overlayOutputPath = Join-Path $repoRoot "dist/campaign_library_overlay"
    $nonEmptyOverlayOutputPath = Join-Path $repoRoot "dist/campaign_library_overlay_nonempty"
    Remove-Item -LiteralPath $saveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $dslPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $overlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $nonEmptyOverlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $saveRoot "Alpha.sav") -Value "fixture" -Encoding UTF8
    @'
campaign "alpha" {
  empire "empire_001" {
    rule "local_defense" {
      ministry = military_ministry
      when campaign_marker
      prefer military_posture defensive intensity 0.7
      duration = next_session
      confidence = 0.72
      rationale = "Local campaign rule."
    }
  }
}
campaign "missing" {
  empire "empire_001" {
    rule "missing_aggression" {
      ministry = military_ministry
      when campaign_marker
      prefer military_posture aggressive intensity 0.7
      duration = next_session
      confidence = 0.72
      rationale = "Unavailable campaign rule."
    }
  }
}
'@ | Set-Content -LiteralPath $dslPath -Encoding ASCII

    $overlayOutput = & $exePath `
        --compile-campaign-library-overlay `
        $dslPath `
        $saveRoot `
        4 `
        $overlayOutputPath
    $overlayExitCode = $LASTEXITCODE
    $overlayText = $overlayOutput -join "`n"

    if ($overlayExitCode -ne 0) {
        throw "campaign_library_overlay app failed. Actual output:`n$overlayText"
    }

    Assert-Contains -Name "campaign_library_overlay app" -Text $overlayText -Expected "campaign_library_overlay_success=true"
    Assert-Contains -Name "campaign_library_overlay app" -Text $overlayText -Expected "campaign_library_overlay_rules_included=1"
    Assert-Contains -Name "campaign_library_overlay app" -Text $overlayText -Expected "campaign_library_overlay_rules_skipped=1"
    Assert-Contains -Name "campaign_library_overlay app" -Text $overlayText -Expected "campaign_library_overlay_limit_reached=false"
    Assert-Contains -Name "campaign_library_overlay app" -Text $overlayText -Expected "campaign_library_overlay_campaigns_skipped_due_to_limit=0"

    $eventsText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "events/strategic_nexus_generated_events.txt")
    $effectsText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "common/scripted_effects/strategic_nexus_generated_effects.txt")
    $planText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "strategic_nexus_campaign_library_plan.json")
    $null = $planText | ConvertFrom-Json
    Assert-Contains -Name "campaign_library_overlay events" -Text $eventsText -Expected "strategic_nexus_generated_monthly_strategy_tick_dispatch = yes"
    Assert-Contains -Name "campaign_library_overlay effects" -Text $effectsText -Expected "strategic_nexus_generated_effect_alpha_empire_001_local_defense = {"
    Assert-NotContains -Name "campaign_library_overlay effects" -Text $effectsText -Unexpected "missing_aggression"
    Assert-Contains -Name "campaign_library_overlay plan" -Text $planText -Expected '"campaign_key": "alpha"'
    Assert-Contains -Name "campaign_library_overlay plan" -Text $planText -Expected '"limit_reached": false'

    New-Item -ItemType Directory -Force -Path $nonEmptyOverlayOutputPath | Out-Null
    Set-Content -LiteralPath (Join-Path $nonEmptyOverlayOutputPath "stale.txt") -Value "stale" -Encoding UTF8

    $nonEmptyOverlayOutput = & $exePath `
        --compile-campaign-library-overlay `
        $dslPath `
        $saveRoot `
        4 `
        $nonEmptyOverlayOutputPath
    $nonEmptyOverlayExitCode = $LASTEXITCODE
    $nonEmptyOverlayText = $nonEmptyOverlayOutput -join "`n"

    if ($nonEmptyOverlayExitCode -eq 0) {
        throw "campaign_library_overlay app unexpectedly accepted non-empty output directory. Actual output:`n$nonEmptyOverlayText"
    }

    Assert-Contains -Name "campaign_library_overlay nonempty app" -Text $nonEmptyOverlayText -Expected "campaign_library_overlay_success=false"
    Assert-Contains -Name "campaign_library_overlay nonempty app" -Text $nonEmptyOverlayText -Expected "campaign_library_overlay_reason=output directory must be empty"
    if (-not (Test-Path -LiteralPath (Join-Path $nonEmptyOverlayOutputPath "stale.txt"))) {
        throw "campaign_library_overlay app removed pre-existing non-empty output directory unexpectedly."
    }

    $missingRootPath = Join-Path $repoRoot "dist/campaign_library_overlay_missing_root_saves"
    $missingRootOutputPath = Join-Path $repoRoot "dist/campaign_library_overlay_missing_root"
    Remove-Item -LiteralPath $missingRootPath -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $missingRootOutputPath -Recurse -Force -ErrorAction SilentlyContinue

    $missingRootOutput = & $exePath `
        --compile-campaign-library-overlay `
        $dslPath `
        $missingRootPath `
        4 `
        $missingRootOutputPath
    $missingRootExitCode = $LASTEXITCODE
    $missingRootText = $missingRootOutput -join "`n"

    if ($missingRootExitCode -eq 0) {
        throw "campaign_library_overlay app unexpectedly accepted unavailable save root. Actual output:`n$missingRootText"
    }

    Assert-Contains -Name "campaign_library_overlay missing-root app" -Text $missingRootText -Expected "campaign_library_overlay_success=false"
    Assert-Contains -Name "campaign_library_overlay missing-root app" -Text $missingRootText -Expected "campaign_library_overlay_reason=save root unavailable"
    if (Test-Path -LiteralPath $missingRootOutputPath) {
        throw "campaign_library_overlay app created output for unavailable save root unexpectedly."
    }

    Write-Host "[PASS] campaign_library_overlay"
}

function Invoke-StellarisSaveRootDiscoveryCase {
    $outputPath = Join-Path $repoRoot "dist/stellaris_save_roots.json"
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue

    $discoveryOutput = & $exePath `
        --discover-stellaris-save-roots `
        $outputPath
    $discoveryExitCode = $LASTEXITCODE
    $discoveryText = $discoveryOutput -join "`n"

    if ($discoveryExitCode -ne 0) {
        throw "stellaris_save_roots app failed. Actual output:`n$discoveryText"
    }

    Assert-Contains -Name "stellaris_save_roots app" -Text $discoveryText -Expected "stellaris_save_roots_success=true"
    Assert-Contains -Name "stellaris_save_roots app" -Text $discoveryText -Expected "stellaris_save_roots_output_written=true"

    $discoveryJson = Get-Content -Raw -LiteralPath $outputPath
    $null = $discoveryJson | ConvertFrom-Json
    Assert-Contains -Name "stellaris_save_roots json" -Text $discoveryJson -Expected '"schema_version": 1'
    Assert-Contains -Name "stellaris_save_roots json" -Text $discoveryJson -Expected '"candidates":'

    Write-Host "[PASS] stellaris_save_roots"
}

function Invoke-StellarisProcessDetectionCase {
    $outputPath = Join-Path $repoRoot "dist/stellaris_running.json"
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue

    $detectionOutput = & $exePath `
        --detect-stellaris-running `
        $outputPath
    $detectionExitCode = $LASTEXITCODE
    $detectionText = $detectionOutput -join "`n"

    if ($detectionExitCode -ne 0) {
        throw "stellaris_running app failed. Actual output:`n$detectionText"
    }

    Assert-Contains -Name "stellaris_running app" -Text $detectionText -Expected "stellaris_process_detection_available=true"
    Assert-Contains -Name "stellaris_running app" -Text $detectionText -Expected "stellaris_running="
    Assert-Contains -Name "stellaris_running app" -Text $detectionText -Expected "stellaris_running_output_written=true"

    $detectionJson = Get-Content -Raw -LiteralPath $outputPath
    $null = $detectionJson | ConvertFrom-Json
    Assert-Contains -Name "stellaris_running json" -Text $detectionJson -Expected '"schema_version": 1'
    Assert-Contains -Name "stellaris_running json" -Text $detectionJson -Expected '"running":'

    Write-Host "[PASS] stellaris_running"
}

function Invoke-SncStatusSnapshotCase {
    $archiveRoot = Join-Path $repoRoot "dist/snc_status_archive"
    $overlayRoot = Join-Path $repoRoot "dist/snc_status_overlay"
    $postPlayStatusRoot = Join-Path $repoRoot "dist/snc_status_post_play"
    $mpPackageRoot = Join-Path $repoRoot "dist/snc_status_mp_package"
    $outputPath = Join-Path $repoRoot "dist/snc_status_snapshot.json"
    $mpOutputPath = Join-Path $repoRoot "dist/snc_status_snapshot_with_mp.json"
    $supportReportPreviewPath = Join-Path $repoRoot "dist/snc_support_report_preview.txt"
    $defaultSupportReportPreviewPath = Join-Path $repoRoot "dist/private_reports/snc_support_report_preview.txt"
    $stagingStatusPath = Join-Path $postPlayStatusRoot "generated_overlay_staging_status.json"
    $campaignLibraryPlanPath = Join-Path $postPlayStatusRoot "strategic_nexus_campaign_library_plan.json"
    $campaignLibraryPlanCliPath = $campaignLibraryPlanPath -replace '\\','/'
    $mpPackageZipPath = Join-Path $repoRoot "dist/snc_mp_overlay_package.zip"
    $mpPackageZipCliPath = $mpPackageZipPath -replace '\\','/'
    Remove-Item -LiteralPath $archiveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $overlayRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $postPlayStatusRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $mpPackageRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $mpOutputPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $defaultSupportReportPreviewPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $archiveRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $overlayRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $postPlayStatusRoot | Out-Null

    $compileOutput = & $exePath `
        --compile-generated-overlay `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $overlayRoot
    if ($LASTEXITCODE -ne 0) {
        $compileText = $compileOutput -join "`n"
        throw "snc_status_snapshot overlay compile failed. Actual output:`n$compileText"
    }

    @'
{
  "schema_version": 1,
  "save_root_available": true,
  "limit_reached": true,
  "max_included_campaigns": 1,
  "included_count": 1,
  "skipped_count": 2,
  "skipped_due_to_limit_count": 2,
  "campaigns": []
}
'@ | Set-Content -LiteralPath $campaignLibraryPlanPath -Encoding ascii

    $sncOutput = & $exePath `
        --snc-status-snapshot `
        $archiveRoot `
        $overlayRoot `
        $outputPath `
        true `
        false `
        false `
        $stagingStatusPath
    $sncExitCode = $LASTEXITCODE
    $sncText = $sncOutput -join "`n"

    if ($sncExitCode -ne 0) {
        throw "snc_status_snapshot app failed. Actual output:`n$sncText"
    }

    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_success=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_app_name=Strategic Nexus Companion"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_abbreviation=SNC"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_startup_lifecycle_state=owner_enabled_start_with_windows"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_state=not_prepared"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_reason=prepare local support report preview before manual review or send"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_contact_email=support@jeniksoft.cz"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_send_requires_approval=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_raw_saves_included=false"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_support_report_prepare_command_hint=powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_snc_support_report.ps1"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_enabled=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_source=config_override"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_shortcut_state=override_enabled"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_command_hint=cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_command_hint_source=startup_shortcut_remove_command"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_enable_command_hint=cmd /c tools\install_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_start_with_windows_disable_command_hint=cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_window_close_behavior=minimize_to_tray"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_explicit_exit_behavior=stop_without_restart"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_crash_restart_policy=bounded_backoff_with_crash_loop_guard"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_archive_state=starting"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_state=ready"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_manifest_hash="
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_reactive_capability=post_session_only"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_event_family_count=0"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_source_quality_count=1"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_source_quality=history_backed"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_bootstrap_campaign_count=0"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_publish_gate_state=needs_setup"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_publish_gate_reason=generated overlay publish status path not configured"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_plan_present=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_plan_path=$campaignLibraryPlanCliPath"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_plan_source=staging_status_directory"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_plan_readiness=ready"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_plan_reason=campaign library plan loaded"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_limit_reached=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_campaign_library_skipped_due_to_limit_count=2"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_center_state=attention_required"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_owner_test_playbook_path="
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_output_written=true"

    $snapshotJson = Get-Content -Raw -LiteralPath $outputPath
    $null = $snapshotJson | ConvertFrom-Json
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"app_name": "Strategic Nexus Companion"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"abbreviation": "SNC"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"support_report_state": "not_prepared"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"support_report_contact_email": "support@jeniksoft.cz"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"support_report_send_requires_approval": true'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"support_report_raw_saves_included": false'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"support_report_prepare_command_hint": "powershell -NoProfile -ExecutionPolicy Bypass -File tools\\prepare_snc_support_report.ps1"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"window_close_behavior": "minimize_to_tray"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"manifest_hash": "'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"reactive_policy_pack_capability": "post_session_only"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"source_qualities": ["history_backed"]'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"bootstrap_campaign_count": 0'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"generated_overlay_publish_gate_status":'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"campaign_library_plan_readiness": "ready"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"campaign_library_skipped_due_to_limit_count": 2'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"owner_test_playbook_path": ""'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"status_center":'

    Remove-Item -LiteralPath $supportReportPreviewPath -Force -ErrorAction SilentlyContinue
    $supportPreviewOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\prepare_snc_support_report.ps1") -StatusPath $outputPath -OutputPath $supportReportPreviewPath
    if ($LASTEXITCODE -ne 0) {
        $supportPreviewText = $supportPreviewOutput -join "`n"
        throw "snc support report preview generation failed. Actual output:`n$supportPreviewText"
    }
    if (-not (Test-Path -LiteralPath $supportReportPreviewPath)) {
        throw "snc support report preview generation did not create $supportReportPreviewPath"
    }
    $supportReportPreviewText = Get-Content -Raw -LiteralPath $supportReportPreviewPath
    Assert-Contains -Name "snc support report preview" -Text $supportReportPreviewText -Expected "support@jeniksoft.cz"
    Assert-Contains -Name "snc support report preview" -Text $supportReportPreviewText -Expected "explicit owner approval required"
    Assert-Contains -Name "snc support report preview" -Text $supportReportPreviewText -Expected "Raw saves included: no"
    Assert-Contains -Name "snc support report preview output" -Text ($supportPreviewOutput -join "`n") -Expected "support_report_open_command_hint=cmd /c start"

    $mpExportOutput = & $exePath `
        --export-mp-overlay-package `
        $overlayRoot `
        "campaign_mp_status_case" `
        "overlay_v1" `
        "stellaris_4.x" `
        "strategic_nexus_v0" `
        $mpPackageRoot `
        false
    if ($LASTEXITCODE -ne 0) {
        $mpExportText = $mpExportOutput -join "`n"
        throw "snc_status_snapshot mp package export failed. Actual output:`n$mpExportText"
    }

    $sncMpOutput = & $exePath `
        --snc-status-snapshot `
        $archiveRoot `
        $overlayRoot `
        $mpOutputPath `
        true `
        $mpPackageRoot `
        false `
        $stagingStatusPath
    $sncMpExitCode = $LASTEXITCODE
    $sncMpText = $sncMpOutput -join "`n"

    if ($sncMpExitCode -ne 0) {
        throw "snc_status_snapshot app with mp package failed. Actual output:`n$sncMpText"
    }

    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_state=ready"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_startup_lifecycle_state=owner_enabled_start_with_windows"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_enabled=true"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_source=config_override"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_shortcut_state=override_enabled"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_command_hint=cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_command_hint_source=startup_shortcut_remove_command"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_enable_command_hint=cmd /c tools\install_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_start_with_windows_disable_command_hint=cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_window_close_behavior=minimize_to_tray"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_explicit_exit_behavior=stop_without_restart"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_crash_restart_policy=bounded_backoff_with_crash_loop_guard"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_previous_host_available=false"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_previous_host_available_known=true"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_zip_state=ready"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_zip_reason=mp overlay package zip ready for handoff"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_zip_path=$mpPackageZipCliPath"
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_zip_hash="
    Assert-Contains -Name "snc_status_snapshot mp app" -Text $sncMpText -Expected "snc_mp_overlay_package_zip_bytes="
    if (-not (Test-Path -LiteralPath $mpPackageZipPath)) {
        throw "snc_status_snapshot current status source did not export expected mp zip: $mpPackageZipPath"
    }
    $snapshotWithMpJson = Get-Content -Raw -LiteralPath $mpOutputPath
    Assert-Contains -Name "snc_status_snapshot mp json" -Text $snapshotWithMpJson -Expected '"package_zip_state": "ready"'
    Assert-Contains -Name "snc_status_snapshot mp json" -Text $snapshotWithMpJson -Expected '"package_zip_hash": "'
    Assert-Contains -Name "snc_status_snapshot mp json" -Text $snapshotWithMpJson -Expected '"package_zip_bytes": '

    @'
{
  "schema_version": 2,
  "limit_reached": true,
  "skipped_due_to_limit_count": 3,
  "campaigns": []
}
'@ | Set-Content -LiteralPath $campaignLibraryPlanPath -Encoding ascii

    $invalidSncOutput = & $exePath `
        --snc-status-snapshot `
        $archiveRoot `
        $overlayRoot `
        $outputPath `
        true `
        false `
        false `
        $stagingStatusPath
    $invalidSncExitCode = $LASTEXITCODE
    $invalidSncText = $invalidSncOutput -join "`n"

    if ($invalidSncExitCode -ne 0) {
        throw "snc_status_snapshot app with invalid campaign-library sidecar failed unexpectedly. Actual output:`n$invalidSncText"
    }

    Assert-Contains -Name "snc_status_snapshot invalid app" -Text $invalidSncText -Expected "snc_campaign_library_plan_present=true"
    Assert-Contains -Name "snc_status_snapshot invalid app" -Text $invalidSncText -Expected "snc_campaign_library_plan_readiness=needs_attention"
    Assert-Contains -Name "snc_status_snapshot invalid app" -Text $invalidSncText -Expected "snc_campaign_library_plan_reason=campaign library plan schema unsupported"
    Assert-Contains -Name "snc_status_snapshot invalid app" -Text $invalidSncText -Expected "snc_campaign_library_limit_reached=false"
    Assert-Contains -Name "snc_status_snapshot invalid app" -Text $invalidSncText -Expected "snc_campaign_library_skipped_due_to_limit_count=0"

    Write-Host "[PASS] snc_status_snapshot"
}

function Invoke-AutosaveArchiveCase {
    $sourceRoot = Join-Path $repoRoot "dist/autosave_archive_cli_source"
    $archiveRoot = Join-Path $repoRoot "dist/autosave_archive_cli_archive"
    Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $archiveRoot -Recurse -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null

    $saveFixtureDir = Join-Path $sourceRoot "_save_fixture"
    $saveFixtureZip = Join-Path $sourceRoot "_save_fixture.zip"
    $saveFixtureSav = Join-Path $sourceRoot "autosave_2230.sav"
    New-Item -ItemType Directory -Force -Path $saveFixtureDir | Out-Null

    @'
version="v4.0.22"
revision="abc"
name="Synthetic Save"
'@ | Set-Content -LiteralPath (Join-Path $saveFixtureDir "meta") -Encoding ascii

    @'
date="2230.07.01"
player={
    {
        name="Tester"
        country=0
    }
}
country={
    0={
        name="Aeel Corp"
        founder_species=7
        capital=42
        starting_system=9
    }
}
species={
    7={
        name="Aeel"
    }
}
planet={
    42={
        name="Aeel Prime"
    }
}
galactic_object={
    9={
        name="Aeel System"
    }
}
war={
    3={
        name="Synthetic War"
    }
}
'@ | Set-Content -LiteralPath (Join-Path $saveFixtureDir "gamestate") -Encoding ascii

    Compress-Archive -Path (Join-Path $saveFixtureDir "*") -DestinationPath $saveFixtureZip -Force
    Move-Item -LiteralPath $saveFixtureZip -Destination $saveFixtureSav -Force
    Remove-Item -LiteralPath $saveFixtureDir -Recurse -Force

    Set-Content -LiteralPath (Join-Path $sourceRoot "ignored.txt") -Value "ignored" -Encoding UTF8

    $archiveOutput = & $exePath `
        --archive-stable-saves `
        $sourceRoot `
        $archiveRoot `
        "session_cli" `
        0
    $archiveExitCode = $LASTEXITCODE
    $archiveText = $archiveOutput -join "`n"

    if ($archiveExitCode -ne 0) {
        throw "autosave_archive app failed. Actual output:`n$archiveText"
    }

    Assert-Contains -Name "autosave_archive app" -Text $archiveText -Expected "autosave_archive_success=true"
    Assert-Contains -Name "autosave_archive app" -Text $archiveText -Expected "autosave_archive_copied=1"
    Assert-Contains -Name "autosave_archive app" -Text $archiveText -Expected "autosave_archive_skipped=0"

    $manifestPath = Join-Path $archiveRoot "session_cli/manifest.json"
    $manifestText = Get-Content -Raw -LiteralPath $manifestPath
    $null = $manifestText | ConvertFrom-Json
    Assert-Contains -Name "autosave_archive manifest" -Text $manifestText -Expected '"copied_count": 1'
    Assert-Contains -Name "autosave_archive manifest" -Text $manifestText -Expected '"reason": "stable_copy"'

    $liveSourceRoot = Join-Path $repoRoot "dist/autosave_live_cli_source"
    Remove-Item -Recurse -Force -LiteralPath $liveSourceRoot -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $liveSourceRoot "campaign_a") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $liveSourceRoot "campaign_b") | Out-Null
    Set-Content -LiteralPath (Join-Path $liveSourceRoot "campaign_a/autosave_2200.01.01.sav") -Value "live autosave one" -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $liveSourceRoot "campaign_b/ironman.sav") -Value "live ironman one" -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $liveSourceRoot "campaign_b/2200.01.01.sav") -Value "ignored date save" -Encoding ASCII

    $liveOutput = & $exePath `
        --archive-live-saves `
        $liveSourceRoot `
        $archiveRoot `
        "session_live_cli" `
        0
    $liveExitCode = $LASTEXITCODE
    $liveText = $liveOutput -join "`n"

    if ($liveExitCode -ne 0) {
        throw "live autosave archive app failed. Actual output:`n$liveText"
    }

    Assert-Contains -Name "live autosave archive app" -Text $liveText -Expected "live_autosave_archive_success=true"
    Assert-Contains -Name "live autosave archive app" -Text $liveText -Expected "live_autosave_archive_copied=2"

    Set-Content -LiteralPath (Join-Path $liveSourceRoot "campaign_a/autosave_2200.01.01.sav") -Value "live autosave two" -Encoding ASCII
    $changedLiveOutput = & $exePath `
        --archive-live-saves `
        $liveSourceRoot `
        $archiveRoot `
        "session_live_cli" `
        0
    $changedLiveExitCode = $LASTEXITCODE
    $changedLiveText = $changedLiveOutput -join "`n"

    if ($changedLiveExitCode -ne 0) {
        throw "changed live autosave archive app failed. Actual output:`n$changedLiveText"
    }

    Assert-Contains -Name "changed live autosave archive app" -Text $changedLiveText -Expected "live_autosave_archive_success=true"
    Assert-Contains -Name "changed live autosave archive app" -Text $changedLiveText -Expected "live_autosave_archive_copied=1"

    $sncLiveSourceRoot = Join-Path $repoRoot "dist/snc_live_autosave_cli_source"
    $sncLiveArchiveRoot = Join-Path $repoRoot "dist/snc_live_autosave_cli_archive"
    $sncLiveStatusPath = Join-Path $repoRoot "dist/private_reports/snc_live_autosave_monitor_status_test.json"
    Remove-Item -Recurse -Force -LiteralPath $sncLiveSourceRoot -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force -LiteralPath $sncLiveArchiveRoot -ErrorAction SilentlyContinue
    Remove-Item -Force -LiteralPath $sncLiveStatusPath -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $sncLiveSourceRoot "campaign_a") | Out-Null
    Set-Content -LiteralPath (Join-Path $sncLiveSourceRoot "campaign_a/autosave_2200.02.01.sav") -Value "snc live autosave one" -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $sncLiveSourceRoot "campaign_a/ironman.sav") -Value "snc live ironman one" -Encoding ASCII

    $sncLiveOutput = & $exePath `
        --snc-live-autosave-monitor `
        $sncLiveSourceRoot `
        $sncLiveArchiveRoot `
        "snc_live_cli" `
        0 `
        0 `
        1 `
        false `
        false `
        false `
        $sncLiveStatusPath
    $sncLiveExitCode = $LASTEXITCODE
    $sncLiveText = $sncLiveOutput -join "`n"

    if ($sncLiveExitCode -ne 0) {
        throw "SNC live autosave monitor app failed. Actual output:`n$sncLiveText"
    }

    Assert-Contains -Name "SNC live autosave monitor app" -Text $sncLiveText -Expected "snc_live_autosave_monitor_success=true"
    Assert-Contains -Name "SNC live autosave monitor app" -Text $sncLiveText -Expected "snc_live_autosave_monitor_iterations=1"
    Assert-Contains -Name "SNC live autosave monitor app" -Text $sncLiveText -Expected "snc_live_autosave_monitor_existing_roots=1"
    Assert-Contains -Name "SNC live autosave monitor app" -Text $sncLiveText -Expected "snc_live_autosave_monitor_copied=2"
    Assert-Contains -Name "SNC live autosave monitor app" -Text $sncLiveText -Expected "snc_live_autosave_monitor_status_written=true"

    $sncLiveManifestPath = Join-Path $sncLiveArchiveRoot "snc_live_cli/manifest.json"
    $sncLiveManifestText = Get-Content -Raw -LiteralPath $sncLiveManifestPath
    $null = $sncLiveManifestText | ConvertFrom-Json
    Assert-Contains -Name "SNC live autosave monitor manifest" -Text $sncLiveManifestText -Expected '"copied_count": 2'
    $sncLiveStatusText = Get-Content -Raw -LiteralPath $sncLiveStatusPath
    $null = $sncLiveStatusText | ConvertFrom-Json
    Assert-Contains -Name "SNC live autosave monitor status" -Text $sncLiveStatusText -Expected '"state": "completed"'
    Assert-Contains -Name "SNC live autosave monitor status" -Text $sncLiveStatusText -Expected '"copied_count": 2'

    $sncCaptureUserProfile = Join-Path $repoRoot "dist/snc_session_capture_user_profile"
    $sncCaptureSaveRoot = Join-Path $sncCaptureUserProfile "Documents/Paradox Interactive/Stellaris/save games"
    $sncCaptureArchiveRoot = Join-Path $repoRoot "dist/snc_session_capture_cli_archive"
    $sncCaptureStatusPath = Join-Path $repoRoot "dist/private_reports/snc_session_capture_status_test.json"
    Remove-Item -Recurse -Force -LiteralPath $sncCaptureUserProfile -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force -LiteralPath $sncCaptureArchiveRoot -ErrorAction SilentlyContinue
    Remove-Item -Force -LiteralPath $sncCaptureStatusPath -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $sncCaptureSaveRoot "campaign_auto") | Out-Null
    Set-Content -LiteralPath (Join-Path $sncCaptureSaveRoot "campaign_auto/autosave_2200.03.01.sav") -Value "snc capture autosave one" -Encoding ASCII

    $oldUserProfile = $env:USERPROFILE
    $oldOneDrive = $env:OneDrive
    $oldOneDriveConsumer = $env:OneDriveConsumer
    $oldOneDriveCommercial = $env:OneDriveCommercial
    $oldProgramFilesX86 = ${env:ProgramFiles(x86)}
    $oldProgramFiles = $env:ProgramFiles
    $oldProgramW6432 = $env:ProgramW6432
    $oldDisableSteamRegistryDiscovery = $env:STRATEGIC_NEXUS_DISABLE_STEAM_REGISTRY_DISCOVERY
    try {
        $env:USERPROFILE = $sncCaptureUserProfile
        $env:OneDrive = ""
        $env:OneDriveConsumer = ""
        $env:OneDriveCommercial = ""
        ${env:ProgramFiles(x86)} = Join-Path $sncCaptureUserProfile "Program Files (x86)"
        $env:ProgramFiles = Join-Path $sncCaptureUserProfile "Program Files"
        $env:ProgramW6432 = Join-Path $sncCaptureUserProfile "ProgramW6432"
        $env:STRATEGIC_NEXUS_DISABLE_STEAM_REGISTRY_DISCOVERY = "1"

        $sncCaptureOutput = & $exePath `
            --run-snc-session-capture `
            $sncCaptureArchiveRoot `
            $sncCaptureStatusPath `
            "snc_capture_cli" `
            0 `
            0 `
            1 `
            true
        $sncCaptureExitCode = $LASTEXITCODE
    } finally {
        $env:USERPROFILE = $oldUserProfile
        $env:OneDrive = $oldOneDrive
        $env:OneDriveConsumer = $oldOneDriveConsumer
        $env:OneDriveCommercial = $oldOneDriveCommercial
        ${env:ProgramFiles(x86)} = $oldProgramFilesX86
        $env:ProgramFiles = $oldProgramFiles
        $env:ProgramW6432 = $oldProgramW6432
        $env:STRATEGIC_NEXUS_DISABLE_STEAM_REGISTRY_DISCOVERY = $oldDisableSteamRegistryDiscovery
    }
    $sncCaptureText = $sncCaptureOutput -join "`n"

    if ($sncCaptureExitCode -ne 0) {
        throw "SNC session capture app failed. Actual output:`n$sncCaptureText"
    }

    Assert-Contains -Name "SNC session capture app" -Text $sncCaptureText -Expected "snc_session_capture_started=true"
    Assert-Contains -Name "SNC session capture app" -Text $sncCaptureText -Expected "snc_session_capture_mode=true"
    Assert-Contains -Name "SNC session capture app" -Text $sncCaptureText -Expected "snc_live_autosave_monitor_success=true"
    Assert-Contains -Name "SNC session capture app" -Text $sncCaptureText -Expected "snc_live_autosave_monitor_copied=1"
    $sncCaptureStatusText = Get-Content -Raw -LiteralPath $sncCaptureStatusPath
    $null = $sncCaptureStatusText | ConvertFrom-Json
    Assert-Contains -Name "SNC session capture status" -Text $sncCaptureStatusText -Expected '"state": "completed"'
    Assert-Contains -Name "SNC session capture status" -Text $sncCaptureStatusText -Expected '"copied_count": 1'

    $liveManifestPath = Join-Path $archiveRoot "session_live_cli/manifest.json"
    $liveManifestText = Get-Content -Raw -LiteralPath $liveManifestPath
    $null = $liveManifestText | ConvertFrom-Json
    Assert-Contains -Name "live autosave archive manifest" -Text $liveManifestText -Expected '"copied_count": 3'
    Assert-Contains -Name "live autosave archive manifest" -Text $liveManifestText -Expected '"reason": "live_archive_copy"'

    $liveVerifyOutput = & $exePath `
        --verify-autosave-archive `
        (Join-Path $archiveRoot "session_live_cli")
    $liveVerifyExitCode = $LASTEXITCODE
    $liveVerifyText = $liveVerifyOutput -join "`n"

    if ($liveVerifyExitCode -ne 0) {
        throw "live autosave archive verify app failed. Actual output:`n$liveVerifyText"
    }

    Assert-Contains -Name "live autosave archive verify app" -Text $liveVerifyText -Expected "autosave_archive_manifest_ok=true"

    $verifyOutput = & $exePath `
        --verify-autosave-archive `
        (Join-Path $archiveRoot "session_cli")
    $verifyExitCode = $LASTEXITCODE
    $verifyText = $verifyOutput -join "`n"

    if ($verifyExitCode -ne 0) {
        throw "autosave_archive verify app failed. Actual output:`n$verifyText"
    }

    Assert-Contains -Name "autosave_archive verify app" -Text $verifyText -Expected "autosave_archive_manifest_ok=true"
    Assert-Contains -Name "autosave_archive verify app" -Text $verifyText -Expected "autosave_archive_manifest_reason=accepted"

    $summaryPath = Join-Path $archiveRoot "session_cli_summary.json"
    $summaryOutput = & $exePath `
        --summarize-autosave-archive `
        (Join-Path $archiveRoot "session_cli") `
        $summaryPath
    $summaryExitCode = $LASTEXITCODE
    $summaryText = $summaryOutput -join "`n"

    if ($summaryExitCode -ne 0) {
        throw "autosave_archive summary app failed. Actual output:`n$summaryText"
    }

    Assert-Contains -Name "autosave_archive summary app" -Text $summaryText -Expected "autosave_archive_summary_success=true"
    Assert-Contains -Name "autosave_archive summary app" -Text $summaryText -Expected "autosave_archive_summary_save_count=1"

    $summaryJson = Get-Content -Raw -LiteralPath $summaryPath
    $null = $summaryJson | ConvertFrom-Json
    Assert-Contains -Name "autosave_archive summary json" -Text $summaryJson -Expected '"copied_save_count": 1'

    $ministryInputPath = Join-Path $archiveRoot "session_cli_ministry_input.json"
    $inputOutput = & $exePath `
        --build-ministry-input-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "empire_cli" `
        "military" `
        $ministryInputPath
    $inputExitCode = $LASTEXITCODE
    $inputText = $inputOutput -join "`n"

    if ($inputExitCode -ne 0) {
        throw "archive ministry input app failed. Actual output:`n$inputText"
    }

    Assert-Contains -Name "archive ministry input app" -Text $inputText -Expected "archive_ministry_input_success=true"
    Assert-Contains -Name "archive ministry input app" -Text $inputText -Expected "archive_ministry_input_campaign_id=campaign_cli"

    $inputJson = Get-Content -Raw -LiteralPath $ministryInputPath
    $null = $inputJson | ConvertFrom-Json
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"campaign_id": "campaign_cli"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"year": 2230'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"is_at_war": true'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"strategic_pressure": 0.6'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_month_hint:7"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_day_hint:1"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_war_hint_confidence_percent:70"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_year_hint_confidence_percent:80"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_month_hint_confidence_percent:80"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_day_hint_confidence_percent:80"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_pressure_year_hint_source:save_date"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_pressure_year_hint_confidence_percent:60"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_pressure_war_hint_source:headline_active_war_count"'
    Assert-Contains -Name "archive ministry input json" -Text $inputJson -Expected '"turn_context_pressure_war_hint_confidence_percent:70"'

    $invalidInputOutput = & $exePath `
        --build-ministry-input-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "  " `
        "empire_cli" `
        "military" `
        (Join-Path $archiveRoot "session_cli_invalid_ministry_input.json")
    $invalidInputExitCode = $LASTEXITCODE
    $invalidInputText = $invalidInputOutput -join "`n"
    if ($invalidInputExitCode -eq 0) {
        throw "archive ministry input app expected failure for whitespace campaign id. Actual output:`n$invalidInputText"
    }
    Assert-Contains -Name "archive ministry input app (whitespace campaign id)" -Text $invalidInputText -Expected "archive_ministry_input_success=false"

    $archivePipelineInputPath = Join-Path $archiveRoot "session_cli_archive_pipeline_input.json"
    $archivePipelineDecisionPath = Join-Path $archiveRoot "session_cli_archive_pipeline_decision.json"
    $archivePipelineAuditPath = Join-Path $archiveRoot "session_cli_archive_pipeline_audit.json"
    $archivePipelineOutput = & $exePath `
        --v0-pipeline-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "empire_cli" `
        "military" `
        $archivePipelineInputPath `
        $archivePipelineDecisionPath `
        301 `
        123456 `
        30000 `
        $archivePipelineAuditPath
    $archivePipelineExitCode = $LASTEXITCODE
    $archivePipelineText = $archivePipelineOutput -join "`n"

    if ($archivePipelineExitCode -ne 0) {
        throw "archive v0 pipeline app failed. Actual output:`n$archivePipelineText"
    }

    Assert-Contains -Name "archive v0 pipeline app" -Text $archivePipelineText -Expected "archive_v0_pipeline_success=true"
    Assert-Contains -Name "archive v0 pipeline app" -Text $archivePipelineText -Expected "archive_v0_pipeline_ministry_input_written=true"
    Assert-Contains -Name "archive v0 pipeline app" -Text $archivePipelineText -Expected "archive_v0_pipeline_decision_written=true"
    Assert-Contains -Name "archive v0 pipeline app" -Text $archivePipelineText -Expected "archive_v0_pipeline_campaign_id=campaign_cli"
    Assert-Contains -Name "archive v0 pipeline app" -Text $archivePipelineText -Expected "archive_v0_pipeline_empire_id=empire_cli"

    $badArchivePipelineOutput = & $exePath `
        --v0-pipeline-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "   " `
        "military" `
        (Join-Path $archiveRoot "session_cli_archive_pipeline_input_bad.json") `
        (Join-Path $archiveRoot "session_cli_archive_pipeline_decision_bad.json") `
        301 `
        123456 `
        30000 `
        (Join-Path $archiveRoot "session_cli_archive_pipeline_audit_bad.json")
    $badArchivePipelineExitCode = $LASTEXITCODE
    $badArchivePipelineText = $badArchivePipelineOutput -join "`n"
    if ($badArchivePipelineExitCode -eq 0) {
        throw "archive v0 pipeline app expected failure for whitespace empire id. Actual output:`n$badArchivePipelineText"
    }
    Assert-Contains -Name "archive v0 pipeline app (whitespace empire id)" -Text $badArchivePipelineText -Expected "archive_v0_pipeline_success=false"

    $archivePipelineDecisionJson = Get-Content -Raw -LiteralPath $archivePipelineDecisionPath
    $null = $archivePipelineDecisionJson | ConvertFrom-Json
    Assert-Contains -Name "archive v0 pipeline decision json" -Text $archivePipelineDecisionJson -Expected '"campaign_id": "campaign_cli"'
    Assert-Contains -Name "archive v0 pipeline decision json" -Text $archivePipelineDecisionJson -Expected '"empire_id": "empire_cli"'
    Assert-Contains -Name "archive v0 pipeline decision json" -Text $archivePipelineDecisionJson -Expected '"military_posture": "defensive"'

    $archivePipelineAuditJson = Get-Content -Raw -LiteralPath $archivePipelineAuditPath
    $null = $archivePipelineAuditJson | ConvertFrom-Json
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"archive_session_summary_v0"'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"strategic_pressure": 0.6'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"turn_context_year_hint_source:save_date"'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"turn_context_month_hint:7"'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"turn_context_day_hint:1"'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"turn_context_month_hint_confidence_percent:80"'
    Assert-Contains -Name "archive v0 pipeline audit json" -Text $archivePipelineAuditJson -Expected '"turn_context_day_hint_confidence_percent:80"'

    $invalidHintsSourceRoot = Join-Path $repoRoot "dist/autosave_archive_cli_source_invalid_hints"
    Remove-Item -LiteralPath $invalidHintsSourceRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $invalidHintsSourceRoot | Out-Null

    $invalidHintsSaveFixtureDir = Join-Path $invalidHintsSourceRoot "_save_fixture"
    $invalidHintsSaveFixtureZip = Join-Path $invalidHintsSourceRoot "_save_fixture.zip"
    $invalidHintsSaveFixtureSav = Join-Path $invalidHintsSourceRoot "autosave_2230_invalid_hints.sav"
    New-Item -ItemType Directory -Force -Path $invalidHintsSaveFixtureDir | Out-Null

    @'
version="v4.0.22"
revision="abc"
name="Invalid Hint Save"
'@ | Set-Content -LiteralPath (Join-Path $invalidHintsSaveFixtureDir "meta") -Encoding ascii

    @'
date="22ab.07.01"
player={
    {
        name="Tester"
        country=0
    }
}
country={
    0={
        name="Aeel Corp"
        founder_species=7
        capital=42
        starting_system=9
    }
}
species={
    7={
        name="Aeel"
    }
}
planet={
    42={
        name="Aeel Prime"
    }
}
galactic_object={
    9={
        name="Aeel System"
    }
}
'@ | Set-Content -LiteralPath (Join-Path $invalidHintsSaveFixtureDir "gamestate") -Encoding ascii

    Compress-Archive -Path (Join-Path $invalidHintsSaveFixtureDir "*") -DestinationPath $invalidHintsSaveFixtureZip -Force
    Move-Item -LiteralPath $invalidHintsSaveFixtureZip -Destination $invalidHintsSaveFixtureSav -Force
    Remove-Item -LiteralPath $invalidHintsSaveFixtureDir -Recurse -Force

    $invalidHintsArchiveOutput = & $exePath `
        --archive-stable-saves `
        $invalidHintsSourceRoot `
        $archiveRoot `
        "session_cli_invalid_hints" `
        0
    $invalidHintsArchiveExitCode = $LASTEXITCODE
    $invalidHintsArchiveText = $invalidHintsArchiveOutput -join "`n"

    if ($invalidHintsArchiveExitCode -ne 0) {
        throw "autosave archive invalid-hints setup failed. Actual output:`n$invalidHintsArchiveText"
    }

    Assert-Contains -Name "autosave archive invalid-hints setup" -Text $invalidHintsArchiveText -Expected "autosave_archive_success=true"

    $invalidHintMinistryInputPath = Join-Path $archiveRoot "session_cli_invalid_hints_ministry_input.json"
    $invalidHintInputOutput = & $exePath `
        --build-ministry-input-from-archive `
        (Join-Path $archiveRoot "session_cli_invalid_hints") `
        "campaign_cli_invalid_hints" `
        "empire_cli" `
        "military" `
        $invalidHintMinistryInputPath
    $invalidHintInputExitCode = $LASTEXITCODE
    $invalidHintInputText = $invalidHintInputOutput -join "`n"

    if ($invalidHintInputExitCode -ne 0) {
        throw "archive ministry input app (invalid hints) failed. Actual output:`n$invalidHintInputText"
    }

    Assert-Contains -Name "archive ministry input app (invalid hints)" -Text $invalidHintInputText -Expected "archive_ministry_input_success=true"
    $invalidHintInputJson = Get-Content -Raw -LiteralPath $invalidHintMinistryInputPath
    $null = $invalidHintInputJson | ConvertFrom-Json
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"year": 0'
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"is_at_war": false'
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"strategic_pressure": 0.25'
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"save_headline_date_invalid"'
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"turn_context_war_hint_source:headline_active_war_count"'
    Assert-Contains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Expected '"turn_context_pressure_war_hint_source:headline_active_war_count"'
    Assert-NotContains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Unexpected '"turn_context_year_hint_source:save_date"'
    Assert-NotContains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Unexpected '"turn_context_month_hint_source:save_date"'
    Assert-NotContains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Unexpected '"turn_context_day_hint_source:save_date"'
    Assert-NotContains -Name "archive ministry input json (invalid hints)" -Text $invalidHintInputJson -Unexpected '"turn_context_pressure_year_hint_source:save_date"'

    $invalidHintArchivePipelineInputPath = Join-Path $archiveRoot "session_cli_invalid_hints_archive_pipeline_input.json"
    $invalidHintArchivePipelineDecisionPath = Join-Path $archiveRoot "session_cli_invalid_hints_archive_pipeline_decision.json"
    $invalidHintArchivePipelineAuditPath = Join-Path $archiveRoot "session_cli_invalid_hints_archive_pipeline_audit.json"
    $invalidHintArchivePipelineOutput = & $exePath `
        --v0-pipeline-from-archive `
        (Join-Path $archiveRoot "session_cli_invalid_hints") `
        "campaign_cli_invalid_hints" `
        "empire_cli" `
        "military" `
        $invalidHintArchivePipelineInputPath `
        $invalidHintArchivePipelineDecisionPath `
        302 `
        123456 `
        30000 `
        $invalidHintArchivePipelineAuditPath
    $invalidHintArchivePipelineExitCode = $LASTEXITCODE
    $invalidHintArchivePipelineText = $invalidHintArchivePipelineOutput -join "`n"

    if ($invalidHintArchivePipelineExitCode -ne 0) {
        throw "archive v0 pipeline app (invalid hints) failed. Actual output:`n$invalidHintArchivePipelineText"
    }

    Assert-Contains -Name "archive v0 pipeline app (invalid hints)" -Text $invalidHintArchivePipelineText -Expected "archive_v0_pipeline_success=true"
    $invalidHintArchivePipelineDecisionJson = Get-Content -Raw -LiteralPath $invalidHintArchivePipelineDecisionPath
    $null = $invalidHintArchivePipelineDecisionJson | ConvertFrom-Json
    Assert-Contains -Name "archive v0 pipeline decision json (invalid hints)" -Text $invalidHintArchivePipelineDecisionJson -Expected '"military_posture": "defensive"'

    $invalidHintArchivePipelineAuditJson = Get-Content -Raw -LiteralPath $invalidHintArchivePipelineAuditPath
    $null = $invalidHintArchivePipelineAuditJson | ConvertFrom-Json
    Assert-Contains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Expected '"strategic_pressure": 0.25'
    Assert-Contains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Expected '"save_headline_date_invalid"'
    Assert-Contains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Expected '"turn_context_war_hint_source:headline_active_war_count"'
    Assert-Contains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Expected '"turn_context_pressure_war_hint_source:headline_active_war_count"'
    Assert-NotContains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Unexpected '"turn_context_year_hint_source:save_date"'
    Assert-NotContains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Unexpected '"turn_context_month_hint_source:save_date"'
    Assert-NotContains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Unexpected '"turn_context_day_hint_source:save_date"'
    Assert-NotContains -Name "archive v0 pipeline audit json (invalid hints)" -Text $invalidHintArchivePipelineAuditJson -Unexpected '"turn_context_pressure_year_hint_source:save_date"'

    $ledgerPath = Join-Path $archiveRoot "session_cli_delta_ledger.json"
    $ledgerOutput = & $exePath `
        --build-season-delta-ledger `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        $ledgerPath
    $ledgerExitCode = $LASTEXITCODE
    $ledgerText = $ledgerOutput -join "`n"

    if ($ledgerExitCode -ne 0) {
        throw "season delta ledger app failed. Actual output:`n$ledgerText"
    }

    Assert-Contains -Name "season delta ledger app" -Text $ledgerText -Expected "season_delta_ledger_success=true"
    Assert-Contains -Name "season delta ledger app" -Text $ledgerText -Expected "season_delta_ledger_campaign_id=campaign_cli"
    Assert-Contains -Name "season delta ledger app" -Text $ledgerText -Expected "season_delta_ledger_save_count=1"

    $badWhitespaceLedgerOutput = & $exePath `
        --build-season-delta-ledger `
        (Join-Path $archiveRoot "session_cli") `
        "   " `
        (Join-Path $archiveRoot "session_cli_delta_ledger_whitespace.json")
    $badWhitespaceLedgerExitCode = $LASTEXITCODE
    $badWhitespaceLedgerText = $badWhitespaceLedgerOutput -join "`n"
    if ($badWhitespaceLedgerExitCode -eq 0) {
        throw "season delta ledger app expected failure for whitespace campaign id. Actual output:`n$badWhitespaceLedgerText"
    }
    Assert-Contains -Name "season delta ledger app (whitespace campaign id)" -Text $badWhitespaceLedgerText -Expected "season_delta_ledger_success=false"

    $ledgerJson = Get-Content -Raw -LiteralPath $ledgerPath
    $null = $ledgerJson | ConvertFrom-Json
    Assert-Contains -Name "season delta ledger json" -Text $ledgerJson -Expected '"campaign_id": "campaign_cli"'
    Assert-Contains -Name "season delta ledger json" -Text $ledgerJson -Expected '"delta_quality": "metadata_plus_save_headline"'
    Assert-Contains -Name "season delta ledger json" -Text $ledgerJson -Expected '"archive_verified:true"'

    $badArchiveDir = Join-Path $archiveRoot "session_cli_bad_archive"
    $null = New-Item -ItemType Directory -Force -Path $badArchiveDir
    $badLedgerPath = Join-Path $archiveRoot "session_cli_bad_delta_ledger.json"

    $badLedgerOutput = & $exePath `
        --build-season-delta-ledger `
        $badArchiveDir `
        "campaign_cli" `
        $badLedgerPath
    $badLedgerExitCode = $LASTEXITCODE

    if ($badLedgerExitCode -eq 0) {
        $badLedgerText = $badLedgerOutput -join "`n"
        throw "season delta ledger app expected failure for bad archive. Actual output:`n$badLedgerText"
    }

    if (-not (Test-Path -LiteralPath $badLedgerPath)) {
        throw "season delta ledger app should still write JSON output for bad archive: $badLedgerPath"
    }

    $badLedgerJson = Get-Content -Raw -LiteralPath $badLedgerPath
    $null = $badLedgerJson | ConvertFrom-Json
    Assert-Contains -Name "season delta ledger json (bad archive)" -Text $badLedgerJson -Expected '"ok": false'
    Assert-Contains -Name "season delta ledger json (bad archive)" -Text $badLedgerJson -Expected "missing autosave archive manifest"

    $briefPath = Join-Path $archiveRoot "session_cli_empire_brief.json"
    $briefOutput = & $exePath `
        --build-empire-brief-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "empire_cli" `
        $briefPath
    $briefExitCode = $LASTEXITCODE
    $briefText = $briefOutput -join "`n"

    if ($briefExitCode -ne 0) {
        throw "archive empire brief app failed. Actual output:`n$briefText"
    }

    Assert-Contains -Name "archive empire brief app" -Text $briefText -Expected "archive_empire_brief_success=true"
    Assert-Contains -Name "archive empire brief app" -Text $briefText -Expected "archive_empire_brief_campaign_id=campaign_cli"
    Assert-Contains -Name "archive empire brief app" -Text $briefText -Expected "archive_empire_brief_empire_id=empire_cli"

    $badWhitespaceBriefOutput = & $exePath `
        --build-empire-brief-from-archive `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "   " `
        (Join-Path $archiveRoot "session_cli_empire_brief_whitespace.json")
    $badWhitespaceBriefExitCode = $LASTEXITCODE
    $badWhitespaceBriefText = $badWhitespaceBriefOutput -join "`n"
    if ($badWhitespaceBriefExitCode -eq 0) {
        throw "archive empire brief app expected failure for whitespace empire id. Actual output:`n$badWhitespaceBriefText"
    }
    Assert-Contains -Name "archive empire brief app (whitespace empire id)" -Text $badWhitespaceBriefText -Expected "archive_empire_brief_success=false"

    $briefJson = Get-Content -Raw -LiteralPath $briefPath
    $null = $briefJson | ConvertFrom-Json
    Assert-Contains -Name "archive empire brief json" -Text $briefJson -Expected '"empire_id": "empire_cli"'
    Assert-Contains -Name "archive empire brief json" -Text $briefJson -Expected '"source_ledger_quality": "metadata_plus_save_headline"'
    Assert-Contains -Name "archive empire brief json" -Text $briefJson -Expected '"empire_identity_resolver_not_implemented_yet"'

    $badBriefPath = Join-Path $archiveRoot "session_cli_bad_empire_brief.json"
    $badBriefOutput = & $exePath `
        --build-empire-brief-from-archive `
        $badArchiveDir `
        "campaign_cli" `
        "empire_cli" `
        $badBriefPath
    $badBriefExitCode = $LASTEXITCODE

    if ($badBriefExitCode -eq 0) {
        $badBriefText = $badBriefOutput -join "`n"
        throw "archive empire brief app expected failure for bad archive. Actual output:`n$badBriefText"
    }

    if (-not (Test-Path -LiteralPath $badBriefPath)) {
        throw "archive empire brief app should still write JSON output for bad archive: $badBriefPath"
    }

    $badBriefJson = Get-Content -Raw -LiteralPath $badBriefPath
    $null = $badBriefJson | ConvertFrom-Json
    Assert-Contains -Name "archive empire brief json (bad archive)" -Text $badBriefJson -Expected '"ok": false'
    Assert-Contains -Name "archive empire brief json (bad archive)" -Text $badBriefJson -Expected "missing autosave archive manifest"

    $offlineSpineWorkDir = Join-Path $archiveRoot "session_cli_offline_spine_work"
    $offlineSpineOverlayDir = Join-Path $archiveRoot "session_cli_offline_spine_overlay"
    $offlineSpineStatusPath = Join-Path $archiveRoot "session_cli_offline_spine_status.json"
    Remove-Item -LiteralPath $offlineSpineWorkDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $offlineSpineOverlayDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $offlineSpineStatusPath -Force -ErrorAction SilentlyContinue

    $offlineSpineOutput = & $exePath `
        --run-offline-spine `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "empire_cli" `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $offlineSpineWorkDir `
        $offlineSpineOverlayDir `
        $offlineSpineStatusPath
    $offlineSpineExitCode = $LASTEXITCODE
    $offlineSpineText = $offlineSpineOutput -join "`n"

    if ($offlineSpineExitCode -ne 0) {
        throw "offline spine app failed. Actual output:`n$offlineSpineText"
    }

    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_success=true"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_archive_verified=true"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_save_count=1"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_ledger_written=true"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_brief_written=true"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_overlay_verified=true"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_status_center_state=ready"
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_work_directory="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_ledger_output_path="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_brief_output_path="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_overlay_output_path="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_status_output_path="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_status_generated_at_local="
    Assert-Contains -Name "offline spine app" -Text $offlineSpineText -Expected "offline_spine_status_output_written=true"

    $offlineLedgerJson = Get-Content -Raw -LiteralPath (Join-Path $offlineSpineWorkDir "season_delta_ledger.json")
    $null = $offlineLedgerJson | ConvertFrom-Json
    Assert-Contains -Name "offline spine ledger json" -Text $offlineLedgerJson -Expected '"campaign_id": "campaign_cli"'
    Assert-Contains -Name "offline spine ledger json" -Text $offlineLedgerJson -Expected '"archive_verified:true"'

    $offlineBriefJson = Get-Content -Raw -LiteralPath (Join-Path $offlineSpineWorkDir "empire_brief.json")
    $null = $offlineBriefJson | ConvertFrom-Json
    Assert-Contains -Name "offline spine brief json" -Text $offlineBriefJson -Expected '"empire_id": "empire_cli"'
    Assert-Contains -Name "offline spine brief json" -Text $offlineBriefJson -Expected '"source_ledger_quality": "metadata_plus_save_headline"'

    $offlineManifestJson = Get-Content -Raw -LiteralPath (Join-Path $offlineSpineOverlayDir "strategic_nexus_generated_manifest.json")
    $null = $offlineManifestJson | ConvertFrom-Json
    Assert-Contains -Name "offline spine manifest json" -Text $offlineManifestJson -Expected '"snapshot_kind": "complete_replacement"'
    Assert-Contains -Name "offline spine manifest json" -Text $offlineManifestJson -Expected '"multiplayer_requirement": "byte_identical_gameplay_affecting_files"'

    $offlineStatusJson = Get-Content -Raw -LiteralPath $offlineSpineStatusPath
    $null = $offlineStatusJson | ConvertFrom-Json
    Assert-Contains -Name "offline spine status json" -Text $offlineStatusJson -Expected '"status_center":'
    Assert-Contains -Name "offline spine status json" -Text $offlineStatusJson -Expected '"state": "ready"'

    $offlineSpineWorkDirPrecreated = Join-Path $archiveRoot "session_cli_offline_spine_work_precreated"
    $offlineSpineOverlayDirPrecreated = Join-Path $archiveRoot "session_cli_offline_spine_overlay_precreated"
    $offlineSpineStatusPathPrecreated = Join-Path $archiveRoot "session_cli_offline_spine_status_precreated.json"
    Remove-Item -LiteralPath $offlineSpineWorkDirPrecreated -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $offlineSpineOverlayDirPrecreated -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $offlineSpineStatusPathPrecreated -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path (Join-Path $offlineSpineOverlayDirPrecreated "events") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $offlineSpineOverlayDirPrecreated "common/scripted_effects") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $offlineSpineOverlayDirPrecreated "common/scripted_triggers") | Out-Null

    $offlineSpineOutputPrecreated = & $exePath `
        --run-offline-spine `
        (Join-Path $archiveRoot "session_cli") `
        "campaign_cli" `
        "empire_cli" `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $offlineSpineWorkDirPrecreated `
        $offlineSpineOverlayDirPrecreated `
        $offlineSpineStatusPathPrecreated
    $offlineSpineExitCodePrecreated = $LASTEXITCODE
    $offlineSpineTextPrecreated = $offlineSpineOutputPrecreated -join "`n"

    if ($offlineSpineExitCodePrecreated -ne 0) {
        throw "offline spine app failed for precreated overlay directory. Actual output:`n$offlineSpineTextPrecreated"
    }

    Assert-Contains -Name "offline spine app (precreated overlay directory)" -Text $offlineSpineTextPrecreated -Expected "offline_spine_success=true"

    Write-Host "[PASS] autosave_archive"
}

function Invoke-AutosaveArchiveVerifyMismatchCase {
    $sourceRoot = Join-Path $repoRoot "dist/autosave_archive_mismatch_source"
    $archiveRoot = Join-Path $repoRoot "dist/autosave_archive_mismatch_archive"
    Remove-Item -LiteralPath $sourceRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $archiveRoot -Recurse -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $sourceRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceRoot "autosave_2230.sav") -Value "fixture" -Encoding UTF8

    $null = & $exePath `
        --archive-stable-saves `
        $sourceRoot `
        $archiveRoot `
        "session_mismatch" `
        0

    Set-Content -LiteralPath (Join-Path $archiveRoot "session_mismatch/saves/001_autosave_2230.sav") -Value "tampered" -Encoding UTF8

    $verifyOutput = & $exePath `
        --verify-autosave-archive `
        (Join-Path $archiveRoot "session_mismatch")
    $verifyExitCode = $LASTEXITCODE
    $verifyText = $verifyOutput -join "`n"

    if ($verifyExitCode -ne 1) {
        throw "autosave_archive mismatch verify should fail. Actual output:`n$verifyText"
    }

    Assert-Contains -Name "autosave_archive mismatch app" -Text $verifyText -Expected "autosave_archive_manifest_ok=false"
    Assert-Contains -Name "autosave_archive mismatch app" -Text $verifyText -Expected "autosave_archive_manifest_reason=autosave archive files do not match manifest"

    Write-Host "[PASS] autosave_archive_verify_mismatch"
}

function Invoke-RealSessionTrendIdentityRiskPriorityCase {
    $trendFixtureRoot = Join-Path $repoRoot "dist/test_trend_identity_risk_priority"
    $sessionA = Join-Path $trendFixtureRoot "session_a"
    $sessionB = Join-Path $trendFixtureRoot "session_b"
    Remove-Item -LiteralPath $trendFixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $sessionA | Out-Null
    New-Item -ItemType Directory -Force -Path $sessionB | Out-Null

    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/prev/snc_status_snapshot.json") -Destination (Join-Path $sessionA "snc_status_snapshot.json")
    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/prev/snc_status_snapshot_with_mp.json") -Destination (Join-Path $sessionA "snc_status_snapshot_with_mp.json")
    New-Item -ItemType Directory -Force -Path (Join-Path $sessionA "work") | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/prev/work/archive_summary.json") -Destination (Join-Path $sessionA "work/archive_summary.json")

    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/curr/snc_status_snapshot.json") -Destination (Join-Path $sessionB "snc_status_snapshot.json")
    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/curr/snc_status_snapshot_with_mp.json") -Destination (Join-Path $sessionB "snc_status_snapshot_with_mp.json")
    New-Item -ItemType Directory -Force -Path (Join-Path $sessionB "work") | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "dist/test_compare_identity_risk/curr/work/archive_summary.json") -Destination (Join-Path $sessionB "work/archive_summary.json")

    # Force an additional observable delta so recommendation priority is exercised.
    $summaryB = Get-Content -Raw -LiteralPath (Join-Path $sessionB "work/archive_summary.json") | ConvertFrom-Json
    $summaryB.copied_save_count = [int]$summaryB.copied_save_count + 1
    $summaryB | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $sessionB "work/archive_summary.json") -Encoding UTF8

    $trendOutputPath = Join-Path $trendFixtureRoot "trend_output.json"
    $trendOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "analyze_real_session_v0_trend.ps1") $trendFixtureRoot $trendOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "real session trend identity-risk priority case failed running trend analyzer (exit code $LASTEXITCODE)."
    }
    $trendText = $trendOutput -join "`n"
    Assert-Contains -Name "real session trend identity-risk priority" -Text $trendText -Expected "real_session_v0_trend_recommendation=review_identity_risk_warning"
    Assert-Contains -Name "real session trend identity-risk priority" -Text $trendText -Expected "real_session_v0_trend_identity_risk_warning=true"
    Assert-Contains -Name "real session trend identity-risk priority" -Text $trendText -Expected "real_session_v0_trend_observable_effect_signal=true"
    Write-Host "[PASS] real_session_trend_identity_risk_priority"
}

function Invoke-RealSessionTrendHandoffContinuityPriorityCase {
    $sourceSessionDir = Join-Path $repoRoot "dist/real_session_v0_loop/session_manual_smoke_mp5"
    $trendFixtureRoot = Join-Path $repoRoot "dist/test_trend_mp_handoff_priority"
    $sessionA = Join-Path $trendFixtureRoot "session_a"
    $sessionB = Join-Path $trendFixtureRoot "session_b"

    Remove-Item -LiteralPath $trendFixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item -LiteralPath $sourceSessionDir -Destination $sessionA -Recurse
    Copy-Item -LiteralPath $sourceSessionDir -Destination $sessionB -Recurse

    $sessionBStatusWithMpPath = Join-Path $sessionB "snc_status_snapshot_with_mp.json"
    $sessionBStatusWithMp = Get-Content -Raw -LiteralPath $sessionBStatusWithMpPath | ConvertFrom-Json
    $sessionBStatusWithMp.mp_overlay_package_status.handoff_status = "degraded_previous_host_unavailable"
    $sessionBStatusWithMp | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $sessionBStatusWithMpPath -Encoding UTF8

    # Force an additional observable delta so recommendation priority is exercised.
    $summaryBPath = Join-Path $sessionB "work/archive_summary.json"
    $summaryB = Get-Content -Raw -LiteralPath $summaryBPath | ConvertFrom-Json
    $summaryB.copied_save_count = [int]$summaryB.copied_save_count + 1
    $summaryB | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryBPath -Encoding UTF8

    $compareOutputPath = Join-Path $trendFixtureRoot "compare_output.json"
    $compareOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "compare_real_session_v0_outputs.ps1") $sessionA $sessionB $compareOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "real session trend handoff continuity case failed running compare (exit code $LASTEXITCODE)."
    }
    $compareText = $compareOutput -join "`n"
    Assert-Contains -Name "real session trend handoff continuity compare" -Text $compareText -Expected "real_session_v0_compare_recommendation=review_mp_handoff_continuity"
    Assert-Contains -Name "real session trend handoff continuity compare" -Text $compareText -Expected "real_session_v0_compare_mp_handoff_follow_up_active=true"
    Assert-Contains -Name "real session trend handoff continuity compare" -Text $compareText -Expected "real_session_v0_compare_mp_handoff_follow_up_reason=current_previous_host_unavailable"

    $trendOutputPath = Join-Path $trendFixtureRoot "trend_output.json"
    $trendOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "analyze_real_session_v0_trend.ps1") $trendFixtureRoot $trendOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "real session trend handoff continuity case failed running trend analyzer (exit code $LASTEXITCODE)."
    }
    $trendText = $trendOutput -join "`n"
    Assert-Contains -Name "real session trend handoff continuity" -Text $trendText -Expected "real_session_v0_trend_recommendation=review_mp_handoff_continuity"
    Assert-Contains -Name "real session trend handoff continuity" -Text $trendText -Expected "real_session_v0_trend_mp_handoff_follow_up_active=true"
    Assert-Contains -Name "real session trend handoff continuity" -Text $trendText -Expected "real_session_v0_trend_mp_handoff_follow_up_reason=current_previous_host_unavailable"
    Assert-Contains -Name "real session trend handoff continuity" -Text $trendText -Expected "real_session_v0_trend_observable_effect_signal=true"
    Write-Host "[PASS] real_session_trend_handoff_continuity_priority"
}

function Invoke-RealSessionWarningCodeDriftSurfaceCase {
    $fixtureRoot = Join-Path $repoRoot "dist/test_compare_identity_risk"
    $prevDir = Join-Path $fixtureRoot "prev"
    $currDir = Join-Path $fixtureRoot "curr"
    $compareOutputPath = Join-Path $fixtureRoot "compare_warning_codes_surface.json"
    $trendRoot = Join-Path $repoRoot "dist/test_trend_warning_code_surface"
    $trendOutputPath = Join-Path $trendRoot "trend_warning_codes_surface.json"

    Remove-Item -LiteralPath $trendRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $trendRoot "session_a/work") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $trendRoot "session_b/work") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $trendRoot "session_a/mp_overlay_package") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $trendRoot "session_b/mp_overlay_package") | Out-Null

    Copy-Item -LiteralPath (Join-Path $prevDir "snc_status_snapshot.json") -Destination (Join-Path $trendRoot "session_a/snc_status_snapshot.json")
    Copy-Item -LiteralPath (Join-Path $prevDir "snc_status_snapshot_with_mp.json") -Destination (Join-Path $trendRoot "session_a/snc_status_snapshot_with_mp.json")
    Copy-Item -LiteralPath (Join-Path $prevDir "work/archive_summary.json") -Destination (Join-Path $trendRoot "session_a/work/archive_summary.json")

    Copy-Item -LiteralPath (Join-Path $currDir "snc_status_snapshot.json") -Destination (Join-Path $trendRoot "session_b/snc_status_snapshot.json")
    Copy-Item -LiteralPath (Join-Path $currDir "snc_status_snapshot_with_mp.json") -Destination (Join-Path $trendRoot "session_b/snc_status_snapshot_with_mp.json")
    Copy-Item -LiteralPath (Join-Path $currDir "work/archive_summary.json") -Destination (Join-Path $trendRoot "session_b/work/archive_summary.json")

    $sessionAStatusWithMpPath = Join-Path $trendRoot "session_a/snc_status_snapshot_with_mp.json"
    $sessionBStatusWithMpPath = Join-Path $trendRoot "session_b/snc_status_snapshot_with_mp.json"
    $sessionAStatusWithMp = Get-Content -Raw -LiteralPath $sessionAStatusWithMpPath | ConvertFrom-Json
    $sessionBStatusWithMp = Get-Content -Raw -LiteralPath $sessionBStatusWithMpPath | ConvertFrom-Json
    if ($null -eq $sessionAStatusWithMp.mp_overlay_package_status.PSObject.Properties["warning_codes"]) {
        $sessionAStatusWithMp.mp_overlay_package_status | Add-Member -NotePropertyName "warning_codes" -NotePropertyValue @()
    }
    if ($null -eq $sessionBStatusWithMp.mp_overlay_package_status.PSObject.Properties["warning_codes"]) {
        $sessionBStatusWithMp.mp_overlay_package_status | Add-Member -NotePropertyName "warning_codes" -NotePropertyValue @()
    }
    $sessionAStatusWithMp.mp_overlay_package_status.warning_codes = @("package_overlay_version_mismatch")
    $sessionBStatusWithMp.mp_overlay_package_status.warning_codes = @("package_game_version_mismatch")
    $sessionAStatusWithMp | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $sessionAStatusWithMpPath -Encoding UTF8
    $sessionBStatusWithMp | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $sessionBStatusWithMpPath -Encoding UTF8

    $compareOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "compare_real_session_v0_outputs.ps1") (Join-Path $trendRoot "session_a") (Join-Path $trendRoot "session_b") $compareOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "real session warning-code drift compare failed (exit code $LASTEXITCODE)."
    }
    $compareText = $compareOutput -join "`n"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_warning_codes_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_identity_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_identity_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_identity_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_identity_mismatch_warning_codes_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_game_version_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_game_version_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_game_version_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_campaign_id_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_campaign_id_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_campaign_id_mismatch_warning_changed=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_overlay_version_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_overlay_version_mismatch_warning_previous=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_overlay_version_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mod_version_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mod_version_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mod_version_mismatch_warning_changed=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_manifest_hash_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_manifest_hash_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_manifest_hash_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mismatch_warning_state_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mismatch_warning_reason_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_mismatch_warning_codes_changed="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_provenance_state_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_source_quality_count_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_bootstrap_campaign_count_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_status_snapshot_present_current=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_status_snapshot_present_previous=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_status_snapshot_present_changed=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_identity_mismatch_warning_code_current=package_manifest_hash_mismatch"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_warning_code_previous=package_overlay_version_mismatch"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_warning_code_current=package_game_version_mismatch"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_handoff_status_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_previous_host_available_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_mp_package_output_dir_changed=true"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_state_current=accepted"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_state_previous=accepted"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_state_changed=false"
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_reason_current="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_reason_previous="
    Assert-Contains -Name "real session warning-code drift compare" -Text $compareText -Expected "real_session_v0_compare_gameplay_acceptance_reason_changed=false"

    $trendOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "analyze_real_session_v0_trend.ps1") $trendRoot $trendOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "real session warning-code drift trend failed (exit code $LASTEXITCODE)."
    }
    $trendText = $trendOutput -join "`n"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_warning_codes_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_identity_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_identity_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_identity_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_identity_mismatch_warning_codes_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_game_version_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_game_version_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_game_version_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_campaign_id_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_campaign_id_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_campaign_id_mismatch_warning_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_overlay_version_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_overlay_version_mismatch_warning_previous=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_overlay_version_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mod_version_mismatch_warning_current=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mod_version_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mod_version_mismatch_warning_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_manifest_hash_mismatch_warning_current=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_manifest_hash_mismatch_warning_previous=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_manifest_hash_mismatch_warning_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mismatch_warning_state_changed="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mismatch_warning_reason_changed="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_mismatch_warning_codes_changed="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_provenance_state_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_source_quality_count_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_bootstrap_campaign_count_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_identity_mismatch_warning_code_current=package_manifest_hash_mismatch"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_warning_code_previous=package_overlay_version_mismatch"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_warning_code_current=package_game_version_mismatch"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_handoff_status_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_previous_host_available_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_mp_package_output_dir_changed=true"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_state_current=accepted"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_state_previous=accepted"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_state_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_reason_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_reason_previous="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_gameplay_acceptance_reason_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_state_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_state_previous="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_state_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_reason_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_reason_previous="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_reason_changed=false"
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_summary_text_current="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_summary_text_previous="
    Assert-Contains -Name "real session warning-code drift trend" -Text $trendText -Expected "real_session_v0_trend_status_center_summary_text_changed="

    Write-Host "[PASS] real_session_warning_code_drift_surface"
}

function Invoke-RealSessionLoopMismatchForwardingCase {
    $saveRoot = Join-Path $repoRoot "dist/test_real_session_loop_saves"
    $archiveRoot = Join-Path $repoRoot "dist/test_real_session_loop_archive"
    $dslPath = Join-Path $repoRoot "resources/generated_overlay_valid.dsl"
    $previousSessionSourceDir = Join-Path $repoRoot "dist/real_session_v0_loop/session_manual_smoke_mp5"
    $previousSessionFixtureRoot = Join-Path $repoRoot "dist/test_real_session_loop_previous_for_handoff"
    Remove-Item -LiteralPath $previousSessionFixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item -LiteralPath $previousSessionSourceDir -Destination $previousSessionFixtureRoot -Recurse
    $previousStatusWithMpPath = Join-Path $previousSessionFixtureRoot "snc_status_snapshot_with_mp.json"
    $previousStatusWithMp = Get-Content -Raw -LiteralPath $previousStatusWithMpPath | ConvertFrom-Json
    $previousStatusWithMp.mp_overlay_package_status.handoff_status = "degraded_previous_host_unavailable"
    $previousStatusWithMp | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $previousStatusWithMpPath -Encoding UTF8
    $previousSessionDir = $previousSessionFixtureRoot
    $sessionId = "session_test_mismatch_forwarding_" + [DateTime]::UtcNow.ToString("yyyyMMdd_HHmmss")

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_real_session_v0_loop.ps1") `
        $saveRoot `
        $archiveRoot `
        "campaign_001" `
        "empire_001" `
        $dslPath `
        -SessionId $sessionId `
        -PreviousSessionDirForCompare $previousSessionDir `
        -EmitTrendSummary `
        -ExportMpPackage
    if ($LASTEXITCODE -ne 0) {
        throw "real session loop mismatch forwarding case failed (exit code $LASTEXITCODE)."
    }

    $text = $output -join "`n"
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_game_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_campaign_id_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_overlay_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_mod_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_manifest_hash_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_mismatch_warning_state_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_mismatch_warning_reason_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_status_snapshot_present_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_status_snapshot_present_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_status_snapshot_present_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_verified_archive_save_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_verified_archive_save_count_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_verified_archive_save_count_delta="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_verified_archive_save_count_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_state_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_state_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_reason_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_reason_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_reason_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_summary_text_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_summary_text_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_status_center_summary_text_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_save_root_resolution_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_save_root_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_save_root_campaign_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_command_hint_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_command_hint_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_command_hint_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_package_zip_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_package_zip_reason_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_package_zip_sha256_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_package_zip_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_package_zip_bytes_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_provenance_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_source_quality_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_bootstrap_campaign_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_handoff_status_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_previous_host_available_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_recommendation=review_mp_handoff_continuity"
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_handoff_follow_up_active=true"
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_mp_handoff_follow_up_reason=handoff_status_changed_between_sessions"
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_game_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_campaign_id_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_overlay_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_mod_version_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_manifest_hash_mismatch_warning_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_mismatch_warning_state_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_mismatch_warning_reason_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_state_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_state_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_reason_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_reason_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_reason_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_summary_text_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_summary_text_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_status_center_summary_text_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_save_root_resolution_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_save_root_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_save_root_campaign_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_command_hint_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_command_hint_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_command_hint_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_package_zip_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_package_zip_reason_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_package_zip_sha256_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_package_zip_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_package_zip_bytes_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_provenance_state_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_source_quality_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_bootstrap_campaign_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_handoff_status_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_mp_previous_host_available_current="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_game_version_mismatch_warning="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_mod_version_mismatch_warning="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_manifest_hash_mismatch_warning="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_campaign_id_mismatch_warning="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_overlay_version_mismatch_warning="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_mismatch_warning_state="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_mismatch_warning_reason="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_provenance_state="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_source_quality_count="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_bootstrap_campaign_count="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_zip_state=ready"
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_zip_reason=zip_created"
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_zip_path="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_zip_sha256="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_zip_bytes="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_handoff_status="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_mp_package_previous_host_available="
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_provenance_state=present"
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_source_quality_count=1"
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_source_quality=history_backed"
    Assert-Contains -Name "real session loop mismatch forwarding export" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_bootstrap_campaign_count=0"
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_run_id=real-session-v0-loop-"
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_archive_copied_save_count="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_archive_last_archived_path="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_status_center_state="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_status_center_reason="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_status_center_summary_text="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action_reason="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action_command_hint_source="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action=review_mp_handoff_continuity"
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action_reason=compare_handoff_status_changed_between_sessions"
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_action_command_hint_source=compare_mp_handoff_follow_up"
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected 'real_session_v0_loop_next_action_command_hint=cmd /c tools\compare_real_session_v0_outputs.cmd "'
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_path_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_next_action_path_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_plan_readiness_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_plan_readiness_previous="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_plan_readiness_changed="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_limit_reached_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_skipped_due_to_limit_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_follow_up_active=true"
    Assert-Contains -Name "real session loop mismatch forwarding compare" -Text $text -Expected "real_session_v0_loop_compare_auto_campaign_library_follow_up_reason="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_path_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_path_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_next_action_path_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_plan_readiness_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_plan_readiness_previous="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_plan_readiness_changed="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_limit_reached_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_skipped_due_to_limit_count_current="
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_follow_up_active=true"
    Assert-Contains -Name "real session loop mismatch forwarding trend" -Text $text -Expected "real_session_v0_loop_trend_auto_campaign_library_follow_up_reason="
    Assert-Contains -Name "real session loop mismatch forwarding output" -Text $text -Expected "real_session_v0_loop_next_steps_brief="
    $runIdLine = ($output | Where-Object { $_ -like "real_session_v0_loop_run_id=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($runIdLine)) {
        throw "real session loop mismatch forwarding case missing run id line."
    }
    $runIdValue = $runIdLine.Substring("real_session_v0_loop_run_id=".Length)

    $evidencePathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_evidence_json=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($evidencePathLine)) {
        throw "real session loop mismatch forwarding case missing evidence path."
    }
    $evidencePath = $evidencePathLine.Substring("real_session_v0_loop_evidence_json=".Length)
    if (-not (Test-Path -LiteralPath $evidencePath)) {
        throw "real session loop mismatch forwarding evidence json missing: $evidencePath"
    }
    $evidenceText = Get-Content -Raw -LiteralPath $evidencePath
    $evidenceJson = $evidenceText | ConvertFrom-Json
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"game_version_mismatch_warning_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"campaign_id_mismatch_warning_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"overlay_version_mismatch_warning_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"mod_version_mismatch_warning_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"manifest_hash_mismatch_warning_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"mismatch_warning_state_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"mismatch_warning_reason_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"status_snapshot_present_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"status_snapshot_present_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence compare" -Text $evidenceText -Expected '"status_snapshot_present_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence status center" -Text $evidenceText -Expected '"summary_text"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence save root compare" -Text $evidenceText -Expected '"save_root_resolution_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence save root compare" -Text $evidenceText -Expected '"save_root_path_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence save root trend" -Text $evidenceText -Expected '"save_root_resolution_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"game_version_mismatch_warning"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"mod_version_mismatch_warning"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"manifest_hash_mismatch_warning"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"campaign_id_mismatch_warning"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"overlay_version_mismatch_warning"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"mismatch_warning_state"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"mismatch_warning_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"package_zip_state"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"package_zip_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"package_zip_path"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"package_zip_sha256"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"package_zip_bytes"'
    if ($evidenceJson.mp_export.provenance_state -ne "not_exported") {
        throw "real session loop mismatch forwarding evidence export expected mp_export.provenance_state=not_exported, got '$($evidenceJson.mp_export.provenance_state)'."
    }
    if ([string]$evidenceJson.mp_export.source_quality_count -ne "0") {
        throw "real session loop mismatch forwarding evidence export expected mp_export.source_quality_count=0, got '$($evidenceJson.mp_export.source_quality_count)'."
    }
    if (@($evidenceJson.mp_export.source_qualities).Count -ne 0) {
        throw "real session loop mismatch forwarding evidence export expected mp_export.source_qualities to be empty."
    }
    if ([string]$evidenceJson.mp_export.bootstrap_campaign_count -ne "0") {
        throw "real session loop mismatch forwarding evidence export expected mp_export.bootstrap_campaign_count=0, got '$($evidenceJson.mp_export.bootstrap_campaign_count)'."
    }
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"handoff_status"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence export" -Text $evidenceText -Expected '"previous_host_available"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence mp snapshot" -Text $evidenceText -Expected '"status_snapshot_with_mp_path"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence mp snapshot" -Text $evidenceText -Expected '"status_snapshot_with_mp_readiness"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence mp snapshot" -Text $evidenceText -Expected '"provenance_state":  "not_exported"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence mp snapshot" -Text $evidenceText -Expected '"source_quality_count":  "0"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence mp snapshot" -Text $evidenceText -Expected '"bootstrap_campaign_count":  "0"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence metadata" -Text $evidenceText -Expected '"run_id":'
    Assert-Contains -Name "real session loop mismatch forwarding evidence metadata" -Text $evidenceText -Expected 'real-session-v0-loop-'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"command_hints"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"compare"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"trend"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"next_session"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"next_steps_brief"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"mp_verify"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"mp_import"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"mp_strict_verify"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence command hints" -Text $evidenceText -Expected '"mp_strict_import"'
    $nextStepsBriefPathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_next_steps_brief=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($nextStepsBriefPathLine)) {
        throw "real session loop mismatch forwarding case missing next-steps brief path."
    }
    $nextStepsBriefPath = $nextStepsBriefPathLine.Substring("real_session_v0_loop_next_steps_brief=".Length)
    if (-not (Test-Path -LiteralPath $nextStepsBriefPath)) {
        throw "real session loop mismatch forwarding next-steps brief missing: $nextStepsBriefPath"
    }
    $nextStepsBriefText = Get-Content -Raw -LiteralPath $nextStepsBriefPath
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "SNC MP status snapshot:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "SNC MP status readiness: ready_for_mp"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "## MP Package"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Status snapshot with MP:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Status readiness: ready_for_mp"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Provenance state: present"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Source quality count: 1"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Source qualities: history_backed"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "- Bootstrap campaign count: 0"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Host next step:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Client next step:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Package zip state: ready"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Package zip reason: zip_created"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Package zip SHA256:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Package zip bytes:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Strict verify command:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Strict import command:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Mismatch warning reason:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Game version mismatch warning:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Manifest hash mismatch warning:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "## Campaign Library"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "## Campaign Library Drift"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Compare readiness previous/current/changed:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Trend readiness previous/current/changed:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Compare follow-up active/reason:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Trend follow-up active/reason:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Compare MP handoff follow-up active/reason:"
    Assert-Contains -Name "real session loop mismatch forwarding next-steps brief" -Text $nextStepsBriefText -Expected "Trend MP handoff follow-up active/reason:"
    Assert-Contains -Name "real session loop mismatch forwarding evidence archive" -Text $evidenceText -Expected '"archive"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence archive" -Text $evidenceText -Expected '"copied_save_count"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence archive" -Text $evidenceText -Expected '"last_archived_path"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"auto_compare"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"command_hint"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"verified_archive_save_count_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"verified_archive_save_count_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"verified_archive_save_count_delta"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"verified_archive_save_count_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_state_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_state_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_state_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_reason_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_reason_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_reason_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_summary_text_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_summary_text_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"status_center_summary_text_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_command_hint_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_command_hint_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_command_hint_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_path_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_path_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"next_action_path_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"campaign_library"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"plan_readiness_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"limit_reached_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"follow_up_active"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"follow_up_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"handoff_follow_up_active"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"handoff_follow_up_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"package_zip_state_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"package_zip_reason_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"package_zip_sha256_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"package_zip_path_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto compare" -Text $evidenceText -Expected '"package_zip_bytes_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"auto_trend"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"latest_compare_command_hint"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_session_command_hint"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_state_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_state_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_state_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_reason_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_reason_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_reason_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_summary_text_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_summary_text_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"status_center_summary_text_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_path_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_path_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_path_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_command_hint_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_command_hint_previous"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"next_action_command_hint_changed"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"package_zip_state_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"package_zip_reason_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"package_zip_sha256_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"package_zip_path_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"package_zip_bytes_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"campaign_library"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"plan_readiness_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"limit_reached_current"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"follow_up_active"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"follow_up_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"handoff_follow_up_active"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence auto trend" -Text $evidenceText -Expected '"handoff_follow_up_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence status center" -Text $evidenceText -Expected '"status_center"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence status center" -Text $evidenceText -Expected '"summary_present"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"entry_point_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"post_play_package_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"decision_input_package_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"decision_input_blocked_entry_count"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"candidate_decision_package_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"candidate_decision_blocked_source_entry_count"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"dsl_draft_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"dsl_draft_eligible_candidate_count"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"dsl_draft_skipped_candidate_count"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence entry point post play" -Text $evidenceText -Expected '"generated_overlay_staging_reason"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence next action" -Text $evidenceText -Expected '"next_action"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence next action" -Text $evidenceText -Expected '"command_hint_source"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence next action" -Text $evidenceText -Expected '"review_mp_handoff_continuity"'
    Assert-Contains -Name "real session loop mismatch forwarding evidence next action" -Text $evidenceText -Expected '"compare_mp_handoff_follow_up"'
    if ($evidenceText -match [regex]::Escape("System.Object[]")) {
        throw "real session loop mismatch forwarding evidence arrays contains serialized System.Object[] placeholder."
    }
    $evidenceJson = $evidenceText | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.run_id)) {
        throw "real session loop mismatch forwarding evidence json missing run_id value."
    }
    if ([string]$evidenceJson.run_id -ne $runIdValue) {
        throw "real session loop mismatch forwarding run_id mismatch. CLI='$runIdValue' evidence='$($evidenceJson.run_id)'."
    }
    if ($null -eq $evidenceJson.evidence_schema_version) {
        throw "real session loop mismatch forwarding evidence json missing evidence_schema_version."
    }
    if ([int]$evidenceJson.evidence_schema_version -ne 1) {
        throw "real session loop mismatch forwarding evidence schema version mismatch. Expected 1 but got '$($evidenceJson.evidence_schema_version)'."
    }
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.generated_at_utc)) {
        throw "real session loop mismatch forwarding evidence json missing generated_at_utc value."
    }
    $generatedAtUtc = [datetimeoffset]::MinValue
    if (-not [datetimeoffset]::TryParse([string]$evidenceJson.generated_at_utc, [ref]$generatedAtUtc)) {
        throw "real session loop mismatch forwarding evidence json generated_at_utc is not a valid timestamp: '$($evidenceJson.generated_at_utc)'."
    }
    if ($null -eq $evidenceJson.archive) {
        throw "real session loop mismatch forwarding evidence json missing archive object."
    }
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.archive.copied_save_count)) {
        throw "real session loop mismatch forwarding evidence json missing archive.copied_save_count."
    }

    Write-Host "[PASS] real_session_loop_mismatch_forwarding"
}

function Invoke-AmbiguousPostPlayCliFailClosedCase {
    $root = Join-Path $repoRoot "dist\test_ambiguous_post_play_cli"
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue

    $captureRoot = Join-Path $root "capture_root"
    $currentRoot = Join-Path $root "current_root"
    $archiveRoot = Join-Path $root "archive"
    $sessionId = "session_ambiguous_post_play_cli"
    $postPlayPackagePath = Join-Path $root "snc_post_play_package.json"
    $decisionInputPackagePath = Join-Path $root "snc_decision_input_package.json"
    $candidateDecisionPackagePath = Join-Path $root "snc_candidate_decision_package.json"
    $captureCampaignRoot = Join-Path $captureRoot "Gamma Campaign"
    $currentCampaignRoot = Join-Path $currentRoot "Gamma Campaign"

    New-Item -ItemType Directory -Force -Path $captureCampaignRoot, $currentCampaignRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $captureCampaignRoot "autosave_2200.01.01.sav") -Value "gamma branch one" -NoNewline
    & $exePath --archive-live-saves $captureRoot $archiveRoot $sessionId 0
    Assert-LastExitCodeOk -StepName "ambiguous post-play cli archive branch one"

    Set-Content -LiteralPath (Join-Path $captureCampaignRoot "autosave_2200.01.01.sav") -Value "gamma branch two" -NoNewline
    & $exePath --archive-live-saves $captureRoot $archiveRoot $sessionId 0
    Assert-LastExitCodeOk -StepName "ambiguous post-play cli archive branch two"

    Set-Content -LiteralPath (Join-Path $currentCampaignRoot "autosave_2200.01.01.sav") -Value "gamma branch two" -NoNewline
    Set-Content -LiteralPath (Join-Path $currentCampaignRoot "2200.02.01.sav") -Value "gamma manual entry point" -NoNewline

    $postPlayOutput = & $exePath --build-post-play-package (Join-Path $archiveRoot $sessionId) $postPlayPackagePath $currentRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Ambiguous post-play package CLI failed. Actual output:`n$($postPlayOutput -join "`n")"
    }
    $postPlayText = $postPlayOutput -join "`n"
    Assert-Contains -Name "ambiguous post-play cli" -Text $postPlayText -Expected "post_play_package_success=true"
    Assert-Contains -Name "ambiguous post-play cli" -Text $postPlayText -Expected "post_play_package_readiness=ambiguous"
    Assert-Contains -Name "ambiguous post-play cli" -Text $postPlayText -Expected "post_play_package_campaign_identity_state_summary=folder_alias_fallback"
    Assert-Contains -Name "ambiguous post-play cli" -Text $postPlayText -Expected "post_play_package_branch_ambiguity_detected=true"
    Assert-Contains -Name "ambiguous post-play cli" -Text $postPlayText -Expected "post_play_package_decision_ready_entry_count=0"

    $postPlayJsonText = Get-Content -LiteralPath $postPlayPackagePath -Raw
    $postPlayJson = $postPlayJsonText | ConvertFrom-Json
    if ($postPlayJson.branch_ambiguity_detected -ne $true) {
        throw "Ambiguous post-play package JSON did not expose branch_ambiguity_detected=true."
    }
    if ($postPlayJson.campaign_identity_state_summary -ne "folder_alias_fallback") {
        throw "Ambiguous post-play package JSON did not expose campaign_identity_state_summary=folder_alias_fallback."
    }
    if ([int]$postPlayJson.decision_ready_entry_count -ne 0) {
        throw "Ambiguous post-play package JSON exposed decision-ready entries."
    }
    Assert-Contains -Name "ambiguous post-play cli json" -Text $postPlayJsonText -Expected '"post_play_branch_ambiguity_blocks_decision_input"'

    $decisionInputOutput = & $exePath --build-snc-decision-input-package $postPlayPackagePath $decisionInputPackagePath
    if ($LASTEXITCODE -ne 0) {
        throw "Ambiguous decision-input CLI failed. Actual output:`n$($decisionInputOutput -join "`n")"
    }
    $decisionInputText = $decisionInputOutput -join "`n"
    Assert-Contains -Name "ambiguous decision-input cli" -Text $decisionInputText -Expected "snc_decision_input_package_success=true"
    Assert-Contains -Name "ambiguous decision-input cli" -Text $decisionInputText -Expected "snc_decision_input_package_readiness=needs_attention"
    Assert-Contains -Name "ambiguous decision-input cli" -Text $decisionInputText -Expected "snc_decision_input_package_decision_inputs=0"
    Assert-Contains -Name "ambiguous decision-input cli" -Text $decisionInputText -Expected "snc_decision_input_package_blocked_entries=2"

    $decisionInputJson = Get-Content -LiteralPath $decisionInputPackagePath -Raw | ConvertFrom-Json
    if ([int]$decisionInputJson.decision_input_count -ne 0) {
        throw "Ambiguous decision-input package JSON exposed decision inputs."
    }
    if ([int]$decisionInputJson.blocked_entry_count -lt 1) {
        throw "Ambiguous decision-input package JSON did not keep blocked entries."
    }

    $candidateDecisionOutput = & $exePath --build-snc-candidate-decision-package $decisionInputPackagePath $candidateDecisionPackagePath
    if ($LASTEXITCODE -ne 0) {
        throw "Ambiguous candidate-decision CLI failed. Actual output:`n$($candidateDecisionOutput -join "`n")"
    }
    $candidateDecisionText = $candidateDecisionOutput -join "`n"
    Assert-Contains -Name "ambiguous candidate-decision cli" -Text $candidateDecisionText -Expected "snc_candidate_decision_package_success=true"
    Assert-Contains -Name "ambiguous candidate-decision cli" -Text $candidateDecisionText -Expected "snc_candidate_decision_package_readiness=needs_attention"
    Assert-Contains -Name "ambiguous candidate-decision cli" -Text $candidateDecisionText -Expected "snc_candidate_decision_package_candidate_decisions=0"
    Assert-Contains -Name "ambiguous candidate-decision cli" -Text $candidateDecisionText -Expected "snc_candidate_decision_package_blocked_source_entries=2"

    $candidateDecisionJson = Get-Content -LiteralPath $candidateDecisionPackagePath -Raw | ConvertFrom-Json
    if ([int]$candidateDecisionJson.candidate_decision_count -ne 0) {
        throw "Ambiguous candidate-decision package JSON exposed candidate decisions."
    }

    Write-Host "[PASS] ambiguous_post_play_cli_fail_closed"
}

function Invoke-RealSessionLoopMpSnapshotContractCase {
    $saveRoot = Join-Path $repoRoot "dist/test_real_session_loop_saves"
    $archiveRoot = Join-Path $repoRoot "dist/test_real_session_loop_archive"
    $dslPath = Join-Path $repoRoot "resources/generated_overlay_valid.dsl"
    $sessionId = "session_test_mp_snapshot_contract_" + [DateTime]::UtcNow.ToString("yyyyMMdd_HHmmss")

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_real_session_v0_loop.ps1") `
        $saveRoot `
        $archiveRoot `
        "campaign_001" `
        "empire_001" `
        $dslPath `
        -SessionId $sessionId
    if ($LASTEXITCODE -ne 0) {
        throw "real session loop mp snapshot contract case failed (exit code $LASTEXITCODE)."
    }

    $text = $output -join "`n"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_status_snapshot_with_mp_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_status_snapshot_with_mp_readiness=not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_status_center_state="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_status_center_reason="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_plan_present="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_plan_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_plan_source="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_plan_readiness="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_plan_reason="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_limit_reached="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_campaign_library_skipped_due_to_limit_count="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_next_action="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_next_action_reason="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_next_action_command_hint_source=loop_next_session"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected 'real_session_v0_loop_next_action_command_hint=cmd /c tools\run_real_session_v0_loop.cmd "'
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_next_action_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_snc_next_action="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_owner_test_ready=false"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_owner_test_playbook_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_entry_point_analysis_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_entry_point_readiness="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_state="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_reason="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_confidence="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_warning_visible="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_anchor_entry_point_id="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_anchor_save_name="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_anchor_source_kind="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_memory_recovery_anchor_archived_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_post_play_package_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_decision_input_package_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_candidate_decision_package_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_dsl_draft_readiness="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_snc_generated_overlay_staging_readiness="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_season_delta_ledger_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_empire_brief_path="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_next_steps_brief="
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_mismatch_warning_state=not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_mismatch_warning_reason=mp_export_not_requested"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_zip_state=not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_zip_reason=mp_export_not_requested"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_provenance_state=not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_source_quality_count=0"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_mp_package_bootstrap_campaign_count=0"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_provenance_state=not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_source_quality_count=0"
    Assert-Contains -Name "real session loop mp snapshot contract output" -Text $text -Expected "real_session_v0_loop_snc_mp_overlay_package_bootstrap_campaign_count=0"

    $evidencePathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_evidence_json=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($evidencePathLine)) {
        throw "real session loop mp snapshot contract case missing evidence path."
    }
    $evidencePath = $evidencePathLine.Substring("real_session_v0_loop_evidence_json=".Length)
    if (-not (Test-Path -LiteralPath $evidencePath)) {
        throw "real session loop mp snapshot contract evidence json missing: $evidencePath"
    }
    $evidenceText = Get-Content -Raw -LiteralPath $evidencePath
    $evidenceJson = $evidenceText | ConvertFrom-Json
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"status_snapshot_with_mp_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"status_snapshot_with_mp_readiness":'
    if ($evidenceJson.mp_export.status_snapshot_with_mp_readiness -ne "not_exported") {
        throw "real session loop mp snapshot contract evidence expected mp_export.status_snapshot_with_mp_readiness=not_exported, got '$($evidenceJson.mp_export.status_snapshot_with_mp_readiness)'."
    }
    if ($evidenceJson.mp_export.provenance_state -ne "not_exported") {
        throw "real session loop mp snapshot contract evidence expected mp_export.provenance_state=not_exported, got '$($evidenceJson.mp_export.provenance_state)'."
    }
    if ([string]$evidenceJson.mp_export.source_quality_count -ne "0") {
        throw "real session loop mp snapshot contract evidence expected mp_export.source_quality_count=0, got '$($evidenceJson.mp_export.source_quality_count)'."
    }
    if (@($evidenceJson.mp_export.source_qualities).Count -ne 0) {
        throw "real session loop mp snapshot contract evidence expected mp_export.source_qualities to be empty."
    }
    if ([string]$evidenceJson.mp_export.bootstrap_campaign_count -ne "0") {
        throw "real session loop mp snapshot contract evidence expected mp_export.bootstrap_campaign_count=0, got '$($evidenceJson.mp_export.bootstrap_campaign_count)'."
    }
    if ($null -eq $evidenceJson.memory_recovery) {
        throw "real session loop mp snapshot contract evidence is missing memory_recovery."
    }
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.memory_recovery.state)) {
        throw "real session loop mp snapshot contract evidence missing memory_recovery.state."
    }
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.memory_recovery.confidence)) {
        throw "real session loop mp snapshot contract evidence missing memory_recovery.confidence."
    }
    if ([string]::IsNullOrWhiteSpace([string]$evidenceJson.memory_recovery.anchor_save_name)) {
        throw "real session loop mp snapshot contract evidence missing memory_recovery.anchor_save_name."
    }
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"save_root":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"resolution":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"source":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"entry_point_post_play":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"memory_recovery":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"anchor_entry_point_id":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"anchor_save_name":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"anchor_source_kind":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"anchor_archived_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"entry_point_analysis_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"post_play_package_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"decision_input_package_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"candidate_decision_package_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"dsl_draft_readiness":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"generated_overlay_staging_readiness":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"season_delta_ledger_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"empire_brief_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"status_center":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"summary_present":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"campaign_library":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"plan_present":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"plan_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"plan_source":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"plan_readiness":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"plan_reason":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"limit_reached":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"skipped_due_to_limit_count":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"next_action":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"command_hint_source":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"loop_next_session"'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"snc_owner_test_contract":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"playbook_path":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"next_steps_brief":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"mismatch_warning_state":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"mismatch_warning_reason":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"package_zip_state":'
    Assert-Contains -Name "real session loop mp snapshot contract evidence" -Text $evidenceText -Expected '"package_zip_reason":'
    if ($null -eq $evidenceJson.snc_owner_test_contract) {
        throw "real_session_loop_mp_snapshot_contract evidence is missing snc_owner_test_contract."
    }
    if ([bool]$evidenceJson.snc_owner_test_contract.ready) {
        throw "real_session_loop_mp_snapshot_contract unexpectedly exposed a ready owner-test contract in the fail-closed case."
    }
    if ([string]$evidenceJson.snc_owner_test_contract.action -eq "run_monthly_reactive_owner_test") {
        throw "real_session_loop_mp_snapshot_contract unexpectedly selected the owner-test next action in the fail-closed case."
    }
    if ([string]$evidenceJson.snc_owner_test_contract.reason -eq "published_monthly_reactive_overlay_ready_for_owner_test") {
        throw "real_session_loop_mp_snapshot_contract unexpectedly reported the owner-test readiness reason in the fail-closed case."
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$evidenceJson.snc_owner_test_contract.playbook_path)) {
        throw "real_session_loop_mp_snapshot_contract unexpectedly exposed a playbook path in the fail-closed case."
    }
    $nextStepsBriefPathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_next_steps_brief=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($nextStepsBriefPathLine)) {
        throw "real session loop mp snapshot contract case missing next-steps brief path."
    }
    $nextStepsBriefPath = $nextStepsBriefPathLine.Substring("real_session_v0_loop_next_steps_brief=".Length)
    if (-not (Test-Path -LiteralPath $nextStepsBriefPath)) {
        throw "real session loop mp snapshot contract next-steps brief missing: $nextStepsBriefPath"
    }
    $nextStepsBriefText = Get-Content -Raw -LiteralPath $nextStepsBriefPath
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Entry point analysis:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Post-play package:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Decision input package:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Candidate decision package:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- DSL draft readiness:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- SNC staged overlay readiness:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Season delta ledger:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Empire brief:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- SNC MP status snapshot:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- SNC MP status readiness: not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "## MP Package"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Status snapshot with MP:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Status readiness: not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Provenance state: not_exported"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Source quality count: 0"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Source qualities: none"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Bootstrap campaign count: 0"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "## Campaign Library"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Plan readiness:"
    Assert-Contains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Expected "- Limit reached:"
    Assert-NotContains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Unexpected "## Monthly Reactive Owner Test"
    Assert-NotContains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Unexpected "- Host readiness:"
    Assert-NotContains -Name "real session loop mp snapshot contract brief" -Text $nextStepsBriefText -Unexpected "- Client readiness gate:"

    Write-Host "[PASS] real_session_loop_mp_snapshot_contract"
}

function Invoke-RealSessionLoopNextActionStrictVerifySourceContractCase {
    $scriptPath = Join-Path $PSScriptRoot "run_real_session_v0_loop.ps1"
    $scriptText = Get-Content -Raw -LiteralPath $scriptPath

    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'mp_export_strict_verify'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'compare_current_mp_strict_verify'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'trend_current_mp_strict_verify'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'run_monthly_reactive_owner_test'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'published_monthly_reactive_overlay_ready_for_owner_test'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'snc_owner_test_contract'
    Assert-Contains -Name "real session loop next-action source contract" -Text $scriptText -Expected 'owner_test_playbook_path'

    Write-Host "[PASS] real_session_loop_next_action_strict_verify_source_contract"
}

function Invoke-RealSessionLoopAutoResolutionCase {
    $saveRoot = Join-Path $repoRoot "dist/real_session_loop_fixture_saves"
    $archiveRoot = Join-Path $repoRoot "dist/test_real_session_loop_archive"
    $sessionId = "session_test_auto_resolution_" + [DateTime]::UtcNow.ToString("yyyyMMdd_HHmmss")

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_real_session_v0_loop.ps1") `
        $saveRoot `
        $archiveRoot `
        "" `
        "" `
        "" `
        -SessionId $sessionId
    if ($LASTEXITCODE -ne 0) {
        throw "real session loop auto resolution case failed (exit code $LASTEXITCODE)."
    }

    $text = $output -join "`n"
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_campaign_id="
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_campaign_id_source=auto_"
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_empire_id=player_country_"
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_empire_id_source=auto_"
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_dsl_input_path="
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected "real_session_v0_loop_dsl_input_source=auto_generated_dsl_draft"
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected 'real_session_v0_loop_next_session_command_hint=cmd /c tools\run_real_session_v0_loop.cmd "'
    Assert-Contains -Name "real session loop auto resolution output" -Text $text -Expected '" "" "" "" -PreviousSessionDirForCompare "'

    $evidencePathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_evidence_json=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($evidencePathLine)) {
        throw "real session loop auto resolution case missing evidence path."
    }
    $evidencePath = $evidencePathLine.Substring("real_session_v0_loop_evidence_json=".Length)
    if (-not (Test-Path -LiteralPath $evidencePath)) {
        throw "real session loop auto resolution evidence json missing: $evidencePath"
    }
    $evidenceText = Get-Content -Raw -LiteralPath $evidencePath
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected '"resolved_identity":'
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected '"campaign_id_source":'
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected '"empire_id_source":'
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected '"dsl_input":'
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected '"source":'
    Assert-Contains -Name "real session loop auto resolution evidence" -Text $evidenceText -Expected 'auto_generated_dsl_draft'
    $evidenceJson = $evidenceText | ConvertFrom-Json
    if (-not ([string]$evidenceJson.resolved_identity.campaign_id_source).StartsWith("auto_")) {
        throw "real session loop auto resolution evidence campaign_id_source should start with auto_."
    }
    if (-not ([string]$evidenceJson.resolved_identity.empire_id_source).StartsWith("auto_")) {
        throw "real session loop auto resolution evidence empire_id_source should start with auto_."
    }

    $nextStepsBriefPathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_next_steps_brief=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($nextStepsBriefPathLine)) {
        throw "real session loop auto resolution case missing next-steps brief path."
    }
    $nextStepsBriefPath = $nextStepsBriefPathLine.Substring("real_session_v0_loop_next_steps_brief=".Length)
    if (-not (Test-Path -LiteralPath $nextStepsBriefPath)) {
        throw "real session loop auto resolution next-steps brief missing: $nextStepsBriefPath"
    }
    $nextStepsBriefText = Get-Content -Raw -LiteralPath $nextStepsBriefPath
    Assert-Contains -Name "real session loop auto resolution brief" -Text $nextStepsBriefText -Expected "Campaign ID source: auto_"
    Assert-Contains -Name "real session loop auto resolution brief" -Text $nextStepsBriefText -Expected "Empire ID source: auto_"
    Assert-Contains -Name "real session loop auto resolution brief" -Text $nextStepsBriefText -Expected "DSL input source: auto_generated_dsl_draft"

    Write-Host "[PASS] real_session_loop_auto_resolution"
}

function Invoke-RealSessionLoopMultiCampaignEligibleAutoResolutionCase {
    $saveRoot = Join-Path $repoRoot "dist/real_session_loop_multi_campaign_fixture_saves"
    $archiveRoot = Join-Path $repoRoot "dist/real_session_loop_multi_campaign_fixture_archive"
    $sessionId = "session_test_multi_campaign_auto_resolution_" + [DateTime]::UtcNow.ToString("yyyyMMdd_HHmmss")

    function New-TestSavArchive {
        param(
            [string]$SavPath,
            [string]$MetaText,
            [string]$GamestateText
        )

        $stagingDir = Join-Path ([System.IO.Path]::GetDirectoryName($SavPath)) ("sav_stage_" + [guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
        Set-Content -LiteralPath (Join-Path $stagingDir "meta") -Value $MetaText -NoNewline
        Set-Content -LiteralPath (Join-Path $stagingDir "gamestate") -Value $GamestateText -NoNewline
        Push-Location $stagingDir
        try {
            & tar -cf $SavPath "meta" "gamestate" | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "tar failed while creating test save archive: $SavPath"
            }
        } finally {
            Pop-Location
            Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    Remove-Item -LiteralPath $saveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $archiveRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null

    $validMeta = @'
version="v4.0.22"
revision="abcdef"
name="Alpha Campaign"
'@
    $validGamestate = @'
date="2230.01.01"
player={
    {
        name="Jeniksoft"
        country=0
    }
}
country={
    0={
        name="Aeel Corp"
        government="gov_megacorporation"
        authority="auth_corporate"
        founder_species=7
        capital=42
        starting_system=9
    }
}
species={
    7={
        name="Aeel"
    }
}
planet={
    42={
        name="Aeel Prime"
    }
}
galactic_object={
    9={
        name="Aeel System"
    }
}
'@
    $partialMeta = @'
version="v4.0.22"
revision="ghijkl"
name="Beta Campaign"
'@
    $partialGamestate = @'
date="2230.01.01"
country={
    1={
        name="Unresolved Observer"
    }
}
'@

    New-TestSavArchive -SavPath (Join-Path $saveRoot "alpha_campaign_2230.01.01.sav") -MetaText $validMeta -GamestateText $validGamestate
    New-TestSavArchive -SavPath (Join-Path $saveRoot "beta_campaign_2230.01.01.sav") -MetaText $partialMeta -GamestateText $partialGamestate

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_real_session_v0_loop.ps1") `
        $saveRoot `
        $archiveRoot `
        "" `
        "" `
        "" `
        -SessionId $sessionId
    if ($LASTEXITCODE -ne 0) {
        throw "real session loop multi-campaign eligible auto resolution case failed (exit code $LASTEXITCODE)."
    }

    $text = $output -join "`n"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_campaign_id=alpha_campaign_2230_01_01"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_campaign_id_source=auto_dsl_eligible_candidate_campaign_key"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_empire_id=player_country_0"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_empire_id_source=auto_dsl_eligible_player_country_id"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_decision_input_count=2"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_candidate_decision_count=2"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_dsl_draft_eligible_candidate_count=1"
    Assert-Contains -Name "real session loop multi-campaign auto resolution output" -Text $text -Expected "real_session_v0_loop_dsl_draft_skipped_candidate_count=1"

    $evidencePathLine = ($output | Where-Object { $_ -like "real_session_v0_loop_evidence_json=*" } | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($evidencePathLine)) {
        throw "real session loop multi-campaign eligible auto resolution case missing evidence path."
    }
    $evidencePath = $evidencePathLine.Substring("real_session_v0_loop_evidence_json=".Length)
    if (-not (Test-Path -LiteralPath $evidencePath)) {
        throw "real session loop multi-campaign eligible auto resolution evidence json missing: $evidencePath"
    }

    $evidenceJson = (Get-Content -Raw -LiteralPath $evidencePath) | ConvertFrom-Json
    if ([string]$evidenceJson.resolved_identity.campaign_id_source -ne "auto_dsl_eligible_candidate_campaign_key") {
        throw "real session loop multi-campaign eligible auto resolution expected campaign_id_source from DSL-eligible candidate."
    }
    if ([string]$evidenceJson.resolved_identity.empire_id_source -ne "auto_dsl_eligible_player_country_id") {
        throw "real session loop multi-campaign eligible auto resolution expected empire_id_source from DSL-eligible candidate."
    }
    if ([string]$evidenceJson.resolved_identity.campaign_id -ne "alpha_campaign_2230_01_01") {
        throw "real session loop multi-campaign eligible auto resolution expected alpha campaign resolved identity."
    }
    if ([string]$evidenceJson.resolved_identity.empire_id -ne "player_country_0") {
        throw "real session loop multi-campaign eligible auto resolution expected player_country_0 resolved identity."
    }

    Write-Host "[PASS] real_session_loop_multi_campaign_eligible_auto_resolution"
}

Invoke-V0PipelineCase `
    -Name "v0_defensive" `
    -InputFixture "resources/ministry_context_military_defensive.json" `
    -DecisionOutput "dist/v0_decision_defensive.json" `
    -SequenceId 101 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 101',
        '"military_posture": "defensive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=101"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_aggressive" `
    -InputFixture "resources/ministry_context_military_aggressive.json" `
    -DecisionOutput "dist/v0_decision_aggressive.json" `
    -SequenceId 102 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=aggressive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 102',
        '"military_posture": "aggressive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=102"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_aggressive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_preserve_agenda_low_confidence" `
    -InputFixture "resources/ministry_context_unknown_preserve_agenda.json" `
    -DecisionOutput "dist/v0_decision_preserve_agenda.json" `
    -SequenceId 103 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=aggressive",
        "v0_pipeline_research_bias=military_industry",
        "v0_pipeline_confidence=0"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 103',
        '"military_posture": "aggressive"',
        '"research_bias": "military_industry"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=103"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_aggressive",
        "effect set_global_flag = strategic_nexus_research_bias_military_industry",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_research_economy" `
    -InputFixture "resources/ministry_context_research_economy.json" `
    -DecisionOutput "dist/v0_decision_research_economy.json" `
    -SequenceId 106 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 106',
        '"military_posture": "defensive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=106"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_research_military_industry" `
    -InputFixture "resources/ministry_context_research_military_industry.json" `
    -DecisionOutput "dist/v0_decision_research_military_industry.json" `
    -SequenceId 107 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=military_industry"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 107',
        '"military_posture": "defensive"',
        '"research_bias": "military_industry"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=107"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_military_industry",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_missing_optional_fields" `
    -InputFixture "resources/ministry_context_missing_optional_fields.json" `
    -DecisionOutput "dist/v0_decision_missing_optional.json" `
    -SequenceId 104 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 104',
        '"military_posture": "defensive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=104"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_escaped_strings" `
    -InputFixture "resources/ministry_context_escaped_strings.json" `
    -DecisionOutput "dist/v0_decision_escaped_strings.json" `
    -SequenceId 109 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 109',
        '"military_posture": "defensive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedAuditJson @(
        'context_escaped_\"quote\"_001',
        'escaped \"quote\" fact',
        'line break fact',
        'bracket ] fact',
        'tab becomes space'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=109"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_untrusted_input_bounds" `
    -InputFixture "resources/ministry_context_untrusted_bounds.json" `
    -DecisionOutput "dist/v0_decision_untrusted_bounds.json" `
    -SequenceId 108 `
    -NowUnixMs 10000 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=true",
        "v0_pipeline_military_posture=defensive",
        "v0_pipeline_research_bias=economy"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 108',
        '"military_posture": "defensive"',
        '"research_bias": "economy"',
        '"fallback_required": false'
    ) `
    -ExpectedAuditJson @(
        "bounded_fact_16"
    ) `
    -UnexpectedAuditJson @(
        "bounded_fact_17"
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=true",
        "pipeline_reason=accepted",
        "pipeline_sequence_id=108"
    ) `
    -ExpectedEffectBatch @(
        "effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect set_global_flag = strategic_nexus_research_bias_economy",
        "event strategic_nexus.1400"
    )

Invoke-V0PipelineCase `
    -Name "v0_invalid_current_agenda_fallback" `
    -InputFixture "resources/ministry_context_invalid_current_agenda.json" `
    -DecisionOutput "dist/v0_decision_invalid_current_agenda.json" `
    -SequenceId 105 `
    -NowUnixMs 10000 `
    -ExpectedAppExitCode 1 `
    -ExpectedAppOutput @(
        "v0_pipeline_success=false",
        "v0_pipeline_military_posture=reckless",
        "v0_pipeline_research_bias=forbidden_research"
    ) `
    -ExpectedDecisionJson @(
        '"sequence_id": 105',
        '"military_posture": "reckless"',
        '"research_bias": "forbidden_research"',
        '"bridge_available": false',
        '"fallback_required": true'
    ) `
    -ExpectedBridgeOutput @(
        "pipeline_accepted=false",
        "pipeline_reason=bridge unavailable",
        "pipeline_sequence_id=105",
        "pipeline_effect_batch_written=false"
    ) `
    -ExpectNoEffectBatch

Invoke-V0PipelineReadFailureCase `
    -Name "v0_missing_input_context" `
    -InputFixture "resources/does_not_exist.json" `
    -ExpectedReason "missing ministry input context"

Invoke-V0PipelineReadFailureCase `
    -Name "v0_unsupported_schema" `
    -InputFixture "resources/ministry_context_unsupported_schema.json" `
    -ExpectedReason "unsupported schema_version"

Invoke-V0PipelineReadFailureCase `
    -Name "v0_malformed_input_context" `
    -InputFixture "resources/ministry_context_malformed.json" `
    -ExpectedReason "malformed ministry input context"

Invoke-V0PipelineOutputFailureCase `
    -Name "v0_decision_output_directory_rejected" `
    -InputFixture "resources/ministry_context_military_defensive.json" `
    -ExpectedReason "failed to write decision output"

Invoke-V0PipelineAuditFailureCase `
    -Name "v0_audit_output_directory_warns" `
    -InputFixture "resources/ministry_context_military_defensive.json"

Invoke-V0PriorityQueueCase
Invoke-GeneratedOverlayCompileCase
Invoke-GeneratedOverlayVerifyMismatchCase
Invoke-GeneratedOverlayInvalidCase
Invoke-CampaignSaveScanCase
Invoke-CampaignSaveDiffCase
Invoke-CampaignSaveRestoreContinuityCase
Invoke-CampaignLibraryPlanCase
Invoke-CampaignLibraryOverlayCase
Invoke-StellarisSaveRootDiscoveryCase
Invoke-StellarisProcessDetectionCase
Invoke-SncStatusSnapshotCase
Invoke-AutosaveArchiveCase
Invoke-AutosaveArchiveVerifyMismatchCase
Invoke-RealSessionTrendIdentityRiskPriorityCase
Invoke-RealSessionTrendHandoffContinuityPriorityCase
Invoke-RealSessionWarningCodeDriftSurfaceCase
Invoke-RealSessionLoopMpSnapshotContractCase
Invoke-RealSessionLoopNextActionStrictVerifySourceContractCase
Invoke-RealSessionLoopAutoResolutionCase
Invoke-RealSessionLoopMultiCampaignEligibleAutoResolutionCase
Invoke-RealSessionLoopMismatchForwardingCase
Invoke-AmbiguousPostPlayCliFailClosedCase
Write-Host "Running generated overlay gameplay acceptance cases..."
$gameplayAcceptanceOutput = & cmd /c (Join-Path $repoRoot "tools\\run_generated_overlay_gameplay_acceptance.cmd")
if ($LASTEXITCODE -ne 0) {
    $gameplayAcceptanceText = $gameplayAcceptanceOutput -join "`n"
    throw "generated overlay gameplay acceptance cases failed.`n$gameplayAcceptanceText"
}
$gameplayAcceptanceText = $gameplayAcceptanceOutput -join "`n"
Assert-Contains -Name "generated overlay gameplay acceptance output" -Text $gameplayAcceptanceText -Expected "gameplay_acceptance_success=true"
Assert-Contains -Name "generated overlay gameplay acceptance output" -Text $gameplayAcceptanceText -Expected "gameplay_acceptance_state=verified_for_v0_domains"
$gameplayAcceptanceReportPath = Join-Path $repoRoot "dist/private_reports/generated_overlay_gameplay_acceptance_v0.json"
if (-not (Test-Path -LiteralPath $gameplayAcceptanceReportPath)) {
    throw "generated overlay gameplay acceptance report is missing: $gameplayAcceptanceReportPath"
}
$gameplayAcceptanceReport = Get-Content -LiteralPath $gameplayAcceptanceReportPath -Raw | ConvertFrom-Json
if ($null -eq $gameplayAcceptanceReport) {
    throw "generated overlay gameplay acceptance report is empty."
}
if ([string]$gameplayAcceptanceReport.acceptance_state -ne "verified_for_v0_domains") {
    throw "generated overlay gameplay acceptance report has unexpected acceptance_state: $($gameplayAcceptanceReport.acceptance_state)"
}
if (@($gameplayAcceptanceReport.cases).Count -lt 6) {
    throw "generated overlay gameplay acceptance report is missing expected case coverage."
}

& $autosaveArchiverExePath
if ($LASTEXITCODE -ne 0) {
    throw "autosave archiver tests failed."
}

& $autosaveArchiveVerifierExePath
if ($LASTEXITCODE -ne 0) {
    throw "autosave archive verifier tests failed."
}

& $autosaveArchiveSummarizerExePath
if ($LASTEXITCODE -ne 0) {
    throw "autosave archive summarizer tests failed."
}

& $archiveMinistryInputBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "archive ministry input builder tests failed."
}

& $seasonDeltaLedgerBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "season delta ledger builder tests failed."
}

& $seasonEmpireBriefBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "season empire brief builder tests failed."
}

& $integratedEmpireStateBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "integrated empire state builder tests failed."
}

& $personalityProfileStoreExePath
if ($LASTEXITCODE -ne 0) {
    throw "personality profile store tests failed."
}

& $personalityEngineExePath
if ($LASTEXITCODE -ne 0) {
    throw "personality engine tests failed."
}

& $cabinetContractExePath
if ($LASTEXITCODE -ne 0) {
    throw "v0 cabinet contract tests failed."
}

& $priorityScoreExePath
if ($LASTEXITCODE -ne 0) {
    throw "v0 priority score tests failed."
}

& $processingQueueExePath
if ($LASTEXITCODE -ne 0) {
    throw "v0 processing queue tests failed."
}

& $ministryInputReaderExePath
if ($LASTEXITCODE -ne 0) {
    throw "v0 ministry input reader tests failed."
}

& $empireContextIsolationExePath
if ($LASTEXITCODE -ne 0) {
    throw "v0 empire context isolation tests failed."
}

& $generatedOverlayContractExePath
if ($LASTEXITCODE -ne 0) {
    throw "generated overlay contract tests failed."
}

& $generatedOverlayVerifierExePath
if ($LASTEXITCODE -ne 0) {
    throw "generated overlay verifier tests failed."
}

& $generatedOverlayPublisherExePath
if ($LASTEXITCODE -ne 0) {
    throw "generated overlay publisher tests failed."
}

& $mpOverlayPackageVerifierExePath
if ($LASTEXITCODE -ne 0) {
    throw "mp overlay package verifier tests failed."
}

& $campaignSaveScannerExePath
if ($LASTEXITCODE -ne 0) {
    throw "campaign save scanner tests failed."
}

& $campaignLibraryPlannerExePath
if ($LASTEXITCODE -ne 0) {
    throw "campaign library planner tests failed."
}

& $stellarisSavePathResolverExePath
if ($LASTEXITCODE -ne 0) {
    throw "stellaris save path resolver tests failed."
}

& $stellarisProcessDetectorExePath
if ($LASTEXITCODE -ne 0) {
    throw "stellaris process detector tests failed."
}

& $saveParserExePath
if ($LASTEXITCODE -ne 0) {
    throw "save parser tests failed."
}

& $saveEntryPointAnalyzerExePath
if ($LASTEXITCODE -ne 0) {
    throw "save entry point analyzer tests failed."
}

& $postPlayPackageBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "post-play package builder tests failed."
}

& $sncDecisionInputPackageBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC decision input package builder tests failed."
}

& $sncCandidateDecisionPackageBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC candidate decision package builder tests failed."
}

& $sncDslDraftPackageBuilderExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC DSL draft package builder tests failed."
}

& $sncGeneratedOverlayStagerExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC generated overlay stager tests failed."
}

& $sncGeneratedOverlayPublishGateExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC generated overlay publish gate tests failed."
}

& $sncPostPlayArtifactBackfillerExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC post-play artifact backfiller tests failed."
}

& $sncFriendPackageExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend package tests failed."
}

$sncFriendCliRoot = Join-Path $repoRoot "dist\test_snc_friend_cli"
if (Test-Path -LiteralPath $sncFriendCliRoot) {
    Remove-Item -LiteralPath $sncFriendCliRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $sncFriendCliRoot | Out-Null
$sncFriendRequestPath = Join-Path $sncFriendCliRoot "host.snc-friend-request.json"
$sncFriendAcceptancePath = Join-Path $sncFriendCliRoot "client.snc-friend-acceptance.json"
$sncFriendTrustStorePath = Join-Path $sncFriendCliRoot "snc_friend_trust_store.json"
$sncFriendSelfAcceptancePath = Join-Path $sncFriendCliRoot "self.snc-friend-acceptance.json"
$sncFriendMpSyncEnvelopePath = Join-Path $sncFriendCliRoot "host-client.snc-friend-mp-sync.json"
$sncFriendMpSyncEncryptedPayloadPath = Join-Path $sncFriendCliRoot "host-client.snc-friend-mp-sync.enc"
$sncFriendSelfMpSyncEnvelopePath = Join-Path $sncFriendCliRoot "self.snc-friend-mp-sync.json"
$sncFriendInvalidMpSyncEnvelopePath = Join-Path $sncFriendCliRoot "invalid.snc-friend-mp-sync.json"

$sncFriendRequestOutput = & $exePath `
    --create-snc-friend-request `
    $sncFriendRequestPath `
    "snc-node-host-cli-001" `
    "Host CLI SNC" `
    "ed25519:host-cli-signing-key" `
    "x25519:host-cli-encryption-key" `
    "fp-host-cli-001" `
    "2026-06-08T18:00:00Z" `
    "2026-06-09T18:00:00Z"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend request CLI failed. Actual output:`n$($sncFriendRequestOutput -join "`n")"
}
$sncFriendRequestText = $sncFriendRequestOutput -join "`n"
Assert-Contains -Name "snc friend request cli" -Text $sncFriendRequestText -Expected "snc_friend_request_success=true"
if (-not (Test-Path -LiteralPath $sncFriendRequestPath)) {
    throw "SNC friend request CLI did not write request package."
}

$sncFriendAcceptanceOutput = & $exePath `
    --create-snc-friend-acceptance `
    $sncFriendRequestPath `
    $sncFriendAcceptancePath `
    "snc-node-client-cli-001" `
    "Client CLI SNC" `
    "ed25519:client-cli-signing-key" `
    "x25519:client-cli-encryption-key" `
    "fp-client-cli-001" `
    "2026-06-08T18:05:00Z"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend acceptance CLI failed. Actual output:`n$($sncFriendAcceptanceOutput -join "`n")"
}
$sncFriendAcceptanceText = $sncFriendAcceptanceOutput -join "`n"
Assert-Contains -Name "snc friend acceptance cli" -Text $sncFriendAcceptanceText -Expected "snc_friend_acceptance_success=true"
if (-not (Test-Path -LiteralPath $sncFriendAcceptancePath)) {
    throw "SNC friend acceptance CLI did not write acceptance package."
}

$sncFriendImportOutput = & $exePath `
    --import-snc-friend-acceptance `
    $sncFriendRequestPath `
    $sncFriendAcceptancePath `
    $sncFriendTrustStorePath `
    "2026-06-08T18:06:00Z" `
    "Client CLI"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend acceptance import CLI failed. Actual output:`n$($sncFriendImportOutput -join "`n")"
}
$sncFriendImportText = $sncFriendImportOutput -join "`n"
Assert-Contains -Name "snc friend acceptance import cli" -Text $sncFriendImportText -Expected "snc_friend_acceptance_import_success=true"
Assert-Contains -Name "snc friend acceptance import cli" -Text $sncFriendImportText -Expected "snc_friend_acceptance_import_auto_sync_enabled=false"
$sncFriendTrustStoreJson = Get-Content -LiteralPath $sncFriendTrustStorePath -Raw
Assert-Contains -Name "snc friend trust store cli json" -Text $sncFriendTrustStoreJson -Expected '"trust_state": "trusted"'
Assert-Contains -Name "snc friend trust store cli json" -Text $sncFriendTrustStoreJson -Expected '"auto_sync_enabled": false'

$sncFriendTrustStoreUpdateOutput = & $exePath `
    --update-snc-friend-trust-store-entry `
    $sncFriendTrustStorePath `
    "snc-node-client-cli-001" `
    "trusted" `
    "true" `
    "2026-06-08T18:06:30Z" `
    "Client CLI"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend trust store update CLI failed. Actual output:`n$($sncFriendTrustStoreUpdateOutput -join "`n")"
}
$sncFriendTrustStoreUpdateText = $sncFriendTrustStoreUpdateOutput -join "`n"
Assert-Contains -Name "snc friend trust store update cli" -Text $sncFriendTrustStoreUpdateText -Expected "snc_friend_trust_store_update_success=true"
Assert-Contains -Name "snc friend trust store update cli" -Text $sncFriendTrustStoreUpdateText -Expected "snc_friend_trust_store_update_auto_sync_enabled=true"
$sncFriendTrustStoreUpdatedJson = Get-Content -LiteralPath $sncFriendTrustStorePath -Raw
Assert-Contains -Name "snc friend trust store updated cli json" -Text $sncFriendTrustStoreUpdatedJson -Expected '"auto_sync_enabled": true'

$sncFriendSelfAcceptanceOutput = & $exePath `
    --create-snc-friend-acceptance `
    $sncFriendRequestPath `
    $sncFriendSelfAcceptancePath `
    "snc-node-host-cli-001" `
    "Host CLI SNC" `
    "ed25519:host-cli-signing-key" `
    "x25519:host-cli-encryption-key" `
    "fp-host-cli-001" `
    "2026-06-08T18:07:00Z"
if ($LASTEXITCODE -eq 0) {
    throw "SNC friend self-acceptance CLI unexpectedly succeeded."
}
$sncFriendSelfAcceptanceText = $sncFriendSelfAcceptanceOutput -join "`n"
Assert-Contains -Name "snc friend self-acceptance cli" -Text $sncFriendSelfAcceptanceText -Expected "snc_friend_acceptance_success=false"
if (Test-Path -LiteralPath $sncFriendSelfAcceptancePath) {
    throw "SNC friend self-acceptance CLI wrote an unsafe acceptance package."
}

$sncFriendMpSyncEnvelopeOutput = & $exePath `
    --create-snc-friend-mp-sync-envelope `
    $sncFriendMpSyncEnvelopePath `
    "snc-node-host-cli-001" `
    "Host CLI SNC" `
    "ed25519:host-cli-signing-key" `
    "x25519:host-cli-encryption-key" `
    "fp-host-cli-001" `
    "snc-node-client-cli-001" `
    "Client CLI SNC" `
    "ed25519:client-cli-signing-key" `
    "x25519:client-cli-encryption-key" `
    "fp-client-cli-001" `
    "campaign-mp-cli-001" `
    "overlay-v0-cli-001" `
    "sha256:manifest-cli-001" `
    "sha256:zip-cli-001" `
    "sha256:encrypted-payload-cli-001" `
    "4096" `
    "ed25519" `
    "x25519-xsalsa20-poly1305" `
    "ed25519:signature-cli-001" `
    "2026-06-08T18:08:00Z"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync envelope CLI failed. Actual output:`n$($sncFriendMpSyncEnvelopeOutput -join "`n")"
}
$sncFriendMpSyncEnvelopeText = $sncFriendMpSyncEnvelopeOutput -join "`n"
Assert-Contains -Name "snc friend mp sync envelope cli" -Text $sncFriendMpSyncEnvelopeText -Expected "snc_friend_mp_sync_envelope_success=true"
Assert-Contains -Name "snc friend mp sync envelope cli" -Text $sncFriendMpSyncEnvelopeText -Expected "snc_friend_mp_sync_envelope_package_manifest_hash=sha256:manifest-cli-001"
if (-not (Test-Path -LiteralPath $sncFriendMpSyncEnvelopePath)) {
    throw "SNC friend MP sync envelope CLI did not write envelope package."
}
$sncFriendMpSyncEnvelopeJson = Get-Content -LiteralPath $sncFriendMpSyncEnvelopePath -Raw
Assert-Contains -Name "snc friend mp sync envelope cli json" -Text $sncFriendMpSyncEnvelopeJson -Expected '"package_type": "snc_friend_mp_package_sync"'
Assert-Contains -Name "snc friend mp sync envelope cli json" -Text $sncFriendMpSyncEnvelopeJson -Expected '"encrypted_payload_bytes": 4096'

$sncFriendMpSyncEnvelopeVerifyOutput = & $exePath `
    --verify-snc-friend-mp-sync-envelope `
    $sncFriendMpSyncEnvelopePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync envelope verify CLI failed. Actual output:`n$($sncFriendMpSyncEnvelopeVerifyOutput -join "`n")"
}
$sncFriendMpSyncEnvelopeVerifyText = $sncFriendMpSyncEnvelopeVerifyOutput -join "`n"
Assert-Contains -Name "snc friend mp sync envelope verify cli" -Text $sncFriendMpSyncEnvelopeVerifyText -Expected "snc_friend_mp_sync_envelope_verify_success=true"
Assert-Contains -Name "snc friend mp sync envelope verify cli" -Text $sncFriendMpSyncEnvelopeVerifyText -Expected "snc_friend_mp_sync_envelope_verify_encrypted_payload_bytes=4096"
Assert-Contains -Name "snc friend mp sync envelope verify cli" -Text $sncFriendMpSyncEnvelopeVerifyText -Expected "snc_friend_mp_sync_envelope_apply_allowed=false"
Assert-Contains -Name "snc friend mp sync envelope verify cli" -Text $sncFriendMpSyncEnvelopeVerifyText -Expected "snc_friend_mp_sync_envelope_apply_gate_state=manual_metadata_only"

$sncFriendMpSyncEnvelopeRunningVerifyOutput = & $exePath `
    --verify-snc-friend-mp-sync-envelope `
    $sncFriendMpSyncEnvelopePath `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync envelope running-game verify CLI failed. Actual output:`n$($sncFriendMpSyncEnvelopeRunningVerifyOutput -join "`n")"
}
$sncFriendMpSyncEnvelopeRunningVerifyText = $sncFriendMpSyncEnvelopeRunningVerifyOutput -join "`n"
Assert-Contains -Name "snc friend mp sync envelope running verify cli" -Text $sncFriendMpSyncEnvelopeRunningVerifyText -Expected "snc_friend_mp_sync_envelope_verify_success=true"
Assert-Contains -Name "snc friend mp sync envelope running verify cli" -Text $sncFriendMpSyncEnvelopeRunningVerifyText -Expected "snc_friend_mp_sync_envelope_apply_allowed=false"
Assert-Contains -Name "snc friend mp sync envelope running verify cli" -Text $sncFriendMpSyncEnvelopeRunningVerifyText -Expected "snc_friend_mp_sync_envelope_apply_gate_state=blocked_stellaris_running"

$sncFriendMissingPayloadInboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-inbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync inbox missing-payload plan CLI failed. Actual output:`n$($sncFriendMissingPayloadInboxPlanOutput -join "`n")"
}
$sncFriendMissingPayloadInboxPlanText = $sncFriendMissingPayloadInboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync inbox missing payload cli" -Text $sncFriendMissingPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_success=true"
Assert-Contains -Name "snc friend mp sync inbox missing payload cli" -Text $sncFriendMissingPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_state=waiting_for_encrypted_payload"
Assert-Contains -Name "snc friend mp sync inbox missing payload cli" -Text $sncFriendMissingPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_package_staging_allowed=false"

Set-Content -LiteralPath $sncFriendMpSyncEncryptedPayloadPath -Value "encrypted-payload-fixture" -NoNewline

$sncFriendManualInboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-inbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "false"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync inbox manual-review plan CLI failed. Actual output:`n$($sncFriendManualInboxPlanOutput -join "`n")"
}
$sncFriendManualInboxPlanText = $sncFriendManualInboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync inbox manual review cli" -Text $sncFriendManualInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_state=waiting_for_owner_approval"
Assert-Contains -Name "snc friend mp sync inbox manual review cli" -Text $sncFriendManualInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_automatic_download_enabled=false"
Assert-Contains -Name "snc friend mp sync inbox manual review cli" -Text $sncFriendManualInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_package_staging_allowed=false"

$sncFriendPayloadInboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-inbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync inbox payload-present plan CLI failed. Actual output:`n$($sncFriendPayloadInboxPlanOutput -join "`n")"
}
$sncFriendPayloadInboxPlanText = $sncFriendPayloadInboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync inbox payload present cli" -Text $sncFriendPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_state=metadata_verified_transport_not_implemented"
Assert-Contains -Name "snc friend mp sync inbox payload present cli" -Text $sncFriendPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_encrypted_payload_present=true"
Assert-Contains -Name "snc friend mp sync inbox payload present cli" -Text $sncFriendPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_automatic_download_enabled=false"
Assert-Contains -Name "snc friend mp sync inbox payload present cli" -Text $sncFriendPayloadInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_package_staging_allowed=false"

$sncFriendRunningInboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-inbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "true" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync inbox running-game plan CLI failed. Actual output:`n$($sncFriendRunningInboxPlanOutput -join "`n")"
}
$sncFriendRunningInboxPlanText = $sncFriendRunningInboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync inbox running cli" -Text $sncFriendRunningInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_state=blocked_stellaris_running"
Assert-Contains -Name "snc friend mp sync inbox running cli" -Text $sncFriendRunningInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_package_staging_allowed=false"

$sncFriendMissingPayloadOutboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-outbox `
    $sncFriendMpSyncEnvelopePath `
    (Join-Path $sncFriendCliRoot "missing-outbox-payload.enc") `
    "false" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync outbox missing-payload plan CLI failed. Actual output:`n$($sncFriendMissingPayloadOutboxPlanOutput -join "`n")"
}
$sncFriendMissingPayloadOutboxPlanText = $sncFriendMissingPayloadOutboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync outbox missing payload cli" -Text $sncFriendMissingPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_success=true"
Assert-Contains -Name "snc friend mp sync outbox missing payload cli" -Text $sncFriendMissingPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_state=waiting_for_encrypted_payload"
Assert-Contains -Name "snc friend mp sync outbox missing payload cli" -Text $sncFriendMissingPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_send_allowed=false"

$sncFriendManualOutboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-outbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "false"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync outbox manual-review plan CLI failed. Actual output:`n$($sncFriendManualOutboxPlanOutput -join "`n")"
}
$sncFriendManualOutboxPlanText = $sncFriendManualOutboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync outbox manual review cli" -Text $sncFriendManualOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_state=waiting_for_owner_approval"
Assert-Contains -Name "snc friend mp sync outbox manual review cli" -Text $sncFriendManualOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_transport_enabled=false"
Assert-Contains -Name "snc friend mp sync outbox manual review cli" -Text $sncFriendManualOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_send_allowed=false"

$sncFriendPayloadOutboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-outbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync outbox payload-present plan CLI failed. Actual output:`n$($sncFriendPayloadOutboxPlanOutput -join "`n")"
}
$sncFriendPayloadOutboxPlanText = $sncFriendPayloadOutboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync outbox payload present cli" -Text $sncFriendPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_state=metadata_verified_transport_not_implemented"
Assert-Contains -Name "snc friend mp sync outbox payload present cli" -Text $sncFriendPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_encrypted_payload_present=true"
Assert-Contains -Name "snc friend mp sync outbox payload present cli" -Text $sncFriendPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_transport_enabled=false"
Assert-Contains -Name "snc friend mp sync outbox payload present cli" -Text $sncFriendPayloadOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_send_allowed=false"

$sncFriendRunningOutboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-outbox `
    $sncFriendMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "true" `
    "true"
if ($LASTEXITCODE -ne 0) {
    throw "SNC friend MP sync outbox running-game plan CLI failed. Actual output:`n$($sncFriendRunningOutboxPlanOutput -join "`n")"
}
$sncFriendRunningOutboxPlanText = $sncFriendRunningOutboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync outbox running cli" -Text $sncFriendRunningOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_state=blocked_stellaris_running"
Assert-Contains -Name "snc friend mp sync outbox running cli" -Text $sncFriendRunningOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_send_allowed=false"

$sncFriendSelfMpSyncEnvelopeOutput = & $exePath `
    --create-snc-friend-mp-sync-envelope `
    $sncFriendSelfMpSyncEnvelopePath `
    "snc-node-host-cli-001" `
    "Host CLI SNC" `
    "ed25519:host-cli-signing-key" `
    "x25519:host-cli-encryption-key" `
    "fp-host-cli-001" `
    "snc-node-host-cli-001" `
    "Host CLI SNC" `
    "ed25519:host-cli-signing-key" `
    "x25519:host-cli-encryption-key" `
    "fp-host-cli-001" `
    "campaign-mp-cli-001" `
    "overlay-v0-cli-001" `
    "sha256:manifest-cli-001" `
    "sha256:zip-cli-001" `
    "sha256:encrypted-payload-cli-001" `
    "4096" `
    "ed25519" `
    "x25519-xsalsa20-poly1305" `
    "ed25519:signature-cli-001" `
    "2026-06-08T18:09:00Z"
if ($LASTEXITCODE -eq 0) {
    throw "SNC friend self-recipient MP sync envelope CLI unexpectedly succeeded."
}
$sncFriendSelfMpSyncEnvelopeText = $sncFriendSelfMpSyncEnvelopeOutput -join "`n"
Assert-Contains -Name "snc friend self-recipient mp sync envelope cli" -Text $sncFriendSelfMpSyncEnvelopeText -Expected "snc_friend_mp_sync_envelope_success=false"
if (Test-Path -LiteralPath $sncFriendSelfMpSyncEnvelopePath) {
    throw "SNC friend self-recipient MP sync envelope CLI wrote an unsafe envelope package."
}

$sncFriendInvalidMpSyncEnvelopeJson = $sncFriendMpSyncEnvelopeJson.Replace('"schema_version": 1', '"schema_version": 2')
Set-Content -LiteralPath $sncFriendInvalidMpSyncEnvelopePath -Value $sncFriendInvalidMpSyncEnvelopeJson -NoNewline
$sncFriendInvalidInboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-inbox `
    $sncFriendInvalidMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "true"
if ($LASTEXITCODE -eq 0) {
    throw "SNC friend MP sync inbox invalid-envelope plan CLI unexpectedly succeeded."
}
$sncFriendInvalidInboxPlanText = $sncFriendInvalidInboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync inbox invalid cli" -Text $sncFriendInvalidInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_success=false"
Assert-Contains -Name "snc friend mp sync inbox invalid cli" -Text $sncFriendInvalidInboxPlanText -Expected "snc_friend_mp_sync_inbox_plan_state=invalid_envelope"

$sncFriendInvalidOutboxPlanOutput = & $exePath `
    --plan-snc-friend-mp-sync-outbox `
    $sncFriendInvalidMpSyncEnvelopePath `
    $sncFriendMpSyncEncryptedPayloadPath `
    "false" `
    "true"
if ($LASTEXITCODE -eq 0) {
    throw "SNC friend MP sync outbox invalid-envelope plan CLI unexpectedly succeeded."
}
$sncFriendInvalidOutboxPlanText = $sncFriendInvalidOutboxPlanOutput -join "`n"
Assert-Contains -Name "snc friend mp sync outbox invalid cli" -Text $sncFriendInvalidOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_success=false"
Assert-Contains -Name "snc friend mp sync outbox invalid cli" -Text $sncFriendInvalidOutboxPlanText -Expected "snc_friend_mp_sync_outbox_plan_state=invalid_envelope"

& $sncTrayStartupShortcutActionExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC tray startup shortcut action tests failed."
}

& $sncTraySupportReportActionExePath
if ($LASTEXITCODE -ne 0) {
    throw "SNC tray support report action tests failed."
}

& $strategicNexusCompanionExePath
if ($LASTEXITCODE -ne 0) {
    throw "strategic nexus companion tests failed."
}

$existingTrayProcess = Get-Process -Name "StrategicNexusCompanionTray" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -ne $existingTrayProcess) {
    Write-Host "[SKIP] snc_tray_smoke_existing_owner_instance"
} else {
    $sncTrayBackfillSmokeOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\smoke_snc_tray.ps1") -PostPlayBackfillFixture
    if ($LASTEXITCODE -ne 0) {
        throw "SNC tray post-play backfill smoke failed."
    }
    $sncTrayBackfillSmokeText = $sncTrayBackfillSmokeOutput -join "`n"
    Assert-Contains -Name "snc tray backfill smoke output" -Text $sncTrayBackfillSmokeText -Expected "snc_tray_smoke_success=true"
    Assert-Contains -Name "snc tray backfill smoke output" -Text $sncTrayBackfillSmokeText -Expected "snc_tray_smoke_next_action=review_staged_overlay_and_publish_if_desired"

    $sncTrayReadyOwnerSmokeOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\smoke_snc_tray.ps1") -ReadyOwnerTestFixture
    if ($LASTEXITCODE -ne 0) {
        throw "SNC tray ready-owner-test smoke failed."
    }
    $sncTrayReadyOwnerSmokeText = $sncTrayReadyOwnerSmokeOutput -join "`n"
    Assert-Contains -Name "snc tray ready owner smoke output" -Text $sncTrayReadyOwnerSmokeText -Expected "snc_tray_smoke_success=true"
    Assert-Contains -Name "snc tray ready owner smoke output" -Text $sncTrayReadyOwnerSmokeText -Expected "snc_tray_smoke_next_action=run_monthly_reactive_owner_test"
    Assert-Contains -Name "snc tray ready owner smoke output" -Text $sncTrayReadyOwnerSmokeText -Expected "snc_tray_smoke_owner_test_playbook_path=docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md"

    $sncTrayMemoryRecoverySmokeOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\smoke_snc_tray.ps1") -MemoryRecoveryFixture
    if ($LASTEXITCODE -ne 0) {
        throw "SNC tray memory-recovery smoke failed."
    }
    $sncTrayMemoryRecoverySmokeText = $sncTrayMemoryRecoverySmokeOutput -join "`n"
    Assert-Contains -Name "snc tray memory recovery smoke output" -Text $sncTrayMemoryRecoverySmokeText -Expected "snc_tray_smoke_success=true"
    Assert-Contains -Name "snc tray memory recovery smoke output" -Text $sncTrayMemoryRecoverySmokeText -Expected "snc_tray_smoke_next_action=review_entry_point_ambiguity"
}

Write-Host "V0 strategic pipeline tests passed."
