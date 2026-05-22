$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist/bridge_core_test.exe"

& (Join-Path $PSScriptRoot "build_bridge_core.ps1")

function Invoke-BridgeCoreCase {
    param(
        [string]$Name,
        [string]$DecisionFile,
        [string[]]$Expected,
        [int64]$NowUnixMs = 20000,
        [int64]$LastSequenceId = 0,
        [string]$EffectBatchOutput = ""
    )

    $decisionPath = Join-Path $repoRoot $DecisionFile
    if ($EffectBatchOutput) {
        $effectBatchPath = Join-Path $repoRoot $EffectBatchOutput
        $output = & $exePath $decisionPath $NowUnixMs $LastSequenceId $effectBatchPath
    } else {
        $output = & $exePath $decisionPath $NowUnixMs $LastSequenceId
    }
    $text = $output -join "`n"

    foreach ($expectedLine in $Expected) {
        if ($text -notmatch [regex]::Escape($expectedLine)) {
            throw "Bridge core test '$Name' failed. Missing expected output '$expectedLine'. Actual output:`n$text"
        }
    }

    Write-Host "[PASS] $Name"
}

$validEffectBatchPath = "dist/test_effect_batch_valid.txt"
$staleEffectBatchPath = "dist/test_effect_batch_stale.txt"

Invoke-BridgeCoreCase `
    -Name "receiver dimensions map to script intent" `
    -DecisionFile "resources/decision_valid_receiver_dimensions.json" `
    -EffectBatchOutput $validEffectBatchPath `
    -Expected @(
        "valid=true",
        "action=ApplyStrategyDimensions",
        "script_event=strategic_nexus.1400",
        "mapped_dimensions=2",
        "script_global_flag=strategic_nexus_military_posture_defensive",
        "script_global_flag=strategic_nexus_research_bias_economy",
        "effect_batch_valid=true",
        "effect_batch_line=effect set_global_flag = strategic_nexus_bridge_available",
        "effect_batch_line=effect set_global_flag = strategic_nexus_military_posture_defensive",
        "effect_batch_line=effect set_global_flag = strategic_nexus_research_bias_economy",
        "effect_batch_line=effect set_country_flag = strategic_nexus_bridge_available",
        "effect_batch_line=effect set_country_flag = strategic_nexus_military_posture_defensive",
        "effect_batch_line=effect set_country_flag = strategic_nexus_research_bias_economy",
        "effect_batch_line=event strategic_nexus.1400",
        "effect_batch_written=true",
        "effect_batch_write_reason=written"
    )

$validEffectBatchFullPath = Join-Path $repoRoot $validEffectBatchPath
$validEffectBatchText = Get-Content -Raw -LiteralPath $validEffectBatchFullPath
foreach ($expectedLine in @(
    "effect set_global_flag = strategic_nexus_bridge_available",
    "effect set_global_flag = strategic_nexus_military_posture_defensive",
    "effect set_global_flag = strategic_nexus_research_bias_economy",
    "effect set_country_flag = strategic_nexus_bridge_available",
    "effect set_country_flag = strategic_nexus_military_posture_defensive",
    "effect set_country_flag = strategic_nexus_research_bias_economy",
    "event strategic_nexus.1400"
)) {
    if ($validEffectBatchText -notmatch [regex]::Escape($expectedLine)) {
        throw "Valid effect batch file missing expected line '$expectedLine'. Actual output:`n$validEffectBatchText"
    }
}
Write-Host "[PASS] valid effect batch file written"

Invoke-BridgeCoreCase `
    -Name "legacy doctrine id rejected" `
    -DecisionFile "resources/decision_legacy_doctrine_id_rejected.json" `
    -Expected @(
        "valid=false",
        "reason=forbidden executable, raw text, or legacy doctrine field",
        "action=UseFallback"
    )

Invoke-BridgeCoreCase `
    -Name "malformed complete payload rejected" `
    -DecisionFile "resources/decision_malformed_complete.json" `
    -Expected @(
        "valid=false",
        "reason=malformed file",
        "action=UseFallback"
    )

