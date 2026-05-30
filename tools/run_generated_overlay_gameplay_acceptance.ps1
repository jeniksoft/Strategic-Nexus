$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$appPath = Join-Path $repoRoot "dist/strategic_nexus_app_test.exe"
$reportPath = Join-Path $repoRoot "dist/private_reports/generated_overlay_gameplay_acceptance_v0.json"
$workRoot = Join-Path $repoRoot "dist/generated_overlay_gameplay_acceptance_v0"

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    if ($Text -notlike "*$Expected*") {
        throw "$Name missing expected text: $Expected"
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Text,
        [Parameter(Mandatory = $true)]
        [string]$Unexpected
    )

    if ($Text -like "*$Unexpected*") {
        throw "$Name unexpectedly contained: $Unexpected"
    }
}

function Ensure-AppBinary {
    if (Test-Path -LiteralPath $appPath) {
        return
    }

    $buildOutput = & cmd /c (Join-Path $repoRoot "tools\\run_v0_pipeline_tests.cmd")
    if ($LASTEXITCODE -ne 0) {
        $text = $buildOutput -join "`n"
        throw "failed to build strategic_nexus_app_test.exe via run_v0_pipeline_tests.cmd`n$text"
    }
}

function Invoke-CompileAndVerifyCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseId,
        [Parameter(Mandatory = $true)]
        [string]$DslText,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedFlag,
        [Parameter(Mandatory = $true)]
        [string]$ForbiddenFlag
    )

    $caseRoot = Join-Path $workRoot $CaseId
    $dslPath = Join-Path $caseRoot "input.dsl"
    $overlayDir = Join-Path $caseRoot "overlay"
    Remove-Item -LiteralPath $caseRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    Set-Content -LiteralPath $dslPath -Value $DslText -Encoding UTF8

    $compileOutput = & $appPath --compile-generated-overlay $dslPath $overlayDir
    $compileText = $compileOutput -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "$CaseId compile failed`n$compileText"
    }
    Assert-Contains -Name "$CaseId compile" -Text $compileText -Expected "generated_overlay_success=true"

    $effectsPath = Join-Path $overlayDir "common/scripted_effects/strategic_nexus_generated_effects.txt"
    if (-not (Test-Path -LiteralPath $effectsPath)) {
        throw "$CaseId missing scripted effects output"
    }
    $effectsText = Get-Content -LiteralPath $effectsPath -Raw
    Assert-Contains -Name "$CaseId effects" -Text $effectsText -Expected $ExpectedFlag
    Assert-NotContains -Name "$CaseId effects" -Text $effectsText -Unexpected $ForbiddenFlag

    $verifyOutput = & $appPath --verify-generated-overlay $overlayDir
    $verifyText = $verifyOutput -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "$CaseId verify failed`n$verifyText"
    }
    Assert-Contains -Name "$CaseId verify" -Text $verifyText -Expected "generated_overlay_manifest_ok=true"
    $hashMatch = [regex]::Match($verifyText, "generated_overlay_manifest_hash=([^\r\n]+)")
    $manifestHash = if ($hashMatch.Success) { $hashMatch.Groups[1].Value.Trim() } else { "" }

    return [pscustomobject]@{
        manifest_hash = $manifestHash
    }
}

function Invoke-InvalidDslCase {
    $caseId = "case_e_invalid_tactical_domain"
    $caseRoot = Join-Path $workRoot $caseId
    $overlayDir = Join-Path $caseRoot "overlay"
    Remove-Item -LiteralPath $caseRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null

    $invalidDslPath = Join-Path $repoRoot "resources/generated_overlay_invalid.dsl"
    $compileOutput = & $appPath --compile-generated-overlay $invalidDslPath $overlayDir
    $compileText = $compileOutput -join "`n"
    if ($LASTEXITCODE -eq 0) {
        throw "$caseId should fail compile but succeeded`n$compileText"
    }
    Assert-Contains -Name "$caseId compile" -Text $compileText -Expected "generated_overlay_success=false"
    Assert-Contains -Name "$caseId compile" -Text $compileText -Expected "generated_overlay_reason=validation failed"

    return [pscustomobject]@{
        manifest_hash = ""
    }
}

function Invoke-ManifestDriftCase {
    $caseId = "case_f_manifest_drift_before_publish"
    $caseRoot = Join-Path $workRoot $caseId
    $dslPath = Join-Path $caseRoot "input.dsl"
    $overlayDir = Join-Path $caseRoot "overlay"
    Remove-Item -LiteralPath $caseRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null

    $dsl = @"
campaign "campaign_case_f" {
  empire "empire_case_f" {
    rule "drift_probe" {
      ministry = military_ministry
      prefer military_posture defensive intensity 0.7
      duration = next_session
      confidence = 0.9
      rationale = "Manifest drift fail-closed probe."
    }
  }
}
"@
    Set-Content -LiteralPath $dslPath -Value $dsl -Encoding UTF8

    $compileOutput = & $appPath --compile-generated-overlay $dslPath $overlayDir
    $compileText = $compileOutput -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "$caseId compile failed`n$compileText"
    }
    Assert-Contains -Name "$caseId compile" -Text $compileText -Expected "generated_overlay_success=true"

    $effectsPath = Join-Path $overlayDir "common/scripted_effects/strategic_nexus_generated_effects.txt"
    Add-Content -LiteralPath $effectsPath -Value "`n# drift" -Encoding UTF8

    $verifyOutput = & $appPath --verify-generated-overlay $overlayDir
    $verifyText = $verifyOutput -join "`n"
    if ($LASTEXITCODE -eq 0) {
        throw "$caseId should fail verify after drift but succeeded`n$verifyText"
    }
    Assert-Contains -Name "$caseId verify" -Text $verifyText -Expected "generated_overlay_manifest_ok=false"
    Assert-Contains -Name "$caseId verify" -Text $verifyText -Expected "generated_overlay_manifest_reason=generated overlay files do not match manifest"

    return [pscustomobject]@{
        manifest_hash = ""
    }
}

