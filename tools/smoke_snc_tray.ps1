param(
    [switch]$ReadyOwnerTestFixture,
    [switch]$PostPlayBackfillFixture,
    [switch]$MemoryRecoveryFixture
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"
$statusPath = Join-Path $repoRoot "dist\private_reports\snc_tray_status.json"
$liveStatusPath = Join-Path $repoRoot "dist\private_reports\snc_live_autosave_monitor_status.json"
$nextStepsBriefPath = Join-Path $repoRoot "dist\private_reports\snc_next_steps_brief.txt"
$privateReportsRoot = Join-Path $repoRoot "dist\private_reports"
$supportReportPreviewPath = Join-Path $privateReportsRoot "snc_support_report_preview.txt"
$friendTrustStorePath = Join-Path $privateReportsRoot "snc_friend_trust_store.json"
$dslDraftPath = Join-Path $privateReportsRoot "snc_validated_dsl_draft.dsl"
$dslDraftAuditPath = Join-Path $privateReportsRoot "snc_dsl_draft_package.json"
$candidateDecisionPackagePath = Join-Path $privateReportsRoot "snc_candidate_decision_package.json"
$entryPointAnalysisPath = Join-Path $privateReportsRoot "snc_entry_point_analysis.json"
$activeOverlayDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_active"
$stagedOverlayDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_staged"
$stagingStatusPath = Join-Path $privateReportsRoot "snc_generated_overlay_staging_status.json"
$publishStatusPath = Join-Path $privateReportsRoot "snc_generated_overlay_publish_status.json"
$publishBackupRootDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_backups"
$gameplayAcceptanceReportPath = Join-Path $privateReportsRoot "generated_overlay_gameplay_acceptance_v0.json"
$mpOverlayPackageDirectory = Join-Path $privateReportsRoot "snc_mp_overlay_package"
$mpOverlayPackageZipPath = Join-Path $privateReportsRoot "snc_mp_overlay_package.zip"

if ((@($ReadyOwnerTestFixture, $PostPlayBackfillFixture, $MemoryRecoveryFixture) | Where-Object { $_ }).Count -gt 1) {
    throw "Choose only one SNC tray smoke fixture at a time."
}

function Get-ExpectedStartupLifecycleContract {
    $startupFolder = [Environment]::GetFolderPath("Startup")
    $shortcutPath = if ([string]::IsNullOrWhiteSpace($startupFolder)) {
        ""
    } else {
        Join-Path $startupFolder "Strategic Nexus Companion.lnk"
    }

    if ([string]::IsNullOrWhiteSpace($shortcutPath)) {
        return [pscustomobject]@{
            Enabled = $false
            LifecycleState = "manual_start_only"
            LifecycleBrief = "manual start only (start with Windows disabled)"
            Source = "startup_shortcut_probe"
            ShortcutState = "shortcut_path_unavailable"
            ShortcutStateBrief = "startup shortcut path unavailable"
            ShortcutPath = ""
            CommandHint = ""
            CommandHintSource = "startup_shortcut_unavailable"
            EnableCommandHint = "cmd /c tools\install_snc_tray_startup_shortcut.cmd"
            DisableCommandHint = "cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
        }
    }

    $installed = Test-Path -LiteralPath $shortcutPath -PathType Leaf
    return [pscustomobject]@{
        Enabled = $installed
        LifecycleState = if ($installed) { "owner_enabled_start_with_windows" } else { "manual_start_only" }
        LifecycleBrief = if ($installed) { "owner enabled start with Windows" } else { "manual start only (start with Windows disabled)" }
        Source = "startup_shortcut_probe"
        ShortcutState = if ($installed) { "shortcut_installed" } else { "shortcut_not_installed" }
        ShortcutStateBrief = if ($installed) { "startup shortcut installed" } else { "startup shortcut not installed" }
        ShortcutPath = $shortcutPath
        CommandHint = if ($installed) { "cmd /c tools\remove_snc_tray_startup_shortcut.cmd" } else { "cmd /c tools\install_snc_tray_startup_shortcut.cmd" }
        CommandHintSource = if ($installed) { "startup_shortcut_remove_command" } else { "startup_shortcut_install_command" }
        EnableCommandHint = "cmd /c tools\install_snc_tray_startup_shortcut.cmd"
        DisableCommandHint = "cmd /c tools\remove_snc_tray_startup_shortcut.cmd"
    }
}

$expectedStartup = Get-ExpectedStartupLifecycleContract
$expectedStartupEnabledText = if ($expectedStartup.Enabled) { "true" } else { "false" }
$expectedStartupShortcutPathText = if ([string]::IsNullOrWhiteSpace($expectedStartup.ShortcutPath)) {
    ""
} else {
    ([string]$expectedStartup.ShortcutPath).Replace('\', '/')
}
$expectedSupportReportPreviewPathText = $supportReportPreviewPath.Replace('\', '/')
$expectedFriendTrustStorePathText = "dist/private_reports/snc_friend_trust_store.json"

function Get-Fnv1a64Hex {
    param([string]$Text)

    $hash = [System.Numerics.BigInteger]::Parse("14695981039346656037")
    $prime = [System.Numerics.BigInteger]::Parse("1099511628211")
    $modulus = [System.Numerics.BigInteger]::Pow(2, 64)
    foreach ($byte in [System.Text.Encoding]::UTF8.GetBytes($Text)) {
        $hash = [System.Numerics.BigInteger]::op_ExclusiveOr($hash, [System.Numerics.BigInteger]$byte)
        $hash = ($hash * $prime) % $modulus
    }

    return ([UInt64]$hash).ToString("x16")
}

function Backup-PathState {
    param(
        [string]$Path,
        [System.Collections.ArrayList]$State,
        [string]$BackupRoot
    )

    $entry = [pscustomobject]@{
        Original = $Path
        Backup = $null
        Existed = Test-Path -LiteralPath $Path
    }

    if ($entry.Existed) {
        $backupPath = Join-Path $BackupRoot ([guid]::NewGuid().ToString("N"))
        Move-Item -LiteralPath $Path -Destination $backupPath
        $entry.Backup = $backupPath
    }

    [void]$State.Add($entry)
}

function Restore-BackedUpPaths {
    param([System.Collections.ArrayList]$State)

    for ($i = $State.Count - 1; $i -ge 0; $i--) {
        $entry = $State[$i]
        if (Test-Path -LiteralPath $entry.Original) {
            Remove-Item -LiteralPath $entry.Original -Recurse -Force -ErrorAction SilentlyContinue
        }
        if ($entry.Existed -and $null -ne $entry.Backup -and (Test-Path -LiteralPath $entry.Backup)) {
            Move-Item -LiteralPath $entry.Backup -Destination $entry.Original
        }
    }
}

function Initialize-ReadyOwnerTestFixture {
    param([string]$BackupRoot)

    $backedUpPaths = [System.Collections.ArrayList]::new()
    foreach ($path in @(
        $statusPath,
        $liveStatusPath,
        $nextStepsBriefPath,
        $friendTrustStorePath,
        $activeOverlayDirectory,
        $stagedOverlayDirectory,
        $stagingStatusPath,
        $publishStatusPath,
        $publishBackupRootDirectory,
        $gameplayAcceptanceReportPath,
        $mpOverlayPackageDirectory,
        $mpOverlayPackageZipPath
    )) {
        Backup-PathState -Path $path -State $backedUpPaths -BackupRoot $BackupRoot
    }

    New-Item -ItemType Directory -Force -Path $privateReportsRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $activeOverlayDirectory | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $activeOverlayDirectory "events") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $activeOverlayDirectory "common\scripted_effects") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $activeOverlayDirectory "common\scripted_triggers") | Out-Null
    New-Item -ItemType Directory -Force -Path $stagedOverlayDirectory | Out-Null
    New-Item -ItemType Directory -Force -Path $publishBackupRootDirectory | Out-Null

    $eventsText = "event test`n"
    $effectsText = "effects test`n"
    $triggersText = "triggers test`n"
    $eventsHash = Get-Fnv1a64Hex -Text $eventsText
    $effectsHash = Get-Fnv1a64Hex -Text $effectsText
    $triggersHash = Get-Fnv1a64Hex -Text $triggersText

    Set-Content -LiteralPath (Join-Path $activeOverlayDirectory "events\strategic_nexus_generated_events.txt") -Value $eventsText -NoNewline
    Set-Content -LiteralPath (Join-Path $activeOverlayDirectory "common\scripted_effects\strategic_nexus_generated_effects.txt") -Value $effectsText -NoNewline
    Set-Content -LiteralPath (Join-Path $activeOverlayDirectory "common\scripted_triggers\strategic_nexus_generated_triggers.txt") -Value $triggersText -NoNewline

    $manifestText = @"
{
  "schema_version": 1,
  "reactive_policy_pack_capability": "event_family_dispatch",
  "event_families": ["monthly_strategy_tick"],
  "source_qualities": ["history_backed", "zero_history_bootstrap"],
  "files": [
    {
      "path": "events/strategic_nexus_generated_events.txt",
      "checksum_relevance": "gameplay_affecting",
      "hash_algorithm": "fnv1a64",
      "hash": "$eventsHash",
      "byte_count": $($eventsText.Length)
    },
    {
      "path": "common/scripted_effects/strategic_nexus_generated_effects.txt",
      "checksum_relevance": "gameplay_affecting",
      "hash_algorithm": "fnv1a64",
      "hash": "$effectsHash",
      "byte_count": $($effectsText.Length)
    },
    {
      "path": "common/scripted_triggers/strategic_nexus_generated_triggers.txt",
      "checksum_relevance": "gameplay_affecting",
      "hash_algorithm": "fnv1a64",
      "hash": "$triggersHash",
      "byte_count": $($triggersText.Length)
    }
  ]
}
"@
    Set-Content -LiteralPath (Join-Path $activeOverlayDirectory "strategic_nexus_generated_manifest.json") -Value $manifestText -NoNewline
    $manifestHash = Get-Fnv1a64Hex -Text $manifestText

    $stagingStatusText = @"
{
  "schema_version": 1,
  "ok": true,
  "reason": "validated generated overlay staged",
  "readiness": "staged_verified",
  "manifest_verified": true,
  "publish_allowed": false,
  "publishes_overlay": false,
  "staged_overlay_directory": "$($stagedOverlayDirectory.Replace('\', '/'))",
  "manifest_hash": "$manifestHash",
  "dsl_rule_count": 10
}
"@
    Set-Content -LiteralPath $stagingStatusPath -Value $stagingStatusText -NoNewline

    $backupDirectory = Join-Path $publishBackupRootDirectory "snc_generated_overlay_active_fixture_backup"
    New-Item -ItemType Directory -Force -Path $backupDirectory | Out-Null
    $publishStatusText = @"
{
  "schema_version": 1,
  "ok": true,
  "reason": "owner approved generated overlay published",
  "readiness": "published",
  "owner_approved": true,
  "published": true,
  "backup_created": true,
  "stellaris_running": false,
  "staging_status_path": "$($stagingStatusPath.Replace('\', '/'))",
  "staged_overlay_directory": "$($stagedOverlayDirectory.Replace('\', '/'))",
  "active_overlay_directory": "$($activeOverlayDirectory.Replace('\', '/'))",
  "backup_directory": "$($backupDirectory.Replace('\', '/'))",
  "source_manifest_hash": "$manifestHash",
  "published_manifest_hash": "$manifestHash",
  "published_file_count": 3,
  "warning_codes": [],
  "validation_errors": []
}
"@
    Set-Content -LiteralPath $publishStatusPath -Value $publishStatusText -NoNewline

    $gameplayAcceptanceReportText = @"
{
  "schema_version": 1,
  "acceptance_state": "verified_for_v0_domains",
  "summary": "gameplay acceptance verified for monthly reactive v0 markers",
  "cases": [
    { "case_id": "case_a_defensive_military_posture", "result": "pass" },
    { "case_id": "case_b_aggressive_military_posture", "result": "pass" },
    { "case_id": "case_c_economy_research_bias", "result": "pass" },
    { "case_id": "case_d_military_industry_research_bias", "result": "pass" },
    { "case_id": "case_e_invalid_tactical_domain", "result": "pass" },
    { "case_id": "case_f_manifest_drift_before_publish", "result": "pass" }
  ]
}
"@
    Set-Content -LiteralPath $gameplayAcceptanceReportPath -Value $gameplayAcceptanceReportText -NoNewline

    $friendTrustStoreText = @"
{
  "schema_version": 1,
  "friends": [
    {
      "node_id": "snc-node-client-001",
      "display_name": "Client SNC",
      "local_alias": "Client",
      "signing_public_key": "ed25519:snc-node-client-001-signing-public-key",
      "encryption_public_key": "x25519:snc-node-client-001-encryption-public-key",
      "fingerprint": "fp-client-001",
      "capabilities": ["mp_package_sync", "handoff_sync"],
      "trust_state": "trusted",
      "auto_sync_enabled": true,
      "accepted_at": "2026-06-08T17:36:00Z",
      "updated_at": "2026-06-08T17:36:00Z"
    },
    {
      "node_id": "snc-node-old-client-001",
      "display_name": "Old Client SNC",
      "local_alias": "Old Client",
      "signing_public_key": "ed25519:snc-node-old-client-001-signing-public-key",
      "encryption_public_key": "x25519:snc-node-old-client-001-encryption-public-key",
      "fingerprint": "fp-old-client-001",
      "capabilities": ["mp_package_sync", "handoff_sync"],
      "trust_state": "revoked",
      "auto_sync_enabled": false,
      "accepted_at": "2026-06-07T10:00:00Z",
      "updated_at": "2026-06-08T10:00:00Z"
    },
    {
      "node_id": "snc-node-blocked-001",
      "display_name": "Blocked SNC",
      "local_alias": "Blocked",
      "signing_public_key": "ed25519:snc-node-blocked-001-signing-public-key",
      "encryption_public_key": "x25519:snc-node-blocked-001-encryption-public-key",
      "fingerprint": "fp-blocked-001",
      "capabilities": ["mp_package_sync", "handoff_sync"],
      "trust_state": "blocked",
      "auto_sync_enabled": false,
      "accepted_at": "2026-06-07T17:36:00Z",
      "updated_at": "2026-06-08T17:36:00Z"
    }
  ]
}
"@
    Set-Content -LiteralPath $friendTrustStorePath -Value $friendTrustStoreText -NoNewline

    $seedPackageDirectory = Join-Path $repoRoot "dist\generated_overlay_mp_package_cli"
    if (-not (Test-Path -LiteralPath $seedPackageDirectory)) {
        throw "Ready-owner-test tray smoke fixture needs dist\\generated_overlay_mp_package_cli as a valid MP package seed."
    }

    Copy-Item -LiteralPath $seedPackageDirectory -Destination $mpOverlayPackageDirectory -Recurse -Force
    $mpManifestPath = Join-Path $mpOverlayPackageDirectory "strategic_nexus_mp_overlay_package_manifest.json"
    $mpManifestText = Get-Content -Raw -LiteralPath $mpManifestPath
    $mpManifestText = $mpManifestText.Replace('"previous_host_available": false', '"previous_host_available": true')
    $mpManifestText = $mpManifestText.Replace(
        '"handoff_status": "degraded_previous_host_unavailable"',
        '"handoff_status": "complete"')
    Set-Content -LiteralPath $mpManifestPath -Value $mpManifestText -NoNewline

    Set-Content -LiteralPath $mpOverlayPackageZipPath -Value "ready owner-test mp package zip fixture`n" -NoNewline

    return $backedUpPaths
}

