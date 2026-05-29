$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-ExePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

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
    param(
        [Parameter(Mandatory = $true)]
        [string]$StepName
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$StepName failed (exit code $LASTEXITCODE)."
    }
}

$exe = Resolve-ExePath -RelativePath "dist/strategic_nexus_app_test.exe"
if (-not $exe) {
    throw "Missing strategic_nexus_app_test.exe. Tip: build via tools/run_v0_pipeline_tests.ps1 (compiles) before running this smoke test."
}

$distDir = Join-Path $repoRoot "dist"
$smokeRoot = Join-Path $distDir "end_to_end_spine_smoke"
$workDir = Join-Path $smokeRoot "work"
$overlayDir = Join-Path $smokeRoot "generated_overlay"
$activeOverlayDir = Join-Path $smokeRoot "active_generated_overlay"
$packageDir = Join-Path $smokeRoot "mp_overlay_package"
$statusOut = Join-Path $smokeRoot "snc_status_snapshot.json"

Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $overlayDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $activeOverlayDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $packageDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $statusOut -Force -ErrorAction SilentlyContinue

New-Item -ItemType Directory -Force -Path $workDir | Out-Null
New-Item -ItemType Directory -Force -Path $overlayDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $activeOverlayDir "stale") | Out-Null
Set-Content -LiteralPath (Join-Path $activeOverlayDir "stale/old.txt") -Encoding UTF8 -Value "old"
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

$archiveRoot = Join-Path $smokeRoot "archive_root"
New-Item -ItemType Directory -Force -Path $archiveRoot | Out-Null

$archiveSessionDir = Join-Path $distDir "autosave_archive_cli_archive/session_cli"
$dslInput = Join-Path $distDir "campaign_library_overlay.dsl"

if (-not (Test-Path -LiteralPath $archiveSessionDir)) {
    throw "Missing archive fixture session directory: $archiveSessionDir"
}
if (-not (Test-Path -LiteralPath $dslInput)) {
    throw "Missing DSL input fixture: $dslInput"
}

$campaignId = "smoke_campaign"
$empireId = "smoke_empire"

Write-Host "==> offline spine (archive -> ledger -> brief -> DSL -> overlay -> status)"
& $exe --run-offline-spine $archiveSessionDir $campaignId $empireId $dslInput $workDir $overlayDir $statusOut
Assert-LastExitCodeOk -StepName "offline spine"

Write-Host "==> publish generated overlay to active directory"
& $exe --publish-generated-overlay $overlayDir $activeOverlayDir "false"
Assert-LastExitCodeOk -StepName "generated overlay publish"

if (Test-Path -LiteralPath (Join-Path $activeOverlayDir "stale/old.txt")) {
    throw "Published generated overlay should replace stale active files."
}

Write-Host "==> verify active generated overlay"
& $exe --verify-generated-overlay $activeOverlayDir
Assert-LastExitCodeOk -StepName "active generated overlay verify"

Write-Host "==> stage archive root for SNC status"
$stagedSessionDir = Join-Path $archiveRoot "session_cli"
Remove-Item -LiteralPath $stagedSessionDir -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $archiveSessionDir -Destination $stagedSessionDir -Recurse -Force

Write-Host "==> export MP overlay package"
& $exe --export-mp-overlay-package $activeOverlayDir $campaignId "smoke_v1" "smoke_game_v1" "smoke_sn_mod_v1" $packageDir "false"
Assert-LastExitCodeOk -StepName "mp overlay package export"

Write-Host "==> verify MP overlay package"
& $exe --verify-mp-overlay-package $packageDir
Assert-LastExitCodeOk -StepName "mp overlay package verify"

Write-Host "==> SNC status snapshot (with MP package)"
& $exe --snc-status-snapshot $archiveRoot $activeOverlayDir $statusOut "false" $packageDir
Assert-LastExitCodeOk -StepName "snc status snapshot"

if (-not (Test-Path -LiteralPath $statusOut)) {
    throw "SNC status snapshot output missing: $statusOut"
}

Write-Host "end_to_end_spine_smoke_ok=true"
Write-Host ("end_to_end_spine_smoke_output=" + $statusOut)
exit 0
