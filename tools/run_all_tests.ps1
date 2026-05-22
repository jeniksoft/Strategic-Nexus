$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

Push-Location $repoRoot
try {
    & (Join-Path $PSScriptRoot "run_safety_audit.ps1")
    & (Join-Path $PSScriptRoot "run_bridge_core_tests.ps1")
    & (Join-Path $PSScriptRoot "run_v0_pipeline_tests.ps1")
} finally {
    Pop-Location
}

Write-Host "Strategic Nexus local test suite passed."
