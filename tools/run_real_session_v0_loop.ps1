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
    [string]$SessionId,
    [string]$WorkDir,
    [string]$OverlayOutputDir,
    [string]$StatusOutputJson,
    [string]$MpPackageOutputDir,
    [string]$MpOverlayVersion = "overlay_v0_session",
    [string]$MpGameVersion = "stellaris_4.x",
    [string]$MpModVersion = "strategic_nexus_v0",
    [switch]$ExportMpPackage,
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
$mpExportVerifyCommand = ""
$mpExportImportCommand = ""
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
    $mpExportVerifyCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_verify_command"
    $mpExportImportCommand = Get-KeyValueLineValue -Lines $mpExportLines -Key "mp_overlay_package_export_import_command"

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

Write-Host "real_session_v0_loop_ok=true"
Write-Host ("real_session_v0_loop_session_id=" + $SessionId)
Write-Host ("real_session_v0_loop_session_archive_dir=" + $sessionArchiveDir)
Write-Host ("real_session_v0_loop_archive_summary_path=" + $archiveSummaryPath)
Write-Host ("real_session_v0_loop_generated_overlay_dir=" + $overlayOutputDirFull)
Write-Host ("real_session_v0_loop_status_snapshot_path=" + $statusOutputJsonFull)
Write-Host ("real_session_v0_loop_compare_previous_session_dir_hint=dist\\real_session_v0_loop\\<previous_session_id>")
$compareCommandHint = 'cmd /c tools\compare_real_session_v0_outputs.cmd "dist\real_session_v0_loop\<previous_session_id>" "' + $defaultRunRoot + '" "dist\private_reports\real_session_v0_compare_' + $SessionId + '.json"'
Write-Host ("real_session_v0_loop_compare_command_hint=" + $compareCommandHint)
if ($ExportMpPackage) {
    Write-Host ("real_session_v0_loop_mp_package_output_dir=" + $mpPackageOutputDirFull)
    Write-Host ("real_session_v0_loop_mp_package_readiness=" + $mpExportReadiness)
    Write-Host ("real_session_v0_loop_mp_package_manifest_hash=" + $mpExportManifestHash)
    Write-Host ("real_session_v0_loop_mp_package_warning_count=" + $mpExportWarningCount)
    Write-Host ("real_session_v0_loop_mp_package_identity_mismatch_warning=" + $mpExportIdentityMismatchWarning)
    Write-Host ("real_session_v0_loop_mp_package_verify_command=" + $mpExportVerifyCommand)
    Write-Host ("real_session_v0_loop_mp_package_import_command=" + $mpExportImportCommand)
    Write-Host ("real_session_v0_loop_status_snapshot_with_mp_path=" + $statusWithMpOutputJsonFull)
    Write-Host ("real_session_v0_loop_status_snapshot_with_mp_readiness=" + $sncStatusWithMpReadiness)
}
exit 0