Invoke-BridgeCoreCase `
    -Name "missing identity rejected" `
    -DecisionFile "resources/decision_missing_identity.json" `
    -Expected @(
        "valid=false",
        "reason=missing campaign_id",
        "action=UseFallback"
    )

Invoke-BridgeCoreCase `
    -Name "invalid oversized number rejected" `
    -DecisionFile "resources/decision_invalid_number.json" `
    -Expected @(
        "valid=false",
        "reason=missing sequence_id",
        "action=UseFallback"
    )

Invoke-BridgeCoreCase `
    -Name "replayed sequence rejected" `
    -DecisionFile "resources/decision_valid_receiver_dimensions.json" `
    -LastSequenceId 20 `
    -Expected @(
        "valid=false",
        "reason=replayed or old sequence_id",
        "action=UseFallback"
    )

Invoke-BridgeCoreCase `
    -Name "fallback required rejected" `
    -DecisionFile "resources/decision_fallback_required.json" `
    -Expected @(
        "valid=false",
        "reason=fallback required by payload",
        "action=UseFallback"
    )

$staleEffectBatchFullPath = Join-Path $repoRoot $staleEffectBatchPath
Set-Content -LiteralPath $staleEffectBatchFullPath -Value "stale unsafe line"
Invoke-BridgeCoreCase `
    -Name "invalid payload clears stale effect batch file" `
    -DecisionFile "resources/decision_fallback_required.json" `
    -EffectBatchOutput $staleEffectBatchPath `
    -Expected @(
        "valid=false",
        "action=UseFallback",
        "effect_batch_valid=false",
        "effect_batch_written=false",
        "effect_batch_write_reason=invalid batch, output cleared"
    )

if (Test-Path -LiteralPath $staleEffectBatchFullPath) {
    throw "Invalid effect batch did not clear stale output file '$staleEffectBatchFullPath'."
}
Write-Host "[PASS] invalid effect batch clears stale file"

$effectBatchDirectoryPath = Join-Path $repoRoot "dist/effect_batch_output_dir"
if (-not (Test-Path -LiteralPath $effectBatchDirectoryPath)) {
    New-Item -ItemType Directory -Force -Path $effectBatchDirectoryPath | Out-Null
}
Invoke-BridgeCoreCase `
    -Name "invalid effect batch preserves output directory" `
    -DecisionFile "resources/decision_fallback_required.json" `
    -EffectBatchOutput "dist/effect_batch_output_dir" `
    -Expected @(
        "valid=false",
        "action=UseFallback",
        "effect_batch_valid=false",
        "effect_batch_written=false",
        "effect_batch_write_reason=invalid batch, output path is directory"
    )

if (-not (Test-Path -LiteralPath $effectBatchDirectoryPath -PathType Container)) {
    throw "Invalid effect batch removed output directory '$effectBatchDirectoryPath'."
}

$validEffectBatchDirectoryPath = Join-Path $repoRoot "dist/valid_effect_batch_output_dir"
if (-not (Test-Path -LiteralPath $validEffectBatchDirectoryPath)) {
    New-Item -ItemType Directory -Force -Path $validEffectBatchDirectoryPath | Out-Null
}
Invoke-BridgeCoreCase `
    -Name "valid effect batch rejects output directory" `
    -DecisionFile "resources/decision_valid_receiver_dimensions.json" `
    -EffectBatchOutput "dist/valid_effect_batch_output_dir" `
    -Expected @(
        "valid=true",
        "action=ApplyStrategyDimensions",
        "effect_batch_valid=true",
        "effect_batch_written=false",
        "effect_batch_write_reason=failed to write output atomically"
    )

if (-not (Test-Path -LiteralPath $validEffectBatchDirectoryPath -PathType Container)) {
    throw "Valid effect batch removed output directory '$validEffectBatchDirectoryPath'."
}

$pipelineEffectBatchDirectoryPath = Join-Path $repoRoot "dist/pipeline_effect_batch_output_dir"
$pipelineEffectDirectorySequencePath = Join-Path $repoRoot "dist/pipeline_effect_directory_sequence.txt"
Remove-Item -LiteralPath $pipelineEffectDirectorySequencePath -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $pipelineEffectBatchDirectoryPath)) {
    New-Item -ItemType Directory -Force -Path $pipelineEffectBatchDirectoryPath | Out-Null
}

$pipelineEffectDirectoryOutput = & $exePath `
    --pipeline `
    (Join-Path $repoRoot "resources/decision_valid_receiver_dimensions.json") `
    $pipelineEffectBatchDirectoryPath `
    $pipelineEffectDirectorySequencePath `
    20000
