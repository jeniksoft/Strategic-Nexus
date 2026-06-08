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
        weighted_progress_percent = [int]$analysis.weighted_progress_percent
        implementation_complete_percent = [int]$analysis.implementation_complete_percent
        implementation_open_percent = [int]$analysis.implementation_open_percent
        implementation_open_complexity_points = [double]$analysis.implementation_open_complexity_points
        in_progress_complexity_percent = [int]$analysis.in_progress_complexity_percent
        in_progress_complexity_points = [int]$analysis.in_progress_complexity_points
        verification_complete_percent = [int]$analysis.verification_complete_percent
        implemented_percent = [int]$analysis.implemented_percent
        verified_percent = [int]$analysis.verified_percent
        implemented_item_percent = [int]$analysis.implemented_item_percent
        verified_item_percent = [int]$analysis.verified_item_percent
        remaining = [string]$analysis.remaining
        confidence = [string]$analysis.confidence
        basis = [string]$analysis.basis
        item_count = [int]$analysis.item_count
        total_complexity_points = [int]$analysis.total_complexity_points
        completed_complexity_points = [double]$analysis.completed_complexity_points
        remaining_complexity_points = [double]$analysis.remaining_complexity_points
        implemented_complexity_points = [double]$analysis.implemented_complexity_points
        verified_complexity_points = [double]$analysis.verified_complexity_points
        implementation_basis = [string]$analysis.implementation_basis
        owner_summary = [string]$analysis.owner_summary
        implemented_item_count = [int]$analysis.implemented_item_count
        verified_item_count = [int]$analysis.verified_item_count
        status_breakdown = @($analysis.status_breakdown)
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
        weighted_progress_percent = $Percent
        implementation_complete_percent = $Percent
        implementation_open_percent = 100 - $Percent
        implementation_open_complexity_points = $null
        in_progress_complexity_percent = $null
        in_progress_complexity_points = $null
        verification_complete_percent = $null
        implemented_percent = $null
        verified_percent = $null
        implemented_item_percent = $null
        verified_item_percent = $null
        remaining = $Remaining
        confidence = $Confidence
        basis = $Basis
        item_count = $null
        total_complexity_points = $null
        completed_complexity_points = $null
        remaining_complexity_points = $null
        implemented_complexity_points = $null
        verified_complexity_points = $null
        implementation_basis = "manualni odhad"
        owner_summary = "implementace $Percent% | postup $Percent%"
        implemented_item_count = $null
        verified_item_count = $null
        status_breakdown = @()
    }
} else {
    $estimate = Get-AutomaticProjectProgressEstimate -RoadmapPath $roadmapPath
}

$directory = Split-Path -Parent $progressPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$outputJson = [pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    percent = $estimate.percent
    weighted_progress_percent = $estimate.weighted_progress_percent
    implementation_complete_percent = $estimate.implementation_complete_percent
    implementation_open_percent = $estimate.implementation_open_percent
    implementation_open_complexity_points = $estimate.implementation_open_complexity_points
    in_progress_complexity_percent = $estimate.in_progress_complexity_percent
    in_progress_complexity_points = $estimate.in_progress_complexity_points
    verification_complete_percent = $estimate.verification_complete_percent
    implemented_percent = $estimate.implemented_percent
    verified_percent = $estimate.verified_percent
    implemented_item_percent = $estimate.implemented_item_percent
    verified_item_percent = $estimate.verified_item_percent
    remaining = $estimate.remaining
    confidence = $estimate.confidence
    basis = $estimate.basis
    item_count = $estimate.item_count
    total_complexity_points = $estimate.total_complexity_points
    completed_complexity_points = $estimate.completed_complexity_points
    remaining_complexity_points = $estimate.remaining_complexity_points
    implemented_complexity_points = $estimate.implemented_complexity_points
    verified_complexity_points = $estimate.verified_complexity_points
    implementation_basis = $estimate.implementation_basis
    owner_summary = $estimate.owner_summary
    implemented_item_count = $estimate.implemented_item_count
    verified_item_count = $estimate.verified_item_count
    status_breakdown = @($estimate.status_breakdown)
} | ConvertTo-Json -Depth 8

[System.IO.File]::WriteAllText(
    $progressPath,
    $outputJson + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "project_progress_estimate_written=$progressPath"
Write-Host "project_progress_percent=$($estimate.percent)"
Write-Host "project_progress_weighted_percent=$($estimate.weighted_progress_percent)"
Write-Host "project_progress_implementation_complete_percent=$($estimate.implementation_complete_percent)"
Write-Host "project_progress_implemented_percent=$($estimate.implemented_percent)"
Write-Host "project_progress_verified_percent=$($estimate.verified_percent)"
Write-Host "project_progress_basis=$($estimate.basis)"
