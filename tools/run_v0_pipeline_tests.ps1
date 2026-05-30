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
$strategicNexusCompanionExePath = Join-Path $repoRoot "dist/strategic_nexus_companion_test.exe"
$strategicNexusCompanionSourceFiles = @(
    (Join-Path $repoRoot "tests/strategic_nexus_companion_test.cpp"),
    (Join-Path $repoRoot "src/StrategicNexusCompanion.cpp"),
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

    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $SourceFiles /Fe:$OutputPath
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
    Invoke-ClCompile -Name "v0_cabinet_contract_test" -SourceFiles $cabinetContractSourceFiles -OutputPath $cabinetContractExePath
    Invoke-ClCompile -Name "v0_priority_score_test" -SourceFiles $priorityScoreSourceFiles -OutputPath $priorityScoreExePath
    Invoke-ClCompile -Name "v0_processing_queue_test" -SourceFiles $processingQueueSourceFiles -OutputPath $processingQueueExePath
    Invoke-ClCompile -Name "v0_ministry_input_reader_test" -SourceFiles $ministryInputReaderSourceFiles -OutputPath $ministryInputReaderExePath
    Invoke-ClCompile -Name "generated_overlay_contract_test" -SourceFiles $generatedOverlayContractSourceFiles -OutputPath $generatedOverlayContractExePath
    Invoke-ClCompile -Name "generated_overlay_verifier_test" -SourceFiles $generatedOverlayVerifierSourceFiles -OutputPath $generatedOverlayVerifierExePath
    Invoke-ClCompile -Name "generated_overlay_publisher_test" -SourceFiles $generatedOverlayPublisherSourceFiles -OutputPath $generatedOverlayPublisherExePath
    Invoke-ClCompile -Name "mp_overlay_package_verifier_test" -SourceFiles $mpOverlayPackageVerifierSourceFiles -OutputPath $mpOverlayPackageVerifierExePath
    Invoke-ClCompile -Name "campaign_save_scanner_test" -SourceFiles $campaignSaveScannerSourceFiles -OutputPath $campaignSaveScannerExePath
    Invoke-ClCompile -Name "campaign_library_planner_test" -SourceFiles $campaignLibraryPlannerSourceFiles -OutputPath $campaignLibraryPlannerExePath
    Invoke-ClCompile -Name "stellaris_save_path_resolver_test" -SourceFiles $stellarisSavePathResolverSourceFiles -OutputPath $stellarisSavePathResolverExePath
    Invoke-ClCompile -Name "stellaris_process_detector_test" -SourceFiles $stellarisProcessDetectorSourceFiles -OutputPath $stellarisProcessDetectorExePath
    Invoke-ClCompile -Name "save_parser_test" -SourceFiles $saveParserSourceFiles -OutputPath $saveParserExePath
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
    $null = $manifestText | ConvertFrom-Json

    Assert-Contains -Name "generated_overlay_compile events" -Text $eventsText -Expected "strategic_nexus_generated_effect_campaign_001_empire_001_border_war_defense = yes"
    Assert-Contains -Name "generated_overlay_compile effects" -Text $effectsText -Expected "set_country_flag = strategic_nexus_pref_military_posture_defensive"
    Assert-Contains -Name "generated_overlay_compile triggers" -Text $triggersText -Expected "has_global_flag = strategic_nexus_campaign_campaign_001"
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"snapshot_kind": "complete_replacement"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"multiplayer_requirement": "byte_identical_gameplay_affecting_files"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"path": "common/scripted_effects/strategic_nexus_generated_effects.txt"'
    Assert-Contains -Name "generated_overlay_compile manifest" -Text $manifestText -Expected '"hash_algorithm": "fnv1a64"'

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
    New-Item -ItemType Directory -Force -Path (Join-Path $currentRoot "Alpha Campaign") | Out-Null
    Set-Content -LiteralPath (Join-Path $previousRoot "Alpha Campaign/autosave_2230.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Alpha Campaign/autosave_2230.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Alpha Campaign/autosave_2231.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $previousRoot "Removed.sav") -Value "fixture" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $currentRoot "Added.sav") -Value "fixture" -Encoding UTF8

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
    Assert-Contains -Name "campaign_save_diff app" -Text $diffText -Expected "save_campaign_diff_changed=1"

    $inventoryDiffText = Get-Content -Raw -LiteralPath $diffPath
    $null = $inventoryDiffText | ConvertFrom-Json
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "added"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "removed"'
    Assert-Contains -Name "campaign_save_diff json" -Text $inventoryDiffText -Expected '"change_kind": "changed"'

    Write-Host "[PASS] campaign_save_diff"
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
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_included=1"
    Assert-Contains -Name "campaign_library_plan app" -Text $planText -Expected "campaign_library_plan_skipped=1"

    $planJson = Get-Content -Raw -LiteralPath $planPath
    $null = $planJson | ConvertFrom-Json
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"status": "included"'
    Assert-Contains -Name "campaign_library_plan json" -Text $planJson -Expected '"reason": "active_library_limit"'

    Write-Host "[PASS] campaign_library_plan"
}

function Invoke-CampaignLibraryOverlayCase {
    $saveRoot = Join-Path $repoRoot "dist/campaign_library_overlay_saves"
    $dslPath = Join-Path $repoRoot "dist/campaign_library_overlay.dsl"
    $overlayOutputPath = Join-Path $repoRoot "dist/campaign_library_overlay"
    Remove-Item -LiteralPath $saveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $dslPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $overlayOutputPath -Recurse -Force -ErrorAction SilentlyContinue

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

    $eventsText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "events/strategic_nexus_generated_events.txt")
    $planText = Get-Content -Raw -LiteralPath (Join-Path $overlayOutputPath "strategic_nexus_campaign_library_plan.json")
    $null = $planText | ConvertFrom-Json
    Assert-Contains -Name "campaign_library_overlay events" -Text $eventsText -Expected "strategic_nexus_generated_effect_alpha_empire_001_local_defense"
    Assert-NotContains -Name "campaign_library_overlay events" -Text $eventsText -Unexpected "missing_aggression"
    Assert-Contains -Name "campaign_library_overlay plan" -Text $planText -Expected '"campaign_key": "alpha"'

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
    $outputPath = Join-Path $repoRoot "dist/snc_status_snapshot.json"
    Remove-Item -LiteralPath $archiveRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $overlayRoot -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue

    New-Item -ItemType Directory -Force -Path $archiveRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $overlayRoot | Out-Null

    $compileOutput = & $exePath `
        --compile-generated-overlay `
        (Join-Path $repoRoot "resources/generated_overlay_valid.dsl") `
        $overlayRoot
    if ($LASTEXITCODE -ne 0) {
        $compileText = $compileOutput -join "`n"
        throw "snc_status_snapshot overlay compile failed. Actual output:`n$compileText"
    }

    $sncOutput = & $exePath `
        --snc-status-snapshot `
        $archiveRoot `
        $overlayRoot `
        $outputPath `
        true `
        false
    $sncExitCode = $LASTEXITCODE
    $sncText = $sncOutput -join "`n"

    if ($sncExitCode -ne 0) {
        throw "snc_status_snapshot app failed. Actual output:`n$sncText"
    }

    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_success=true"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_app_name=Strategic Nexus Companion"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_abbreviation=SNC"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_archive_state=starting"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_state=ready"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_manifest_hash="
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_publish_gate_state=ready"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_generated_overlay_publish_gate_reason=Stellaris is not running; generated overlay publish allowed"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_center_state=starting"
    Assert-Contains -Name "snc_status_snapshot app" -Text $sncText -Expected "snc_status_output_written=true"

    $snapshotJson = Get-Content -Raw -LiteralPath $outputPath
    $null = $snapshotJson | ConvertFrom-Json
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"app_name": "Strategic Nexus Companion"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"abbreviation": "SNC"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"window_close_behavior": "minimize_to_tray"'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"manifest_hash": "'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"generated_overlay_publish_gate_status":'
    Assert-Contains -Name "snc_status_snapshot json" -Text $snapshotJson -Expected '"status_center":'

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
Invoke-CampaignLibraryPlanCase
Invoke-CampaignLibraryOverlayCase
Invoke-StellarisSaveRootDiscoveryCase
Invoke-StellarisProcessDetectionCase
Invoke-SncStatusSnapshotCase
Invoke-AutosaveArchiveCase
Invoke-AutosaveArchiveVerifyMismatchCase

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

& $strategicNexusCompanionExePath
if ($LASTEXITCODE -ne 0) {
    throw "strategic nexus companion tests failed."
}

Write-Host "V0 strategic pipeline tests passed."