function Invoke-Case {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseId,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    try {
        $result = & $Action
        return [pscustomobject]@{
            case_id = $CaseId
            result = "pass"
            note = "ok"
            manifest_hash = $result.manifest_hash
        }
    } catch {
        return [pscustomobject]@{
            case_id = $CaseId
            result = "fail"
            note = $_.Exception.Message
            manifest_hash = ""
        }
    }
}

Ensure-AppBinary
Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null

$cases = @()

$cases += Invoke-Case -CaseId "case_a_defensive_military_posture" -Action {
    $dsl = @"
campaign "campaign_case_a" {
  empire "empire_case_a" {
    rule "defensive_posture" {
      ministry = military_ministry
      prefer military_posture defensive intensity 0.7
      duration = next_session
      confidence = 0.9
      rationale = "Defensive posture acceptance."
    }
  }
}
"@
    Invoke-CompileAndVerifyCase `
        -CaseId "case_a_defensive_military_posture" `
        -DslText $dsl `
        -ExpectedFlag "set_country_flag = strategic_nexus_pref_military_posture_defensive" `
        -ForbiddenFlag "set_country_flag = strategic_nexus_pref_military_posture_aggressive"
}

$cases += Invoke-Case -CaseId "case_b_aggressive_military_posture" -Action {
    $dsl = @"
campaign "campaign_case_b" {
  empire "empire_case_b" {
    rule "aggressive_posture" {
      ministry = military_ministry
      prefer military_posture aggressive intensity 0.7
      duration = next_session
      confidence = 0.9
      rationale = "Aggressive posture acceptance."
    }
  }
}
"@
    Invoke-CompileAndVerifyCase `
        -CaseId "case_b_aggressive_military_posture" `
        -DslText $dsl `
        -ExpectedFlag "set_country_flag = strategic_nexus_pref_military_posture_aggressive" `
        -ForbiddenFlag "set_country_flag = strategic_nexus_pref_military_posture_defensive"
}

$cases += Invoke-Case -CaseId "case_c_economy_research_bias" -Action {
    $dsl = @"
campaign "campaign_case_c" {
  empire "empire_case_c" {
    rule "economy_bias" {
      ministry = research_ministry
      prefer research_bias economy intensity 0.7
      duration = next_session
      confidence = 0.9
      rationale = "Economy bias acceptance."
    }
  }
}
"@
    Invoke-CompileAndVerifyCase `
        -CaseId "case_c_economy_research_bias" `
        -DslText $dsl `
        -ExpectedFlag "set_country_flag = strategic_nexus_pref_research_bias_economy" `
        -ForbiddenFlag "set_country_flag = strategic_nexus_pref_research_bias_military_industry"
}

$cases += Invoke-Case -CaseId "case_d_military_industry_research_bias" -Action {
    $dsl = @"
campaign "campaign_case_d" {
  empire "empire_case_d" {
    rule "military_industry_bias" {
      ministry = research_ministry
      prefer research_bias military_industry intensity 0.7
      duration = next_session
      confidence = 0.9
      rationale = "Military industry bias acceptance."
    }
  }
}
"@
    Invoke-CompileAndVerifyCase `
        -CaseId "case_d_military_industry_research_bias" `
        -DslText $dsl `
        -ExpectedFlag "set_country_flag = strategic_nexus_pref_research_bias_military_industry" `
        -ForbiddenFlag "set_country_flag = strategic_nexus_pref_research_bias_economy"
}

$cases += Invoke-Case -CaseId "case_e_invalid_tactical_domain" -Action {
    Invoke-InvalidDslCase
}

$cases += Invoke-Case -CaseId "case_f_manifest_drift_before_publish" -Action {
    Invoke-ManifestDriftCase
}

$allPassed = @($cases | Where-Object { $_.result -ne "pass" }).Count -eq 0
$acceptanceState = if ($allPassed) { "verified_for_v0_domains" } else { "failed" }
$summaryText = if ($allPassed) {
    "gameplay acceptance verified for v0 domains"
} else {
    "gameplay acceptance failed"
}

$report = [pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("d.M.yyyy  H:mm:ss")
    acceptance_state = $acceptanceState
    summary = $summaryText
    cases = $cases
}

$reportDir = Split-Path -Parent $reportPath
if ($reportDir -and -not (Test-Path -LiteralPath $reportDir)) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Host "gameplay_acceptance_success=$($allPassed.ToString().ToLowerInvariant())"
Write-Host "gameplay_acceptance_state=$acceptanceState"
Write-Host "gameplay_acceptance_report_path=$reportPath"

if (-not $allPassed) {
    $failed = $cases | Where-Object { $_.result -ne "pass" }
    foreach ($case in $failed) {
        Write-Host "gameplay_acceptance_failure=$($case.case_id): $($case.note)"
    }
    exit 1
}

