$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"
$statusPath = Join-Path $repoRoot "dist\private_reports\snc_tray_status.json"

& (Join-Path $PSScriptRoot "build_snc_tray.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$existing = Get-Process -Name "StrategicNexusCompanionTray" -ErrorAction SilentlyContinue
if ($existing) {
    throw "SNC tray is already running; stop it before smoke testing so the test does not close the owner instance."
}

Remove-Item -Force -LiteralPath $statusPath -ErrorAction SilentlyContinue
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
                $null -ne $json.status_center_summary_text -and
                $null -ne $json.next_action -and
                $null -ne $json.next_action_command_hint_source -and
                $null -ne $json.next_action_path -and
                $null -ne $json.next_steps_brief_path -and
                $null -ne $json.generated_overlay_publish_gate_state -and
                $null -ne $json.generated_overlay_publish_gate_reason -and
                $null -ne $json.generated_overlay_publish_gate_published -and
                $null -ne $json.generated_overlay_publish_gate_can_publish -and
                $null -ne $json.mp_package_refresh_state -and
                $null -ne $json.mp_overlay_package_directory -and
                $null -ne $json.mp_overlay_package_state
            ) {
                if ($null -eq $json.mp_overlay_package_previous_host_available) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available."
                }
                if ($null -eq $json.mp_overlay_package_previous_host_available_known) {
                    throw "SNC tray status JSON did not expose mp_overlay_package_previous_host_available_known."
                }
                if ($json.generated_overlay_publish_gate_can_publish -eq $true -and
                    $json.next_action -ne "review_staged_overlay_and_publish_if_desired") {
                    throw "SNC tray publish-ready status did not surface the publish/review next action."
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
                if ($summaryText -notlike "*mp_previous_host_available:*") {
                    throw "SNC tray summary text did not expose mp_previous_host_available."
                }
                if ($summaryText -notlike "*mp_previous_host_available_known:*") {
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
                if ($briefText -notlike "*MP previous host available:*") {
                    throw "SNC tray next-steps brief did not expose MP previous host available."
                }
                if ($briefText -notlike "*MP previous host availability known:*") {
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
}
