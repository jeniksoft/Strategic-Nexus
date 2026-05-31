$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $repoRoot "dist\private_tools\StrategicNexusCompanionTray.exe"

if (-not (Test-Path -LiteralPath $exePath)) {
    & (Join-Path $PSScriptRoot "build_snc_tray.ps1")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$existing = Get-Process -Name "StrategicNexusCompanionTray" -ErrorAction SilentlyContinue |
    Select-Object -First 1

if ($existing) {
    Write-Host "snc_tray_already_running=true"
    Write-Host ("snc_tray_process_id=" + $existing.Id)
    exit 0
}

$process = Start-Process -FilePath $exePath -WorkingDirectory $repoRoot -PassThru
Write-Host "snc_tray_started=true"
Write-Host ("snc_tray_process_id=" + $process.Id)
Write-Host ("snc_tray_exe=" + $exePath)
