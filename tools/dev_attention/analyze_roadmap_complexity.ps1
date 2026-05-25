param(
    [string]$RoadmapFilePath = "docs/DEVELOPMENT_ROADMAP.md",
    [string]$WorkLogPath = ".codex_local/implementation_work_log.csv",
    [string]$OutputPath = "dist/private_reports/roadmap_complexity_estimate.json"
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

function Get-StatusProgress {
    param([string]$Status)

    switch ($Status) {
        "VERIFIED" { return 100 }
        "IMPLEMENTED" { return 70 }
        "IN_PROGRESS" { return 35 }
        "BLOCKED" { return 5 }
        "NOT_STARTED" { return 0 }
        default { return 0 }
    }
}

function Get-RoadmapItemComplexity {
    param(
        [string]$Title,
        [string]$Body
    )

    $requiredBlock = ""
    $requiredMatch = [regex]::Match($Body, "(?ms)^Required:\s*\r?\n(?<required>.*?)(^Notes:|^Current progress:|^Planned requirement:|^---)")
    if ($requiredMatch.Success) {
        $requiredBlock = $requiredMatch.Groups["required"].Value
    }

    $requiredCount = [regex]::Matches($requiredBlock, "(?m)^\s*\*\s+").Count
    $currentProgressCount = [regex]::Matches($Body, "(?m)^Current progress:").Count

    $points = 3 + $requiredCount

    if ($Title -match "(?i)(multiplayer|runtime|memory|personality|empire|orchestrator|overlay distribution)") {
        $points += 4
    }
    if ($Title -match "(?i)(advanced)") {
        $points += 6
    }
    if ($currentProgressCount -gt 0) {
        $points += 1
    }

    return [pscustomobject]@{
        points = [int][Math]::Max(2, [Math]::Min(30, $points))
        required_count = $requiredCount
    }
}

function Get-RemainingEstimate {
    param(
        [double]$RemainingPoints,
        [double]$PointsPerHour
    )

    if ($PointsPerHour -gt 0) {
        $hours = [Math]::Ceiling($RemainingPoints / $PointsPerHour)
        if ($hours -lt 10) {
            return "cca $hours hodin + testovani"
        }
        $weeksLow = [Math]::Max(1, [Math]::Floor($hours / 10))
        $weeksHigh = [Math]::Max($weeksLow + 1, [Math]::Ceiling($hours / 5))
        return "cca $weeksLow-$weeksHigh tydnu podle tempa"
    }

    if ($RemainingPoints -gt 120) { return "6-12 tydnu Freework + testovani" }
    if ($RemainingPoints -gt 80) { return "5-10 tydnu Freework + testovani" }
    if ($RemainingPoints -gt 45) { return "3-7 tydnu Freework + testovani" }
    if ($RemainingPoints -gt 20) { return "2-5 tydnu Freework + testovani" }
    return "1-3 tydny doladeni/testovani"
}

$roadmapPath = Resolve-ProjectPath $RoadmapFilePath
$workLogResolvedPath = Resolve-ProjectPath $WorkLogPath
$outputResolvedPath = Resolve-ProjectPath $OutputPath

if (-not (Test-Path -LiteralPath $roadmapPath)) {
    throw "Roadmap file not found: $roadmapPath"
}

$text = Get-Content -Raw -LiteralPath $roadmapPath
$matches = [regex]::Matches(
    $text,
    "(?ms)^##\s+(?<number>[0-9]+[A-Z]?)\.\s+(?<title>[^\r\n]+)(?<body>.*?)(?=^##\s+[0-9]+[A-Z]?\.\s+|\z)")

$items = @()
$totalPoints = 0.0
$donePoints = 0.0
$statusCounts = @{}

foreach ($match in $matches) {
    $title = [string]$match.Groups["title"].Value
    $body = [string]$match.Groups["body"].Value
    $statusMatch = [regex]::Match($body, "(?m)^Status:\s*\r?\n(?<status>[A-Z_]+)")
    if (-not $statusMatch.Success) {
        continue
    }

    $status = [string]$statusMatch.Groups["status"].Value
    $progress = Get-StatusProgress -Status $status
    $complexity = Get-RoadmapItemComplexity -Title $title -Body $body
    $points = [double]$complexity.points
    $done = $points * ($progress / 100.0)

    $totalPoints += $points
    $donePoints += $done
    if (-not $statusCounts.ContainsKey($status)) {
        $statusCounts[$status] = 0
    }
    $statusCounts[$status]++

    $items += [pscustomobject]@{
        id = [string]$match.Groups["number"].Value
        title = $title
        status = $status
        status_progress_percent = $progress
        complexity_points = [int]$points
        completed_points = [Math]::Round($done, 2)
        required_count = $complexity.required_count
    }
}

if ($totalPoints -le 0) {
    throw "No analyzable roadmap items found."
}

$loggedRows = @()
if (Test-Path -LiteralPath $workLogResolvedPath) {
    $loggedRows = @(Import-Csv -LiteralPath $workLogResolvedPath | Where-Object {
        $_.duration_minutes -and $_.complexity_points_done -and [double]$_.duration_minutes -gt 0 -and [double]$_.complexity_points_done -gt 0
    })
}

$loggedMinutes = 0.0
$loggedPoints = 0.0
foreach ($row in $loggedRows) {
    $loggedMinutes += [double]$row.duration_minutes
    $loggedPoints += [double]$row.complexity_points_done
}

$pointsPerHour = 0.0
if ($loggedMinutes -gt 0) {
    $pointsPerHour = $loggedPoints / ($loggedMinutes / 60.0)
}

$percent = [int][Math]::Round(($donePoints / $totalPoints) * 100.0)
$remainingPoints = [Math]::Max(0, $totalPoints - $donePoints)
$statusSummary = (($statusCounts.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ", ")
$confidence = if ($loggedRows.Count -ge 5) { "stredni" } else { "nizka" }

$directory = Split-Path -Parent $outputResolvedPath
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

[pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    percent = $percent
    remaining = Get-RemainingEstimate -RemainingPoints $remainingPoints -PointsPerHour $pointsPerHour
    confidence = $confidence
    basis = "automaticky vazeny odhad: $([int]$totalPoints) bodu roadmapy, $([Math]::Round($donePoints, 1)) hotovo, $([Math]::Round($remainingPoints, 1)) zbyva; $statusSummary"
    total_complexity_points = [int]$totalPoints
    completed_complexity_points = [Math]::Round($donePoints, 2)
    remaining_complexity_points = [Math]::Round($remainingPoints, 2)
    logged_work_rows = $loggedRows.Count
    logged_minutes = [int][Math]::Round($loggedMinutes)
    logged_points_done = [Math]::Round($loggedPoints, 2)
    points_per_hour = [Math]::Round($pointsPerHour, 2)
    items = @($items)
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputResolvedPath -Encoding UTF8

Write-Host "roadmap_complexity_estimate_written=$outputResolvedPath"
Write-Host "roadmap_progress_percent=$percent"
Write-Host "roadmap_complexity_points=$([int]$totalPoints)"