function Initialize-PostPlayBackfillFixture {
    param([string]$BackupRoot)

    $backedUpPaths = [System.Collections.ArrayList]::new()
    foreach ($path in @(
        $statusPath,
        $liveStatusPath,
        $nextStepsBriefPath,
        $entryPointAnalysisPath,
        $dslDraftPath,
        $dslDraftAuditPath,
        $candidateDecisionPackagePath,
        $activeOverlayDirectory,
        $stagedOverlayDirectory,
        $stagingStatusPath,
        $publishStatusPath,
        $publishBackupRootDirectory,
        $mpOverlayPackageDirectory,
        $mpOverlayPackageZipPath
    )) {
        Backup-PathState -Path $path -State $backedUpPaths -BackupRoot $BackupRoot
    }

    New-Item -ItemType Directory -Force -Path $privateReportsRoot | Out-Null

    $dslText = @"
campaign "campaign_backfill" {
  empire "country_0" {
    rule "entry_backfill" {
      ministry = military_ministry
      when campaign_marker = campaign_backfill
      when known.save_fingerprint = h_backfill123456
      when known.save_date = d_2200_03_01
      prefer doctrine_inertia high intensity 0.8
      duration = next_session
      confidence = 0.25
      rationale = "Tray should backfill missing staged artifacts from an existing DSL draft."
    }
  }
}
"@
    Set-Content -LiteralPath $dslDraftPath -Value $dslText -NoNewline

    $dslAuditText = @"
{
  "schema_version": 1,
  "ok": true,
  "reason": "dsl draft built from deterministic candidate decisions",
  "readiness": "ready_partial",
  "dry_run_only": true,
  "publishes_overlay": false,
  "validator_passed": true,
  "dsl_rule_count": 1,
  "eligible_candidate_count": 1,
  "skipped_candidate_count": 0,
  "warning_codes": [],
  "validation_errors": []
}
"@
    Set-Content -LiteralPath $dslDraftAuditPath -Value $dslAuditText -NoNewline

    $entryPointAnalysisText = @"
{
  "schema_version": 1,
  "reason": "entry points scanned for backfill fixture",
  "readiness": "ready",
  "archive_verified": true,
  "branch_ambiguity_detected": false,
  "entry_point_count": 1,
  "archived_evidence_count": 1,
  "entry_points": [
    {
      "id": "backfill_fixture_2200_03_01",
      "campaign_key": "campaign_backfill",
      "source_root": "current_save_root",
      "relative_path": "Campaign Backfill/autosave_2200.03.01.sav",
      "source_kind": "autosave",
      "save_name": "autosave_2200.03.01.sav",
      "save_date": "2200.03.01",
      "analysis_state": "ready",
      "compatible_archived_evidence_count": 1,
      "later_archived_evidence_count": 0
    }
  ]
}
"@
    Set-Content -LiteralPath $entryPointAnalysisPath -Value $entryPointAnalysisText -NoNewline

    $candidatePackageText = @"
{
  "schema_version": 1,
  "ok": true,
  "reason": "candidate decision package built",
  "readiness": "ready",
  "validator_passed": true,
  "candidate_decision_count": 1,
  "blocked_source_entry_count": 0,
  "candidate_decisions": [
    {
      "campaign_key": "Campaign Backfill",
      "candidate_decision_id": "candidate-decision::Campaign Backfill::2200.03.01::abcd",
      "decision_input_id": "decision-input::Campaign Backfill::2200.03.01::abcd",
      "package_entry_id": "postplay::Campaign Backfill::2200.03.01::abcd"
    }
  ]
}
"@
    Set-Content -LiteralPath $candidateDecisionPackagePath -Value $candidatePackageText -NoNewline
    Set-Content -LiteralPath $mpOverlayPackageZipPath -Value "stale mp zip fixture`n" -NoNewline

    return $backedUpPaths
}

