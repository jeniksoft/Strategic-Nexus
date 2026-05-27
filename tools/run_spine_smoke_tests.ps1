$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-TestExePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    $candidatePaths = @(
        (Join-Path $repoRoot $RelativePath),
        (Join-Path $repoRoot ("dist/test_bin/" + (Split-Path -Leaf $RelativePath))),
        (Join-Path $repoRoot ("dist/" + (Split-Path -Leaf $RelativePath)))
    )

    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

$tests = @(
    @{ name = "autosave_archive_verifier_test"; path = "dist/test_bin/autosave_archive_verifier_test.exe" },
    @{ name = "autosave_archive_summarizer_test"; path = "dist/test_bin/autosave_archive_summarizer_test.exe" },
    @{ name = "season_delta_ledger_builder_test"; path = "dist/test_bin/season_delta_ledger_builder_test.exe" },
    @{ name = "season_empire_brief_builder_test"; path = "dist/test_bin/season_empire_brief_builder_test.exe" },
    @{ name = "strategic_nexus_companion_test"; path = "dist/test_bin/strategic_nexus_companion_test.exe" },
    @{ name = "generated_overlay_contract_test"; path = "dist/test_bin/generated_overlay_contract_test.exe" },
    @{ name = "generated_overlay_verifier_test"; path = "dist/test_bin/generated_overlay_verifier_test.exe" },
    @{ name = "mp_overlay_package_verifier_test"; path = "dist/test_bin/mp_overlay_package_verifier_test.exe" }
)

$missing = @()
$failed = @()

foreach ($test in $tests) {
    $exe = Resolve-TestExePath -RelativePath $test.path
    if (-not $exe) {
        $missing += $test.name
        continue
    }

    Write-Host "==> $($test.name)"
    & $exe
    if ($LASTEXITCODE -ne 0) {
        $failed += $test.name
    }
}

if ($missing.Count -gt 0) {
    Write-Warning ("Missing test binaries: " + ($missing -join ", "))
    Write-Warning "Tip: run tools/run_v0_pipeline_tests.ps1 to (re)build the test binaries."
}

if ($failed.Count -gt 0) {
    Write-Error ("FAILED: " + ($failed -join ", "))
    exit 1
}

Write-Host "Spine smoke tests OK."
exit 0
