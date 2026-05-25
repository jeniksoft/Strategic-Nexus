param(
    [string]$WorkLogPath = ".codex_local/codex_work_log.csv",
    [string]$OutputPath = "dist/private_reports/maintenance_balance.json",
    [int]$LookbackDays = 7
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

function Get-EntryDurationMinutes {
    param($Entry)

    try {
        $start = [DateTime]::ParseExact([string]$Entry.start_local, "yyyy-MM-dd HH:mm", [System.Globalization.CultureInfo]::InvariantCulture)
        $end = [DateTime]::ParseExact([string]$Entry.end_local, "yyyy-MM-dd HH:mm", [System.Globalization.CultureInfo]::InvariantCulture)
        return [Math]::Max(0, ($end - $start).TotalMinutes)
    } catch {
        return 0
    }
}

function Classify-WorkEntry {
    param($Entry)

    $text = (([string]$Entry.summary) + " " + ([string]$Entry.notes)).ToLowerInvariant()

    $implementationPatterns = @(
        "autosave", "archive", "generated overlay", "dsl", "parser", "validator",
        "verifier", "manifest", "season", "empire brief", "ledger", "compiler",
        "pipeline", "test", "regression", "src/", "tests/", "c\\+\\+", "companion",
        "orchestrator", "runtime", "save"
    )
    foreach ($pattern in $implementationPatterns) {
        if ($text -match $pattern) {
            return "implementation"
        }
    }

    $maintenancePatterns = @(
        "task board", "\btb\b", "user task", "user_report", "user_suggestion",
        "suggestion", "navrh", "navrhy", "automation", "freework gate",
        "cadence", "budget", "workflow", "watchdog", "shortcut", "copilot",
        "privacy", "github cleanup", "rules", "hygiene", "icon", "scrollbar",
        "window", "clipboard"
    )
    foreach ($pattern in $maintenancePatterns) {
        if ($text -match $pattern) {
            return "maintenance"
        }
    }

    if ([string]$Entry.mode -eq "automation") {
        return "implementation"
    }

    return "other"
}

$workLog = Resolve-ProjectPath $WorkLogPath
$output = Resolve-ProjectPath $OutputPath
$cutoff = (Get-Date).AddDays(-[Math]::Max(1, $LookbackDays))

$entries = @()
if (Test-Path -LiteralPath $workLog) {
    $entries = @(Import-Csv -LiteralPath $workLog | Where-Object {
        try {
            [DateTime]::ParseExact([string]$_.start_local, "yyyy-MM-dd HH:mm", [System.Globalization.CultureInfo]::InvariantCulture) -ge $cutoff
        } catch {
            $false
        }
    })
}

$buckets = @{
    maintenance = 0.0
    implementation = 0.0
    other = 0.0
}

$classified = @()
foreach ($entry in $entries) {
    $minutes = Get-EntryDurationMinutes -Entry $entry
    $kind = Classify-WorkEntry -Entry $entry
    $buckets[$kind] = [double]$buckets[$kind] + $minutes
    $classified += [pscustomobject]@{
        start_local = $entry.start_local
        end_local = $entry.end_local
        mode = $entry.mode
        kind = $kind
        minutes = [Math]::Round($minutes, 1)
        summary = $entry.summary
    }
}

$total = [double]($buckets.maintenance + $buckets.implementation + $buckets.other)
$maintenancePercent = if ($total -gt 0) { [Math]::Round(($buckets.maintenance / $total) * 100, 1) } else { 0 }
$implementationPercent = if ($total -gt 0) { [Math]::Round(($buckets.implementation / $total) * 100, 1) } else { 0 }

$recommendation = "balanced"
$reason = "maintenance is near the target support range"
if ($buckets.maintenance -gt 60 -and $maintenancePercent -gt 30) {
    $recommendation = "prefer-product-implementation"
    $reason = "maintenance is above the 25% target and should be simplified or deferred when implementation is safe"
}
if ($buckets.maintenance -gt 90 -and $maintenancePercent -gt 40) {
    $recommendation = "pause-nonessential-maintenance"
    $reason = "maintenance consumed more than 40% of recent logged work and more than 90 minutes"
}
if ($buckets.implementation -lt 30 -and $buckets.maintenance -gt 60) {
    $recommendation = "pause-nonessential-maintenance"
    $reason = "recent logged work has substantial maintenance but little implementation"
}

$directory = Split-Path -Parent $output
if ($directory -and -not (Test-Path -LiteralPath $directory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

[pscustomobject]@{
    schema_version = 1
    updated_at = (Get-Date).ToString("o")
    lookback_days = $LookbackDays
    entries_count = $entries.Count
    maintenance_minutes = [Math]::Round([double]$buckets.maintenance, 1)
    implementation_minutes = [Math]::Round([double]$buckets.implementation, 1)
    other_minutes = [Math]::Round([double]$buckets.other, 1)
    maintenance_percent = $maintenancePercent
    implementation_percent = $implementationPercent
    target_maintenance_percent = 25
    warning_maintenance_percent = 30
    high_maintenance_percent = 40
    recommendation = $recommendation
    reason = $reason
    policy = "Target maintenance around 25% of logged work. Above 30%, prefer product/runtime implementation when safe. Above 40%, pause nonessential maintenance unless it is blocking, safety-critical, privacy-critical, or prevents repeated wasted work."
    recent_entries = @($classified | Select-Object -Last 20)
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $output -Encoding UTF8

Write-Host "maintenance_balance_written=$output"
Write-Host "maintenance_percent=$maintenancePercent"
Write-Host "implementation_percent=$implementationPercent"
Write-Host "recommendation=$recommendation"
