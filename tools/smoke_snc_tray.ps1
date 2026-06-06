param(
    [switch]$ReadyOwnerTestFixture
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"
$statusPath = Join-Path $repoRoot "dist\private_reports\snc_tray_status.json"
$liveStatusPath = Join-Path $repoRoot "dist\private_reports\snc_live_autosave_monitor_status.json"
$nextStepsBriefPath = Join-Path $repoRoot "dist\private_reports\snc_next_steps_brief.txt"
$privateReportsRoot = Join-Path $repoRoot "dist\private_reports"
$activeOverlayDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_active"
$stagedOverlayDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_staged"
$stagingStatusPath = Join-Path $privateReportsRoot "snc_generated_overlay_staging_status.json"
$publishStatusPath = Join-Path $privateReportsRoot "snc_generated_overlay_publish_status.json"
$publishBackupRootDirectory = Join-Path $privateReportsRoot "snc_generated_overlay_backups"
$gameplayAcceptanceReportPath = Join-Path $privateReportsRoot "generated_overlay_gameplay_acceptance_v0.json"
$mpOverlayPackageDirectory = Join-Path $privateReportsRoot "snc_mp_overlay_package"
$mpOverlayPackageZipPath = Join-Path $privateReportsRoot "snc_mp_overlay_package.zip"

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

Remove-Item -Force -LiteralPath $statusPath -ErrorAction SilentlyContinue
Remove-Item -Force -LiteralPath $liveStatusPath -ErrorAction SilentlyContinue

$fixtureBackupRoot = $null
$fixtureBackups = $null
if ($ReadyOwnerTestFixture) {
    $fixtureBackupRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("snc-tray-smoke-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $fixtureBackupRoot | Out-Null
    $fixtureBackups = Initialize-ReadyOwnerTestFixture -BackupRoot $fixtureBackupRoot
}

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
                $null -ne $json.window_close_behavior -and
                $null -ne $json.explicit_exit_behavior -and
                $null -ne $json.crash_restart_policy -and
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
                $null -ne $json.mp_package_refresh_state -and
                $null -ne $json.mp_overlay_package_directory -and
                $null -ne $json.mp_overlay_package_state
            ) {
                if ($json.window_close_behavior -ne "minimize_to_tray") {
                    throw "SNC tray status JSON did not expose window_close_behavior=minimize_to_tray."
                }
                if ($json.explicit_exit_behavior -ne "stop_without_restart") {
                    throw "SNC tray status JSON did not expose explicit_exit_behavior=stop_without_restart."
                }
                if ($json.crash_restart_policy -ne "bounded_backoff_with_crash_loop_guard") {
                    throw "SNC tray status JSON did not expose crash_restart_policy=bounded_backoff_with_crash_loop_guard."
                }
                $mpPackageReady = ([string]$json.mp_overlay_package_state -eq "ready")
                if (-not $ReadyOwnerTestFixture -and $mpPackageReady -and $null -eq $json.mp_overlay_package_previous_host_available) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available."
                }
                if (-not $ReadyOwnerTestFixture -and $mpPackageReady -and $null -eq $json.mp_overlay_package_previous_host_available_known) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available_known."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    $json.next_action -ne "review_staged_overlay_and_publish_if_desired") {
                    throw "SNC tray publish-ready status did not surface the publish/review next action."
                }
                $ownerTestPlaybookPath = [string]$json.owner_test_playbook_path
                if ($ReadyOwnerTestFixture) {
                    if ($ownerTestPlaybookPath -ne "docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md") {
                        throw "SNC tray ready-state fixture did not expose the stable owner_test_playbook_path field."
                    }
                    if ([string]$json.generated_overlay_publish_gate_state -ne "published") {
                        throw "SNC tray ready-state fixture did not expose generated_overlay_publish_gate_state=published."
                    }
                } elseif (-not [string]::IsNullOrWhiteSpace($ownerTestPlaybookPath)) {
                    throw "SNC tray smoke expected owner_test_playbook_path to stay empty in the non-ready baseline state."
                }
                $summaryText = [string]$json.status_center_summary_text
                if ($summaryText -notlike "*startup_rationale:*") {
                    throw "SNC tray summary text did not expose startup_rationale."
                }
                if ($summaryText -notlike "*startup_start_with_windows: optional_owner_setting_default_disabled*") {
                    throw "SNC tray summary text did not expose the default-disabled startup contract."
                }
                if ($summaryText -notlike "*startup_lifecycle_state: manual_start_only*") {
                    throw "SNC tray summary text did not expose the startup lifecycle state."
                }
                if ($summaryText -notlike "*startup_start_with_windows_enabled: false*") {
                    throw "SNC tray summary text did not expose the current start-with-Windows enabled flag."
                }
                if ($mpPackageReady -and $summaryText -notlike "*mp_previous_host_available:*") {
                    throw "SNC tray summary text did not expose mp_previous_host_available."
                }
                if ($mpPackageReady -and $summaryText -notlike "*mp_previous_host_available_known:*") {
                    throw "SNC tray summary text did not expose mp_previous_host_available_known."
                }
                if (-not (Test-Path -LiteralPath $json.next_steps_brief_path)) {
                    throw "SNC tray did not write the next-steps brief named by status JSON."
                }
                $briefText = Get-Content -Raw -LiteralPath $json.next_steps_brief_path
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
                if ($briefText -notlike "*Startup note:*") {
                    throw "SNC tray next-steps brief did not expose the startup rationale note."
                }
                if ($briefText -notlike "*Start with Windows: volitelne nastaveni, vychozi stav ma zustat vypnuty.*") {
                    throw "SNC tray next-steps brief did not expose the default-disabled start-with-Windows note."
                }
                if ($briefText -notlike "*Startup lifecycle state: manual start only (start with Windows disabled).*") {
                    throw "SNC tray next-steps brief did not expose the current startup lifecycle state."
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
    if ($ReadyOwnerTestFixture) {
        Restore-BackedUpPaths -State $fixtureBackups
        if ($null -ne $fixtureBackupRoot -and (Test-Path -LiteralPath $fixtureBackupRoot)) {
            Remove-Item -LiteralPath $fixtureBackupRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