$pipelineEffectDirectoryText = $pipelineEffectDirectoryOutput -join "`n"

foreach ($expectedLine in @(
    "pipeline_accepted=false",
    "pipeline_reason=failed to write output atomically",
    "pipeline_sequence_id=20",
    "pipeline_previous_sequence_id=0",
    "pipeline_effect_batch_written=false",
    "pipeline_sequence_advanced=false"
)) {
    if ($pipelineEffectDirectoryText -notmatch [regex]::Escape($expectedLine)) {
        throw "Pipeline effect batch write failure test missing expected output '$expectedLine'. Actual output:`n$pipelineEffectDirectoryText"
    }
}

if (Test-Path -LiteralPath $pipelineEffectDirectorySequencePath) {
    throw "Pipeline effect batch write failure advanced sequence state '$pipelineEffectDirectorySequencePath'."
}
if (-not (Test-Path -LiteralPath $pipelineEffectBatchDirectoryPath -PathType Container)) {
    throw "Pipeline effect batch write failure removed output directory '$pipelineEffectBatchDirectoryPath'."
}
Write-Host "[PASS] pipeline rejects effect batch directory without sequence advance"

$pipelineEffectBatchPath = Join-Path $repoRoot "dist/pipeline_effect_batch.txt"
$pipelineSequencePath = Join-Path $repoRoot "dist/pipeline_sequence_state.txt"
Remove-Item -LiteralPath $pipelineEffectBatchPath -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $pipelineSequencePath -ErrorAction SilentlyContinue

$pipelineOutput = & $exePath `
    --pipeline `
    (Join-Path $repoRoot "resources/decision_valid_receiver_dimensions.json") `
    $pipelineEffectBatchPath `
    $pipelineSequencePath `
    20000
$pipelineText = $pipelineOutput -join "`n"

foreach ($expectedLine in @(
    "pipeline_accepted=true",
    "pipeline_reason=accepted",
    "pipeline_sequence_id=20",
    "pipeline_previous_sequence_id=0",
    "pipeline_current_sequence_id=20",
    "pipeline_effect_batch_written=true",
    "pipeline_sequence_advanced=true"
)) {
    if ($pipelineText -notmatch [regex]::Escape($expectedLine)) {
        throw "Pipeline valid test missing expected output '$expectedLine'. Actual output:`n$pipelineText"
    }
}

$pipelineSequenceText = Get-Content -Raw -LiteralPath $pipelineSequencePath
if ($pipelineSequenceText.Trim() -ne "20") {
    throw "Pipeline sequence state expected '20', got '$pipelineSequenceText'."
}
Write-Host "[PASS] pipeline accepts valid payload and advances sequence"

$pipelineInvalidReadEffectBatchPath = Join-Path $repoRoot "dist/pipeline_invalid_read_effect_batch.txt"
$pipelineInvalidReadSequencePath = Join-Path $repoRoot "dist/pipeline_invalid_read_sequence.txt"
Remove-Item -LiteralPath $pipelineInvalidReadSequencePath -ErrorAction SilentlyContinue
Set-Content -LiteralPath $pipelineInvalidReadEffectBatchPath -Value "stale unsafe pipeline line"

$pipelineInvalidReadOutput = & $exePath `
    --pipeline `
    (Join-Path $repoRoot "resources/decision_malformed_complete.json") `
    $pipelineInvalidReadEffectBatchPath `
    $pipelineInvalidReadSequencePath `
    20000
$pipelineInvalidReadText = $pipelineInvalidReadOutput -join "`n"

