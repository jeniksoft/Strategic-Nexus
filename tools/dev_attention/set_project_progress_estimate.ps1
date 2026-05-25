param(
    [int]$Percent = -1,
    [string]$Remaining = "",
    [string]$Confidence = "",
    [string]$Basis = "",
    [string]$ProjectProgressFilePath = "dist/private_reports/project_progress_estimate.json",
    [string]$RoadmapFilePath = "docs/DEVELOPMENT_ROADMAP.md",
    [string]$RoadmapComplexityOutputPath = "dist/private_reports/roadmap_complexity_estimate.json"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

function Get-RemainingEstimate {
    param([int]$ProgressPercent)

    if ($ProgressPercent -lt 20) {
        return "6-12 tydnu Freework + testovani"
    }
    if ($ProgressPercent -lt 40) {
        return "5-10 tydnu Freework + testovani"
    }
    if ($ProgressPercent -lt 60) {
        return "3-7 tydnu Freework + testovani"
    }
    if ($ProgressPercent -lt 80) {
        return "2-5 tydnu Freework + testovani"
    }
    if ($ProgressPercent -lt 95) {
        return "1-3 tydny doladeni/testovani"
    }
    return "finalni doladeni"
}

function Get-AutomaticProjectProgressEstimate {
    param([string]$RoadmapPath)

    $analysisScript = Join-Path $PSScriptRoot "analyze_roadmap_complexity.ps1"
    $analysisOutputPath = Resolve-ProjectPath $RoadmapComplexityOutputPath
    & powershell -NoProfile -ExecutionPolicy Bypass -File $analysisScript -RoadmapFilePath $RoadmapPath -OutputPath $analysisOutputPath | Out-Null
    $analysis = Get-Content -Raw -LiteralPath $analysisOutputPath | ConvertFrom-Json

    return [pscustomobject]@{
        percent = [int]$analysis.percent
        remaining = [string]$analysis.remaining
        confidence = [string]$analysis.confidence
        basis = [string]$analysis.basis
    }
}

$progressPath = Resolve-ProjectPath $ProjectProgressFilePath
$roadmapPath = Resolve-ProjectPath $RoadmapFilePath

if ($Percent -ge 0) {
    if ($Percent -gt 100) {
        throw "Percent must be between 0 and 100."
    }
    if ([string]::IsNullOrWhiteSpace($Remaining)) {
        $Remaining = Get-RemainingEstimate -ProgressPercent $Percent
    }
    if ([string]::IsNullOrWhiteSpace($Confidence)) {
        $Confidence = "nizka"
    }
    if (@("nizka", "stredni", "vysoka") -notcontains $Confidence) {
        throw "Confidence must be one of: nizka, stredni, vysoka."
    }
    if ([string]::IsNullOrWhiteSpace($Basis)) {
        $Basis = "manualni odhad"
    }
    $estimate = [pscustomobject]@{
        percent = $Percent
        remaining = $Remaining
        confidence = $Confidence
        basis = $Basis
    }
} else {
    $estimate = Get-AutomaticProjectProgressEstimate -RoadmapPath $roadmapPath
}

$directory = Split-Path -Parent $progressPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

[pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    percent = $estimate.percent
    remaining = $estimate.remaining
    confidence = $estimate.confidence
    basis = $estimate.basis
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $progressPath -Encoding UTF8

Write-Host "project_progress_estimate_written=$progressPath"
Write-Host "project_progress_percent=$($estimate.percent)"
Write-Host "project_progress_basis=$($estimate.basis)"