function Initialize-MemoryRecoveryFixture {
    param([string]$BackupRoot)

    $backedUpPaths = [System.Collections.ArrayList]::new()
    foreach ($path in @(
        $statusPath,
        $liveStatusPath,
        $nextStepsBriefPath,
        $entryPointAnalysisPath,
        $dslDraftPath,
        $dslDraftAuditPath,
        $candidateDecisionPackagePath,
        $stagedOverlayDirectory,
        $stagingStatusPath,
        $publishStatusPath,
        $mpOverlayPackageDirectory,
        $mpOverlayPackageZipPath
    )) {
        Backup-PathState -Path $path -State $backedUpPaths -BackupRoot $BackupRoot
    }

    New-Item -ItemType Directory -Force -Path $privateReportsRoot | Out-Null

    $entryPointAnalysisText = @"
{
  "schema_version": 1,
  "reason": "entry points scanned; branch ambiguity requires conservative evidence selection",
  "readiness": "ambiguous",
  "archive_verified": true,
  "branch_ambiguity_detected": true,
  "entry_point_count": 1,
  "archived_evidence_count": 1,
  "entry_points": [
    {
      "id": "memory_fixture_ironman_2200_05_01",
      "campaign_key": "memory_fixture_campaign",
      "source_root": "current_save_root",
      "relative_path": "MemoryFixture/ironman.sav",
      "source_kind": "ironman",
      "save_name": "ironman.sav",
      "save_date": "2200.05.01",
      "analysis_state": "ready_conservative",
      "compatible_archived_evidence_count": 1,
      "later_archived_evidence_count": 1
    }
  ]
}
"@
    Set-Content -LiteralPath $entryPointAnalysisPath -Value $entryPointAnalysisText -NoNewline

    $seedPackageDirectory = Join-Path $repoRoot "dist\generated_overlay_mp_package_cli"
    if (-not (Test-Path -LiteralPath $seedPackageDirectory)) {
        throw "Memory-recovery tray smoke fixture needs dist\\generated_overlay_mp_package_cli as a valid MP package seed."
    }

    Copy-Item -LiteralPath $seedPackageDirectory -Destination $mpOverlayPackageDirectory -Recurse -Force
    $mpManifestPath = Join-Path $mpOverlayPackageDirectory "strategic_nexus_mp_overlay_package_manifest.json"
    $mpManifestText = Get-Content -Raw -LiteralPath $mpManifestPath
    $mpManifestText = $mpManifestText.Replace('"previous_host_available": false', '"previous_host_available": true')
    $mpManifestText = $mpManifestText.Replace(
        '"handoff_status": "degraded_previous_host_unavailable"',
        '"handoff_status": "complete"')
    Set-Content -LiteralPath $mpManifestPath -Value $mpManifestText -NoNewline

    Set-Content -LiteralPath $mpOverlayPackageZipPath -Value "memory recovery mp package zip fixture`n" -NoNewline

    return $backedUpPaths
}

