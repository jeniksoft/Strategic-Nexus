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
                $null -ne $json.next_steps_brief_path -and
                $null -ne $json.mp_package_refresh_state -and
                $null -ne $json.mp_overlay_package_directory -and
                $null -ne $json.mp_overlay_package_state
            ) {
                Write-Host "snc_tray_smoke_success=true"
                Write-Host ("snc_tray_smoke_state=" + $json.state)
                Write-Host ("snc_tray_smoke_next_action=" + $json.next_action)
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