foreach ($expectedLine in @(
    "pipeline_accepted=false",
    "pipeline_reason=malformed file",
    "pipeline_sequence_id=0",
    "pipeline_previous_sequence_id=0",
    "pipeline_current_sequence_id=0",
    "pipeline_effect_batch_written=false",
    "pipeline_sequence_advanced=false"
)) {
    if ($pipelineInvalidReadText -notmatch [regex]::Escape($expectedLine)) {
        throw "Pipeline invalid read test missing expected output '$expectedLine'. Actual output:`n$pipelineInvalidReadText"
    }
}

if (Test-Path -LiteralPath $pipelineInvalidReadEffectBatchPath) {
    throw "Pipeline invalid read did not clear stale effect batch output '$pipelineInvalidReadEffectBatchPath'."
}
if (Test-Path -LiteralPath $pipelineInvalidReadSequencePath) {
    throw "Pipeline invalid read advanced sequence state '$pipelineInvalidReadSequencePath'."
}
Write-Host "[PASS] pipeline malformed payload clears stale batch without sequence advance"

$pipelineReplayOutput = & $exePath `
    --pipeline `
    (Join-Path $repoRoot "resources/decision_valid_receiver_dimensions.json") `
    $pipelineEffectBatchPath `
    $pipelineSequencePath `
    20000
$pipelineReplayText = $pipelineReplayOutput -join "`n"

foreach ($expectedLine in @(
    "pipeline_accepted=false",
    "pipeline_reason=replayed or old sequence_id",
    "pipeline_sequence_id=20",
    "pipeline_previous_sequence_id=20",
    "pipeline_current_sequence_id=20",
    "pipeline_effect_batch_written=false",
    "pipeline_sequence_advanced=false"
)) {
    if ($pipelineReplayText -notmatch [regex]::Escape($expectedLine)) {
        throw "Pipeline replay test missing expected output '$expectedLine'. Actual output:`n$pipelineReplayText"
    }
}

if (Test-Path -LiteralPath $pipelineEffectBatchPath) {
    throw "Pipeline replay did not clear stale effect batch output '$pipelineEffectBatchPath'."
}
Write-Host "[PASS] pipeline rejects replay and clears stale batch"

$pipelineSequenceParentFilePath = Join-Path $repoRoot "dist/pipeline_sequence_parent_file"
$pipelineSequencePathUnderFile = Join-Path $pipelineSequenceParentFilePath "state.txt"
$pipelineSequenceFailureEffectBatchPath = Join-Path $repoRoot "dist/pipeline_sequence_failure_effect_batch.txt"
Remove-Item -LiteralPath $pipelineSequenceFailureEffectBatchPath -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $pipelineSequenceParentFilePath -Recurse -Force -ErrorAction SilentlyContinue
Set-Content -LiteralPath $pipelineSequenceParentFilePath -Value "not a directory"

$pipelineSequenceFailureOutput = & $exePath `
    --pipeline `
    (Join-Path $repoRoot "resources/decision_valid_receiver_dimensions.json") `
    $pipelineSequenceFailureEffectBatchPath `
    $pipelineSequencePathUnderFile `
    20000
$pipelineSequenceFailureText = $pipelineSequenceFailureOutput -join "`n"

foreach ($expectedLine in @(
    "pipeline_accepted=false",
    "pipeline_reason=failed to replace sequence state",
    "pipeline_sequence_id=20",
    "pipeline_previous_sequence_id=0",
    "pipeline_effect_batch_written=false",
    "pipeline_sequence_advanced=false"
)) {
    if ($pipelineSequenceFailureText -notmatch [regex]::Escape($expectedLine)) {
        throw "Pipeline sequence write failure test missing expected output '$expectedLine'. Actual output:`n$pipelineSequenceFailureText"
    }
}

if (Test-Path -LiteralPath $pipelineSequenceFailureEffectBatchPath) {
    throw "Pipeline sequence write failure left effect batch output '$pipelineSequenceFailureEffectBatchPath'."
}
if (-not (Test-Path -LiteralPath $pipelineSequenceParentFilePath -PathType Leaf)) {
    throw "Pipeline sequence write failure removed sequence parent file '$pipelineSequenceParentFilePath'."
}
Write-Host "[PASS] pipeline clears effect batch when sequence write fails"

Write-Host "Bridge core smoke tests passed."