& (Join-Path $PSScriptRoot "build_snc_tray.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$existing = Get-Process -Name "StrategicNexusCompanionTray" -ErrorAction SilentlyContinue
if ($existing) {
    throw "SNC tray is already running; stop it before smoke testing so the test does not close the owner instance."
}

$supportReportBackupPath = $null
if (Test-Path -LiteralPath $supportReportPreviewPath) {
    $supportReportBackupPath = Join-Path ([System.IO.Path]::GetTempPath()) ("snc-support-preview-" + [guid]::NewGuid().ToString("N") + ".txt")
    Move-Item -LiteralPath $supportReportPreviewPath -Destination $supportReportBackupPath
}

Remove-Item -Force -LiteralPath $statusPath -ErrorAction SilentlyContinue
Remove-Item -Force -LiteralPath $liveStatusPath -ErrorAction SilentlyContinue
Remove-Item -Force -LiteralPath $supportReportPreviewPath -ErrorAction SilentlyContinue

$fixtureBackupRoot = $null
$fixtureBackups = $null
if ($ReadyOwnerTestFixture -or $PostPlayBackfillFixture -or $MemoryRecoveryFixture) {
    $fixtureBackupRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("snc-tray-smoke-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $fixtureBackupRoot | Out-Null
    if ($ReadyOwnerTestFixture) {
        $fixtureBackups = Initialize-ReadyOwnerTestFixture -BackupRoot $fixtureBackupRoot
    } elseif ($MemoryRecoveryFixture) {
        $fixtureBackups = Initialize-MemoryRecoveryFixture -BackupRoot $fixtureBackupRoot
    } else {
        $fixtureBackups = Initialize-PostPlayBackfillFixture -BackupRoot $fixtureBackupRoot
    }
}

$previousSncUiLanguage = $env:SNC_UI_LANGUAGE
$env:SNC_UI_LANGUAGE = "en"
$process = Start-Process -FilePath $exePath -WorkingDirectory $repoRoot -PassThru

try {
    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $statusPath) {
            $text = Get-Content -Raw -LiteralPath $statusPath
            $json = $text | ConvertFrom-Json
            if (
                $json.mode -eq "tray" -and
                $json.state -and
                $null -ne $json.startup_rationale -and
                $null -ne $json.start_with_windows_default_state -and
                $null -ne $json.window_close_behavior -and
                $null -ne $json.explicit_exit_behavior -and
                $null -ne $json.crash_restart_policy -and
                $null -ne $json.start_with_windows_source -and
                $null -ne $json.start_with_windows_shortcut_state -and
                $null -ne $json.start_with_windows_shortcut_path -and
                $null -ne $json.start_with_windows_command_hint -and
                $null -ne $json.start_with_windows_command_hint_source -and
                $null -ne $json.start_with_windows_enable_command_hint -and
                $null -ne $json.start_with_windows_disable_command_hint -and
                $null -ne $json.support_report_state -and
                $null -ne $json.support_report_reason -and
                $null -ne $json.support_report_preview_path -and
                $null -ne $json.support_report_contact_email -and
                $null -ne $json.support_report_send_requires_approval -and
                $null -ne $json.support_report_raw_saves_included -and
                $null -ne $json.support_report_prepare_command_hint -and
                $null -ne $json.support_report_open_command_hint -and
                $null -ne $json.support_report_data_categories -and
                $null -ne $json.crash_recovery_state -and
                $null -ne $json.crash_recovery_reason -and
                $null -ne $json.crash_recovery_state_path -and
                $null -ne $json.crash_recovery_last_crash_at_local -and
                $null -ne $json.crash_recovery_last_recovery_action -and
                $null -ne $json.crash_recovery_last_operation -and
                $null -ne $json.crash_recovery_app_version -and
                $null -ne $json.crash_recovery_recent_unexpected_restart_count -and
                $null -ne $json.crash_recovery_restart_window_minutes -and
                $null -ne $json.crash_recovery_next_backoff_seconds -and
                $null -ne $json.crash_recovery_restart_budget_exhausted -and
                $null -ne $json.crash_recovery_warning_visible -and
                $null -ne $json.crash_recovery_support_report_recommended -and
                $null -ne $json.human_control_guard_state -and
                $null -ne $json.status_center_summary_text -and
                $null -ne $json.next_action -and
                $null -ne $json.next_action_command_hint_source -and
                $null -ne $json.next_action_path -and
                $null -ne $json.owner_test_playbook_path -and
                $null -ne $json.next_steps_brief_path -and
                $null -ne $json.generated_overlay_publish_gate_state -and
                $null -ne $json.generated_overlay_publish_gate_reason -and
                $null -ne $json.generated_overlay_publish_gate_published -and
                $null -ne $json.generated_overlay_publish_gate_can_publish -and
                $null -ne $json.memory_recovery_state -and
                $null -ne $json.memory_recovery_reason -and
                $null -ne $json.memory_recovery_confidence -and
                $null -ne $json.memory_recovery_warning_visible -and
                $null -ne $json.memory_recovery_anchor_entry_point_id -and
                $null -ne $json.memory_recovery_anchor_campaign_key -and
                $null -ne $json.memory_recovery_anchor_path -and
                $null -ne $json.memory_recovery_anchor_save_name -and
                $null -ne $json.memory_recovery_anchor_save_date -and
                $null -ne $json.memory_recovery_anchor_source_kind -and
                $null -ne $json.memory_recovery_compatible_archived_evidence_count -and
                $null -ne $json.memory_recovery_later_archived_evidence_count -and
                $null -ne $json.mp_package_refresh_state -and
                $null -ne $json.post_play_package_campaign_identity_state_summary -and
                $null -ne $json.mp_overlay_package_directory -and
                $null -ne $json.mp_overlay_package_state
            ) {
                if ($ReadyOwnerTestFixture -and [string]$json.friend_trust_store_controls_state -ne "ready") {
                    Start-Sleep -Milliseconds 250
                    continue
                }
                if ($json.window_close_behavior -ne "minimize_to_tray") {
                    throw "SNC tray status JSON did not expose window_close_behavior=minimize_to_tray."
                }
                if ($json.explicit_exit_behavior -ne "stop_without_restart") {
                    throw "SNC tray status JSON did not expose explicit_exit_behavior=stop_without_restart."
                }
                if ($json.crash_restart_policy -ne "bounded_backoff_with_crash_loop_guard") {
                    throw "SNC tray status JSON did not expose crash_restart_policy=bounded_backoff_with_crash_loop_guard."
                }
                if ([string]$json.startup_rationale -ne "start SNC before Stellaris to preserve more autosave history before the game rotates older saves away") {
                    throw "SNC tray status JSON did not expose the expected startup_rationale."
                }
                if ([string]$json.start_with_windows_default_state -ne "optional_owner_setting_default_disabled") {
                    throw "SNC tray status JSON did not expose start_with_windows_default_state=optional_owner_setting_default_disabled."
                }
                if ([string]$json.startup_lifecycle_state -ne $expectedStartup.LifecycleState) {
                    throw "SNC tray status JSON did not expose the expected startup_lifecycle_state."
                }
                if ([bool]$json.start_with_windows_enabled -ne [bool]$expectedStartup.Enabled) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_enabled flag."
                }
                if ([string]$json.start_with_windows_source -ne $expectedStartup.Source) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_source."
                }
                if ([string]$json.start_with_windows_shortcut_state -ne $expectedStartup.ShortcutState) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_shortcut_state."
                }
                if ([string]$json.start_with_windows_shortcut_path -ne $expectedStartupShortcutPathText) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_shortcut_path."
                }
                if ([string]$json.start_with_windows_command_hint -ne [string]$expectedStartup.CommandHint) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_command_hint."
                }
                if ([string]$json.start_with_windows_command_hint_source -ne [string]$expectedStartup.CommandHintSource) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_command_hint_source."
                }
                if ([string]$json.start_with_windows_enable_command_hint -ne [string]$expectedStartup.EnableCommandHint) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_enable_command_hint."
                }
                if ([string]$json.start_with_windows_disable_command_hint -ne [string]$expectedStartup.DisableCommandHint) {
                    throw "SNC tray status JSON did not expose the expected start_with_windows_disable_command_hint."
                }
                if ([string]$json.memory_recovery_state -eq "") {
                    throw "SNC tray status JSON did not expose memory_recovery_state."
                }
                if ([string]$json.memory_recovery_confidence -eq "") {
                    throw "SNC tray status JSON did not expose memory_recovery_confidence."
                }
                if ([string]$json.support_report_state -ne "not_prepared") {
                    throw "SNC tray status JSON did not expose support_report_state=not_prepared before preview generation."
                }
                if ([string]$json.support_report_reason -ne
                    "prepare the local support report preview before manual review or sending it") {
                    throw "SNC tray status JSON did not expose the expected support_report_reason before preview generation."
                }
                if ([string]$json.support_report_preview_path -ne $expectedSupportReportPreviewPathText) {
                    throw "SNC tray status JSON did not expose the expected support_report_preview_path."
                }
                if ([string]$json.support_report_contact_email -ne "support@jeniksoft.cz") {
                    throw "SNC tray status JSON did not expose support_report_contact_email=support@jeniksoft.cz."
                }
                if ([bool]$json.support_report_send_requires_approval -ne $true) {
                    throw "SNC tray status JSON did not expose support_report_send_requires_approval=true."
                }
                if ([bool]$json.support_report_raw_saves_included -ne $false) {
                    throw "SNC tray status JSON did not expose support_report_raw_saves_included=false."
                }
                if ([string]$json.support_report_prepare_command_hint -ne
                    "powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_snc_support_report.ps1") {
                    throw "SNC tray status JSON did not expose the expected support_report_prepare_command_hint."
                }
                if (-not [string]::IsNullOrWhiteSpace([string]$json.support_report_open_command_hint)) {
                    throw "SNC tray status JSON should leave support_report_open_command_hint empty before preview generation."
                }
                $mpPackageReady = ([string]$json.mp_overlay_package_state -eq "ready")
                if (-not $ReadyOwnerTestFixture -and $mpPackageReady -and $null -eq $json.mp_overlay_package_previous_host_available) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available."
                }
                if (-not $ReadyOwnerTestFixture -and $mpPackageReady -and $null -eq $json.mp_overlay_package_previous_host_available_known) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available_known."
                }
                if ($mpPackageReady -and [string]$json.mp_overlay_package_handoff_status -eq "degraded_previous_host_unavailable" -and
                    [string]::IsNullOrWhiteSpace([string]$json.mp_overlay_package_handoff_recovery_hint)) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_handoff_recovery_hint for degraded MP handoff continuity."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    $json.next_action -ne "review_staged_overlay_and_publish_if_desired") {
                    throw "SNC tray publish-ready status did not surface the publish/review next action."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    [string]$json.next_action_command_hint_source -ne "generated_overlay_publish_gate_publish_command") {
                    throw "SNC tray publish-ready status did not expose generated_overlay_publish_gate_publish_command as the next-action hint source."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    [string]$json.next_action_command_hint -notlike "*--publish-snc-generated-overlay*") {
                    throw "SNC tray publish-ready status did not expose a concrete publish command hint."
                }
                $ownerTestPlaybookPath = [string]$json.owner_test_playbook_path
                if ($ReadyOwnerTestFixture) {
                    if ($ownerTestPlaybookPath -ne "docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md") {
                        throw "SNC tray ready-state fixture did not expose the stable owner_test_playbook_path field."
                    }
                    if ([string]$json.generated_overlay_publish_gate_state -ne "published") {
                        throw "SNC tray ready-state fixture did not expose generated_overlay_publish_gate_state=published."
                    }
                    if ([string]$json.next_action -ne "run_monthly_reactive_owner_test") {
                        throw "SNC tray ready-state fixture did not align next_action with the monthly reactive owner test contract."
                    }
                    if ([string]$json.next_action_reason -ne "published_monthly_reactive_overlay_ready_for_owner_test") {
                        throw "SNC tray ready-state fixture did not expose the stable owner-test next_action_reason."
                    }
                    if ([string]$json.next_action_command_hint_source -ne "owner_test_playbook_path") {
                        throw "SNC tray ready-state fixture did not reuse owner_test_playbook_path as the next_action command-hint source."
                    }
                    if ([string]$json.next_action_command_hint -ne "open docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md") {
                        throw "SNC tray ready-state fixture did not point the next_action command hint at the owner-test playbook."
                    }
                    $normalizedExpectedAcceptancePath = $gameplayAcceptanceReportPath.Replace('\', '/')
                    if ([string]$json.next_action_path -ne $normalizedExpectedAcceptancePath) {
                        throw "SNC tray ready-state fixture did not reuse the gameplay acceptance artifact as next_action_path."
                    }
                } elseif ($MemoryRecoveryFixture) {
                    if ([string]$json.memory_recovery_state -ne "degraded") {
                        throw "SNC tray memory-recovery fixture did not expose degraded memory recovery state."
                    }
                    if ([string]$json.memory_recovery_reason -ne "branch ambiguity requires conservative recovery anchor") {
                        throw "SNC tray memory-recovery fixture did not expose the branch ambiguity recovery reason."
                    }
                    if ([string]$json.next_action -ne "review_entry_point_ambiguity") {
                        throw "SNC tray memory-recovery fixture did not promote branch ambiguity next_action; actual=$($json.next_action)."
                    }
                    if ([string]$json.next_action_reason -ne "entry_point_branch_ambiguity_detected") {
                        throw "SNC tray memory-recovery fixture did not expose the branch ambiguity next_action_reason."
                    }
                    if ([string]$json.next_action_command_hint_source -ne "snc_next_steps_brief") {
                        throw "SNC tray memory-recovery fixture should route branch ambiguity guidance to the next-steps brief."
                    }
                    if ([string]$json.next_action_path -ne $entryPointAnalysisPath.Replace('\', '/')) {
                        throw "SNC tray memory-recovery fixture did not point next_action_path at entry-point evidence."
                    }
                } elseif ($PostPlayBackfillFixture) {
                    if (-not (Test-Path -LiteralPath $stagingStatusPath)) {
                        throw "SNC tray backfill fixture did not rewrite snc_generated_overlay_staging_status.json."
                    }
                    $backfilledStagingStatus = Get-Content -Raw -LiteralPath $stagingStatusPath | ConvertFrom-Json
                    if ([string]$backfilledStagingStatus.readiness -ne "staged_verified") {
                        throw "SNC tray backfill fixture did not rewrite staging status with readiness=staged_verified."
                    }
                    if ([string]$backfilledStagingStatus.reason -ne "validated generated overlay staged") {
                        throw "SNC tray backfill fixture did not rewrite staging status with the expected reason."
                    }
                    if (-not (Test-Path -LiteralPath $mpOverlayPackageDirectory)) {
                        throw "SNC tray backfill fixture did not recreate snc_mp_overlay_package."
                    }
                    $mpPackageManifestPath = Join-Path $mpOverlayPackageDirectory "strategic_nexus_mp_overlay_package_manifest.json"
                    if (-not (Test-Path -LiteralPath $mpPackageManifestPath)) {
                        throw "SNC tray backfill fixture did not recreate the MP package manifest."
                    }
                    if (-not (Test-Path -LiteralPath $mpOverlayPackageZipPath)) {
                        throw "SNC tray backfill fixture did not recreate snc_mp_overlay_package.zip."
                    }
                } elseif (-not [string]::IsNullOrWhiteSpace($ownerTestPlaybookPath)) {
                    throw "SNC tray smoke expected owner_test_playbook_path to stay empty in the non-ready baseline state."
                }
                $summaryText = [string]$json.status_center_summary_text
                if ($summaryText -notlike "*startup_rationale:*") {
                    throw "SNC tray summary text did not expose startup_rationale."
                }
                if (-not [string]::IsNullOrWhiteSpace([string]$json.post_play_package_campaign_identity_state_summary) -and
                    $summaryText -notlike "*post_play_package_campaign_identity_state_summary:*") {
                    throw "SNC tray summary text did not expose the post-play campaign identity state summary."
                }
                if ([string]$json.post_play_package_campaign_identity_state_summary -eq "folder_alias_fallback" -and
                    $summaryText -notlike "*post_play_package_campaign_identity_owner_note:*") {
                    throw "SNC tray summary text did not expose the campaign identity owner note for folder alias fallback."
                }
                if ($summaryText -notlike "*startup_start_with_windows: optional_owner_setting_default_disabled*") {
                    throw "SNC tray summary text did not expose the default-disabled startup contract."
                }
                if ($summaryText -notlike "*startup_lifecycle_state: $($expectedStartup.LifecycleState)*") {
                    throw "SNC tray summary text did not expose the expected startup lifecycle state."
                }
                if ($summaryText -notlike "*startup_start_with_windows_enabled: $expectedStartupEnabledText*") {
                    throw "SNC tray summary text did not expose the current start-with-Windows enabled flag."
                }
                if ($summaryText -notlike "*startup_start_with_windows_source: $($expectedStartup.Source)*") {
                    throw "SNC tray summary text did not expose the startup source."
                }
                if ($summaryText -notlike "*startup_start_with_windows_shortcut_state: $($expectedStartup.ShortcutState)*") {
                    throw "SNC tray summary text did not expose the startup shortcut state."
                }
                if (-not [string]::IsNullOrWhiteSpace($expectedStartupShortcutPathText) -and
                    $summaryText -notlike "*startup_start_with_windows_shortcut_path: $expectedStartupShortcutPathText*") {
                    throw "SNC tray summary text did not expose the startup shortcut path."
                }
                if ($summaryText -notlike "*startup_start_with_windows_command_hint_source: $($expectedStartup.CommandHintSource)*") {
                    throw "SNC tray summary text did not expose the startup command-hint source."
                }
                if (-not [string]::IsNullOrWhiteSpace([string]$expectedStartup.CommandHint) -and
                    $summaryText -notlike "*startup_start_with_windows_command_hint: $($expectedStartup.CommandHint)*") {
                    throw "SNC tray summary text did not expose the startup command hint."
                }
                if ($summaryText -notlike "*startup_start_with_windows_enable_command_hint: $($expectedStartup.EnableCommandHint)*") {
                    throw "SNC tray summary text did not expose the startup enable command hint."
                }
                if ($summaryText -notlike "*startup_start_with_windows_disable_command_hint: $($expectedStartup.DisableCommandHint)*") {
                    throw "SNC tray summary text did not expose the startup disable command hint."
                }
                if ($summaryText -notlike "*support_report_state: Not prepared yet*") {
                    throw "SNC tray summary text did not expose support_report_state."
                }
                if ($summaryText -notlike "*support_report_preview_path: $expectedSupportReportPreviewPathText*") {
                    throw "SNC tray summary text did not expose support_report_preview_path."
                }
                if ($summaryText -notlike "*support_report_contact_email: support@jeniksoft.cz*") {
                    throw "SNC tray summary text did not expose support_report_contact_email."
                }
                if ($summaryText -notlike "*support_report_send_requires_approval: true*") {
                    throw "SNC tray summary text did not expose support_report_send_requires_approval."
                }
                if ($summaryText -notlike "*support_report_raw_saves_included: false*") {
                    throw "SNC tray summary text did not expose support_report_raw_saves_included."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.local_llm_install_guidance)) {
                    throw "SNC tray status JSON did not expose local_llm_install_guidance."
                }
                if ([string]$json.local_llm_install_guidance -notlike "*--prepare-local-llm-model*") {
                    throw "SNC tray local_llm_install_guidance did not reuse the prepare command hint."
                }
                if ($summaryText -notlike "*local_llm_install_guidance:*") {
                    throw "SNC tray summary text did not expose local_llm_install_guidance."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.local_llm_model_state_path)) {
                    throw "SNC tray status JSON did not expose local_llm_model_state_path."
                }
                if ($summaryText -notlike "*local_llm_model_state_path:*") {
                    throw "SNC tray summary text did not expose local_llm_model_state_path."
                }
                if ([string]$json.next_action -eq "review_local_llm_model_manager") {
                    if ([string]$json.next_action_command_hint_source -ne "local_llm_model_state_path") {
                        throw "SNC tray local LLM attention state did not expose local_llm_model_state_path as the next-action hint source."
                    }
                    if ([string]$json.next_action_path -ne [string]$json.local_llm_model_state_path) {
                        throw "SNC tray local LLM attention state did not point next_action_path at local_llm_model_state_path."
                    }
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_trust_store_state)) {
                    throw "SNC tray status JSON did not expose friend_trust_store_state."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_trust_store_reason)) {
                    throw "SNC tray status JSON did not expose friend_trust_store_reason."
                }
                if ([string]$json.friend_trust_store_path -ne $expectedFriendTrustStorePathText) {
                    throw "SNC tray status JSON did not expose the expected friend_trust_store_path."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_trust_store_controls_state)) {
                    throw "SNC tray status JSON did not expose friend_trust_store_controls_state."
                }
                if ([string]$json.friend_trust_store_controls_state -ne "ready") {
                    throw "SNC tray friend_trust_store_controls_state did not report ready."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_trust_store_update_command_template)) {
                    throw "SNC tray status JSON did not expose friend_trust_store_update_command_template."
                }
                if ([string]$json.friend_trust_store_update_command_template -notlike "*--update-snc-friend-trust-store-entry*" -or
                    [string]$json.friend_trust_store_update_command_template -notlike "*<friend_node_id>*" -or
                    [string]$json.friend_trust_store_update_command_template -notlike "*<trusted|revoked|blocked>*") {
                    throw "SNC tray friend_trust_store_update_command_template did not include the trust-state update controls."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_pairing_command_template)) {
                    throw "SNC tray status JSON did not expose friend_pairing_command_template."
                }
                if ([string]$json.friend_pairing_command_template -notlike "*--create-snc-friend-request*" -or
                    [string]$json.friend_pairing_command_template -notlike "*--create-snc-friend-acceptance*" -or
                    [string]$json.friend_pairing_command_template -notlike "*--import-snc-friend-acceptance*") {
                    throw "SNC tray friend_pairing_command_template did not include the manual request/accept/import flow."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_mp_sync_envelope_command_template)) {
                    throw "SNC tray status JSON did not expose friend_mp_sync_envelope_command_template."
                }
                if ([string]$json.friend_mp_sync_envelope_command_template -notlike "*--create-snc-friend-mp-sync-envelope*" -or
                    [string]$json.friend_mp_sync_envelope_command_template -notlike "*--verify-snc-friend-mp-sync-envelope*" -or
                    [string]$json.friend_mp_sync_envelope_command_template -notlike "*[stellaris_running:true|false]*") {
                    throw "SNC tray friend_mp_sync_envelope_command_template did not include the manual create/verify flow and apply-gate input."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_mp_sync_inbox_plan_command_template)) {
                    throw "SNC tray status JSON did not expose friend_mp_sync_inbox_plan_command_template."
                }
                if ([string]$json.friend_mp_sync_inbox_plan_command_template -notlike "*--plan-snc-friend-mp-sync-inbox*" -or
                    [string]$json.friend_mp_sync_inbox_plan_command_template -notlike "*<encrypted_payload_path>*" -or
                    [string]$json.friend_mp_sync_inbox_plan_command_template -notlike "*[friend_auto_sync_enabled:true|false]*") {
                    throw "SNC tray friend_mp_sync_inbox_plan_command_template did not include the manual inbox-plan gate inputs."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_mp_sync_outbox_plan_command_template)) {
                    throw "SNC tray status JSON did not expose friend_mp_sync_outbox_plan_command_template."
                }
                if ([string]$json.friend_mp_sync_outbox_plan_command_template -notlike "*--plan-snc-friend-mp-sync-outbox*" -or
                    [string]$json.friend_mp_sync_outbox_plan_command_template -notlike "*<encrypted_payload_path>*" -or
                    [string]$json.friend_mp_sync_outbox_plan_command_template -notlike "*[friend_auto_sync_enabled:true|false]*") {
                    throw "SNC tray friend_mp_sync_outbox_plan_command_template did not include the manual outbox-plan gate inputs."
                }
                if ([string]$json.friend_mp_sync_transport_state -ne "disabled_not_implemented") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_state."
                }
                if ([string]$json.friend_mp_sync_transport_reason -notlike "*transport adapter is not implemented*" -or
                    [string]$json.friend_mp_sync_transport_reason -notlike "*upload/send/download/staging disabled*") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_reason."
                }
                if ([string]$json.friend_mp_sync_transport_next_step -notlike "*manual MP package export/import*" -or
                    [string]$json.friend_mp_sync_transport_next_step -notlike "*strict verify*") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_next_step."
                }
                if ([string]$json.friend_mp_sync_transport_adapter_kind -ne "shared-folder/cloud-folder") {
                    throw "SNC tray status JSON did not expose friend_mp_sync_transport_adapter_kind."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_mp_sync_transport_adapter_path)) {
                    throw "SNC tray status JSON did not expose friend_mp_sync_transport_adapter_path."
                }
                if ([string]$json.friend_mp_sync_transport_adapter_state -ne "disabled_not_implemented") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_adapter_state."
                }
                if ([string]$json.friend_mp_sync_transport_adapter_reason -notlike "*transport adapter is not implemented*" -or
                    [string]$json.friend_mp_sync_transport_adapter_reason -notlike "*upload/send/download/staging disabled*") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_adapter_reason."
                }
                if ([string]$json.friend_mp_sync_transport_adapter_next_step -notlike "*manual MP package export/import*" -or
                    [string]$json.friend_mp_sync_transport_adapter_next_step -notlike "*strict verify*") {
                    throw "SNC tray status JSON did not expose disabled friend_mp_sync_transport_adapter_next_step."
                }
                if ([string]$json.friend_mp_sync_preflight_checklist -notlike "*current MP package ZIP*" -or
                    [string]$json.friend_mp_sync_preflight_checklist -notlike "*friend MP sync envelope metadata*" -or
                    [string]$json.friend_mp_sync_preflight_checklist -notlike "*inbox/outbox plan checks*" -or
                    [string]$json.friend_mp_sync_preflight_checklist -notlike "*automatic sync stays disabled*") {
                    throw "SNC tray status JSON did not expose friend_mp_sync_preflight_checklist."
                }
                if ([string]$json.mp_host_rotation_sync_state -ne "disabled_not_implemented") {
                    throw "SNC tray status JSON did not expose disabled mp_host_rotation_sync_state."
                }
                if ([string]$json.mp_host_rotation_sync_reason -notlike "*host-owned automatic handoff sync for host rotation is not implemented*") {
                    throw "SNC tray status JSON did not expose mp_host_rotation_sync_reason."
                }
                if ([string]$json.mp_host_rotation_sync_next_step -notlike "*manual host rotation handoff*" -or
                    [string]$json.mp_host_rotation_sync_next_step -notlike "*signed/encrypted friend transport*") {
                    throw "SNC tray status JSON did not expose mp_host_rotation_sync_next_step."
                }
                if (-not [string]::IsNullOrWhiteSpace([string]$json.friend_mesh_update_state)) {
                    if (-not ($ReadyOwnerTestFixture -or $PostPlayBackfillFixture -or $MemoryRecoveryFixture)) {
                        if ([string]$json.friend_mesh_update_state -ne "degraded_handoff") {
                            throw "SNC tray status JSON did not expose friend_mesh_update_state."
                        }
                        if ([string]$json.friend_mesh_update_reason -notlike "*previous host unavailable*" -or
                            [string]$json.friend_mesh_update_reason -notlike "*handoff continuity is degraded*") {
                            throw "SNC tray status JSON did not expose friend_mesh_update_reason."
                        }
                        if ([string]$json.friend_mesh_update_next_step -notlike "*nacti nejnovnejsi handoff balicek*" -or
                            [string]$json.friend_mesh_update_next_step -notlike "*confidence snizenou*") {
                            throw "SNC tray status JSON did not expose friend_mesh_update_next_step."
                        }
                    }
                }
                $sncTraySource = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot "src/SncTrayApp.cpp")
                if ($sncTraySource -notlike "*WM_SNC_OPEN_STATUS*" -or
                    $sncTraySource -notlike "*requestExistingInstanceStatusWindow*" -or
                    $sncTraySource -like "*SNC uz bezi*") {
                    throw "SNC tray source did not redirect duplicate launches to the existing status window."
                }
                if ($sncTraySource -notlike "*ID_STATUS_EXPORT_MP_PACKAGE*" -or
                    $sncTraySource -notlike "*MP export*" -or
                    $sncTraySource -notlike "*Rebuilds the multiplayer overlay package from validated local artifacts.*") {
                    throw "SNC tray source did not expose the MP export dashboard action."
                }
                if ($sncTraySource -notlike "*ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE*" -or
                    $sncTraySource -notlike "*SNC MP sync*" -or
                    $sncTraySource -notlike "*Manual metadata only*" -or
                    $sncTraySource -notlike "*verification path then stays safely blocked*") {
                    throw "SNC tray source did not expose the friend MP sync envelope dashboard action."
                }
                if ($sncTraySource -notlike "*ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN*" -or
                    $sncTraySource -notlike "*SNC inbox*" -or
                    $sncTraySource -notlike "*Fail-closed status plan*" -or
                    $sncTraySource -notlike "*Automatic download, decrypt, staging, and package apply do not run*") {
                    throw "SNC tray source did not expose the friend MP sync inbox-plan dashboard action."
                }
                if ($sncTraySource -notlike "*ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN*" -or
                    $sncTraySource -notlike "*SNC outbox*" -or
                    $sncTraySource -notlike "*Automatic upload, send, download, decrypt, staging, and package apply do not run*" -or
                    $sncTraySource -notlike "*does not upload, send, download, or stage gameplay files*") {
                    throw "SNC tray source did not expose the friend MP sync outbox-plan dashboard action."
                }
                if ($summaryText -notlike "*friend_mp_sync_transport_state: Not implemented yet*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_reason: Signed and encrypted friend transport is not implemented yet*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_next_step: Use manual MP export/import and strict verification*") {
                    throw "SNC tray summary text did not expose disabled friend MP sync transport status."
                }
                if ($summaryText -notlike "*friend_mp_sync_transport_adapter_state: Not implemented yet*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_adapter_kind: shared-folder/cloud-folder*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_adapter_path:*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_adapter_reason: Signed and encrypted friend transport is not implemented yet; automatic upload, download, and staging are disabled.*" -or
                    $summaryText -notlike "*friend_mp_sync_transport_adapter_next_step: Use manual MP export/import and strict verification until secure friend transport is implemented.*") {
                    throw "SNC tray summary text did not expose friend MP sync transport adapter seam."
                }
                if ($sncTraySource -notlike "*metadata_verified_transport_not_implemented*" -or
                    $sncTraySource -notlike "*Metadata verified, transport not implemented yet*") {
                    throw "SNC tray source did not keep the metadata_verified_transport_not_implemented translation entry."
                }
                if ($summaryText -notlike "*friend_mp_sync_preflight_checklist: Before a friend MP season*" -or
                    $summaryText -notlike "*run inbox/outbox plan checks with Stellaris closed*") {
                    throw "SNC tray summary text did not expose friend MP sync preflight checklist."
                }
                if ($summaryText -notlike "*friend_trust_store_controls_state: ready*" -or
                    $summaryText -notlike "*friend_trust_store_controls_reason: trusted friends can be revoked, blocked, or have auto-sync disabled through the trust-store update command*" -or
                    $summaryText -notlike "*friend_trust_store_update_command_template: Strategic Nexus.exe --update-snc-friend-trust-store-entry*") {
                    throw "SNC tray summary text did not expose friend trust store control visibility."
                }
                if ($summaryText -notlike "*mp_host_rotation_sync_state: Not implemented yet*" -or
                    $summaryText -notlike "*mp_host_rotation_sync_reason: Host-owned automatic handoff sync for host rotation is not implemented yet*" -or
                    $summaryText -notlike "*mp_host_rotation_sync_next_step: Use the current MP ZIP, strict verify/import, and manual host handoff until signed/encrypted friend transport is implemented.*") {
                    throw "SNC tray summary text did not expose host rotation sync gap."
                }
                if ($summaryText -like "*friend_mesh_update_state:*" -and
                    -not ($ReadyOwnerTestFixture -or $PostPlayBackfillFixture -or $MemoryRecoveryFixture)) {
                    if ($summaryText -notlike "*friend_mesh_update_state: degraded_handoff*" -or
                        $summaryText -notlike "*friend_mesh_update_reason: previous host unavailable; handoff continuity is degraded*" -or
                        $summaryText -notlike "*friend_mesh_update_next_step: Load the newest handoff package if available; otherwise use an older verified archive or client save and keep confidence reduced.*") {
                        throw "SNC tray summary text did not expose friend mesh update state."
                    }
                }
                if ($summaryText -notlike "*mp_sdileni_tip: zkopiruj mp_package_zip_path a mp_package_manifest_hash; host/client kroky jsou nize.*") {
                    throw "SNC tray summary text did not expose MP export/share guidance."
                }
                if ([string]::IsNullOrWhiteSpace([string]$json.friend_pairing_guide_text)) {
                    throw "SNC tray status JSON did not expose friend_pairing_guide_text."
                }
                if ([string]$json.friend_pairing_guide_text -notlike "*create request*friend verifies fingerprint*import acceptance into local trust store*" -or
                    [string]$json.friend_pairing_guide_text -notlike "*manual MP package export/import*" -or
                    [string]$json.friend_pairing_guide_text -notlike "*strict verify/import*" -or
                    [string]$json.friend_pairing_guide_text -notlike "*Auto-sync stays disabled until signed/encrypted transport is active*") {
                    throw "SNC tray friend_pairing_guide_text did not expose the manual flow, fallback, and auto-sync safety state."
                }
                if ($summaryText -notlike "*memory_recovery:*") {
                    throw "SNC tray summary text did not expose memory recovery state."
                }
                if ($summaryText -notlike "*memory_recovery_confidence:*") {
                    throw "SNC tray summary text did not expose memory recovery confidence."
                }
                if ($summaryText -notlike "*human_control_guard_state:*") {
                    throw "SNC tray summary text did not expose human_control_guard_state."
                }
                if ($mpPackageReady -and $summaryText -notlike "*mp_previous_host_available:*") {
                    throw "SNC tray summary text did not expose mp_previous_host_available."
                }
                if ($mpPackageReady -and $summaryText -notlike "*mp_previous_host_available_known:*") {
                    throw "SNC tray summary text did not expose mp_previous_host_available_known."
                }
                if ($mpPackageReady -and $summaryText -like "*mp_handoff_status: Degraded: previous host unavailable*" -and
                    $summaryText -notlike "*mp_handoff_recovery_hint:*") {
                    throw "SNC tray summary text did not expose mp_handoff_recovery_hint for degraded MP handoff continuity."
                }
                if (-not (Test-Path -LiteralPath $json.next_steps_brief_path)) {
                    throw "SNC tray did not write the next-steps brief named by status JSON."
                }
                $briefText = Get-Content -Raw -LiteralPath $json.next_steps_brief_path
                if ($briefText -notlike "*Human control guard:*") {
                    throw "SNC tray next-steps brief did not expose human control guard."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    $briefText -notlike "*Publish gate available: ano*") {
                    throw "SNC tray next-steps brief does not match publish gate availability from status JSON."
                }
                if ($mpPackageReady -and $briefText -notlike "*MP previous host available:*") {
                    throw "SNC tray next-steps brief did not expose MP previous host available."
                }
                if ($mpPackageReady -and $briefText -notlike "*MP previous host availability known:*") {
                    throw "SNC tray next-steps brief did not expose MP previous host availability known."
                }
                if ($mpPackageReady -and $summaryText -like "*campaign_library_limit_reached: true*" -and
                    $briefText -notlike "*campaign_library_pin_state: unavailable*") {
                    throw "SNC tray next-steps brief did not expose the pinned-campaign placeholder state."
                }
                if ($mpPackageReady -and $summaryText -like "*mp_handoff_status: Degraded: previous host unavailable*" -and
                    $briefText -notlike "*MP recovery:*") {
                    throw "SNC tray next-steps brief did not expose MP recovery guidance for degraded handoff continuity."
                }
                if ($briefText -notlike "*Startup note:*") {
                    throw "SNC tray next-steps brief did not expose the startup rationale note."
                }
                if ($briefText -notlike "*Start with Windows: volitelne nastaveni, vychozi stav ma zustat vypnuty.*") {
                    throw "SNC tray next-steps brief did not expose the default-disabled start-with-Windows note."
                }
                if ($briefText -notlike "*Startup lifecycle state: $($expectedStartup.LifecycleBrief).*") {
                    throw "SNC tray next-steps brief did not expose the expected startup lifecycle state."
                }
                if ($briefText -notlike "*Startup state source: $($expectedStartup.Source).*") {
                    throw "SNC tray next-steps brief did not expose the startup state source."
                }
                if ($briefText -notlike "*Startup shortcut state: $($expectedStartup.ShortcutStateBrief).*") {
                    throw "SNC tray next-steps brief did not expose the startup shortcut state."
                }
                if (-not [string]::IsNullOrWhiteSpace($expectedStartupShortcutPathText) -and
                    $briefText -notlike "*Startup shortcut path: $expectedStartupShortcutPathText*") {
                    throw "SNC tray next-steps brief did not expose the startup shortcut path."
                }
                if (-not [string]::IsNullOrWhiteSpace([string]$expectedStartup.CommandHint) -and
                    $briefText -notlike "*Startup command hint: $($expectedStartup.CommandHint)*") {
                    throw "SNC tray next-steps brief did not expose the startup command hint."
                }
                if ($briefText -notlike "*Startup command hint source: $($expectedStartup.CommandHintSource)*") {
                    throw "SNC tray next-steps brief did not expose the startup command-hint source."
                }
                if ($briefText -notlike "*Startup enable command: $($expectedStartup.EnableCommandHint)*") {
                    throw "SNC tray next-steps brief did not expose the startup enable command."
                }
                if ($briefText -notlike "*Startup disable command: $($expectedStartup.DisableCommandHint)*") {
                    throw "SNC tray next-steps brief did not expose the startup disable command."
                }
                if ($briefText -notlike "*Support report state: Not prepared yet*") {
                    throw "SNC tray next-steps brief did not expose support report state."
                }
                if ($briefText -notlike "*Support report send approval required: ano*") {
                    throw "SNC tray next-steps brief did not expose support report approval requirement."
                }
                if ($briefText -notlike "*Support report raw saves included: ne*") {
                    throw "SNC tray next-steps brief did not expose support report raw-save exclusion."
                }
                if ($briefText -notlike "*Support report contact: support@jeniksoft.cz*") {
                    throw "SNC tray next-steps brief did not expose support report contact."
                }
                if ($briefText -notlike "*Support report preview: $expectedSupportReportPreviewPathText*") {
                    throw "SNC tray next-steps brief did not expose support report preview path."
                }
                if ($briefText -notlike "*Support report prepare command: powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_snc_support_report.ps1*") {
                    throw "SNC tray next-steps brief did not expose support report prepare command."
                }
                if ($briefText -notlike "*Friend mesh update state:*") {
                    throw "SNC tray next-steps brief did not expose friend mesh update state."
                }
                if ($briefText -notlike "*Friend mesh update reason:*") {
                    throw "SNC tray next-steps brief did not expose friend mesh update reason."
                }
                if ($briefText -notlike "*Friend mesh update next step:*") {
                    throw "SNC tray next-steps brief did not expose friend mesh update next step."
                }
                if ($briefText -notlike "*MP host rotation sync state: Not implemented yet*") {
                    throw "SNC tray next-steps brief did not expose MP host rotation sync state."
                }
                if ($briefText -notlike "*MP host rotation sync reason: Host-owned automatic handoff sync for host rotation is not implemented yet; the fallback is manual MP export/import.*") {
                    throw "SNC tray next-steps brief did not expose MP host rotation sync reason."
                }
                if ($briefText -notlike "*MP host rotation sync next step: Use the current MP ZIP, strict verify/import, and manual host handoff until signed/encrypted friend transport is implemented.*") {
                    throw "SNC tray next-steps brief did not expose MP host rotation sync next step."
                }
                if ($briefText -notlike "*SNC friend pairing command template:*--create-snc-friend-request*" -or
                    $briefText -notlike "*--create-snc-friend-acceptance*" -or
                    $briefText -notlike "*--import-snc-friend-acceptance*") {
                    throw "SNC tray next-steps brief did not expose the manual SNC friend-pairing command template."
                }
                if ($briefText -notlike "*SNC friend pairing guide:*create request*friend verifies fingerprint*import acceptance into local trust store*") {
                    throw "SNC tray next-steps brief did not expose the manual SNC friend-pairing guide."
                }
                if ($briefText -notlike "*SNC friend pairing auto-sync: vypnuto, dokud neni hotovy signed/encrypted transport.*") {
                    throw "SNC tray next-steps brief did not expose the friend-pairing auto-sync safety state."
                }
                if ($ReadyOwnerTestFixture -and
                    $briefText -notlike "*Owner test playbook: docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md*") {
                    throw "SNC tray ready-state next-steps brief did not expose the owner-test playbook path."
                }
                if (-not $ReadyOwnerTestFixture -and
                    $briefText -like "*Owner test playbook:*") {
                    throw "SNC tray non-ready baseline should keep the owner-test playbook path hidden in the next-steps brief."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    $briefText -notlike "*Command hint: *--publish-snc-generated-overlay*") {
                    throw "SNC tray publish-ready brief did not expose the concrete publish command hint."
                }
                & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "tools\prepare_snc_support_report.ps1") -StatusPath $statusPath -OutputPath $supportReportPreviewPath | Out-Null
                if (-not (Test-Path -LiteralPath $supportReportPreviewPath)) {
                    throw "SNC tray smoke did not create the local support report preview."
                }
                $supportReportDeadline = (Get-Date).AddSeconds(10)
                $updatedJson = $null
                do {
                    $updatedJson = Get-Content -Raw -LiteralPath $statusPath | ConvertFrom-Json
                    if ([string]$updatedJson.support_report_state -eq "ready_for_review") {
                        break
                    }
                    Start-Sleep -Milliseconds 250
                } while ((Get-Date) -lt $supportReportDeadline)
                if ([string]$updatedJson.support_report_state -ne "ready_for_review") {
                    throw "SNC tray status JSON did not refresh support_report_state=ready_for_review after preview generation."
                }
                if ([string]$updatedJson.support_report_open_command_hint -notlike "cmd /c start*") {
                    throw "SNC tray status JSON did not expose support_report_open_command_hint after preview generation."
                }
                $supportReportPreviewText = Get-Content -Raw -LiteralPath $supportReportPreviewPath
                if ($supportReportPreviewText -notlike "*support@jeniksoft.cz*") {
                    throw "SNC support report preview did not include the support contact."
                }
                if ($supportReportPreviewText -notlike "*explicit owner approval required*") {
                    throw "SNC support report preview did not include the explicit approval note."
                }
                if ($supportReportPreviewText -notlike "*Raw saves included: no*") {
                    throw "SNC support report preview did not state that raw saves stay excluded by default."
                }
                Write-Host "snc_tray_smoke_success=true"
                Write-Host ("snc_tray_smoke_state=" + $json.state)
                Write-Host ("snc_tray_smoke_next_action=" + $json.next_action)
                Write-Host ("snc_tray_smoke_next_action_command_hint_source=" + $json.next_action_command_hint_source)
                Write-Host ("snc_tray_smoke_next_action_path=" + $json.next_action_path)
                Write-Host ("snc_tray_smoke_owner_test_playbook_path=" + $ownerTestPlaybookPath)
                Write-Host ("snc_tray_smoke_process_id=" + $process.Id)
                exit 0
            }
        }
        Start-Sleep -Milliseconds 250
    }

    throw "SNC tray did not write a valid heartbeat within 10 seconds."
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $process.Id -Timeout 5 -ErrorAction SilentlyContinue
    }
    if ($ReadyOwnerTestFixture -or $PostPlayBackfillFixture -or $MemoryRecoveryFixture) {
        Restore-BackedUpPaths -State $fixtureBackups
        if ($null -ne $fixtureBackupRoot -and (Test-Path -LiteralPath $fixtureBackupRoot)) {
            Remove-Item -LiteralPath $fixtureBackupRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    if ($null -ne $supportReportBackupPath -and (Test-Path -LiteralPath $supportReportBackupPath)) {
        Move-Item -LiteralPath $supportReportBackupPath -Destination $supportReportPreviewPath -Force
    }
    if ($null -eq $previousSncUiLanguage) {
        Remove-Item Env:SNC_UI_LANGUAGE -ErrorAction SilentlyContinue
    } else {
        $env:SNC_UI_LANGUAGE = $previousSncUiLanguage
    }
}
