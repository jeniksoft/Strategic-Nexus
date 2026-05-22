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
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/strategic_pipeline") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

$cabinetContractExePath = Join-Path $repoRoot "dist/v0_cabinet_contract_test.exe"
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

Push-Location $repoRoot
try {
    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $sourceFiles /Fe:$exePath
    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $cabinetContractSourceFiles /Fe:$cabinetContractExePath
    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $priorityScoreSourceFiles /Fe:$priorityScoreExePath
    & cl.exe /nologo /MP /std:c++20 /EHsc /I "src" $processingQueueSourceFiles /Fe:$processingQueueExePath
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

Write-Host "V0 strategic pipeline tests passed."
