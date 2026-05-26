param(
    [string]$WorkLogPath = ".codex_local/codex_work_log.csv",
    [string]$MaintenanceBalancePath = "dist/private_reports/maintenance_balance.json",
    [int]$LookbackDays = 7,
    [int]$MinMaintenanceMinutes = 90,
    [double]$MinMaintenancePercent = 40.0,
    [int]$TopN = 6,
    [switch]$Force,
    [string]$SuggestionId = "suggestion-followup-vynucovat-pomer-maintenance-vs-produktova-implementace"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\\..")

function Resolve-ProjectPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

function Parse-LocalStamp {
    param([string]$Text)
    try {
        return [DateTime]::ParseExact($Text, "yyyy-MM-dd HH:mm", [System.Globalization.CultureInfo]::InvariantCulture)
    } catch {
        return $null
    }
}

function Get-EntryMinutes {
    param($Entry)
    $start = Parse-LocalStamp -Text ([string]$Entry.start_local)
    $end = Parse-LocalStamp -Text ([string]$Entry.end_local)
    if ($null -eq $start -or $null -eq $end) {
        return 0.0
    }
    return [Math]::Max(0, ($end - $start).TotalMinutes)
}

function Classify-WorkEntryKind {
    param($Entry)

    $text = (([string]$Entry.summary) + " " + ([string]$Entry.notes)).ToLowerInvariant()
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

    if ([string]$Entry.mode -eq "automation") {
        return "implementation"
    }

    return "other"
}

function Get-MaintenanceBalance {
    param([string]$BalancePath)

    $balanceScript = Join-Path $PSScriptRoot "measure_maintenance_balance.ps1"
    if (Test-Path -LiteralPath $balanceScript) {
        & $balanceScript -WorkLogPath $WorkLogPath -OutputPath $BalancePath -LookbackDays $LookbackDays | Out-Null
    }

    if (!(Test-Path -LiteralPath $BalancePath)) {
        throw "maintenance balance not found at $BalancePath"
    }

    return Get-Content -Raw -LiteralPath $BalancePath | ConvertFrom-Json
}

function Format-Minutes {
    param([double]$Minutes)
    return ([Math]::Round($Minutes, 0)).ToString("0")
}

$workLog = Resolve-ProjectPath $WorkLogPath
$balancePath = Resolve-ProjectPath $MaintenanceBalancePath

$balance = Get-MaintenanceBalance -BalancePath $balancePath

if (-not $Force) {
    if ([double]$balance.maintenance_percent -lt $MinMaintenancePercent -or [double]$balance.maintenance_minutes -lt $MinMaintenanceMinutes) {
        Write-Host "maintenance_simplifications_skipped=true"
        Write-Host "maintenance_percent=$($balance.maintenance_percent)"
        Write-Host "maintenance_minutes=$($balance.maintenance_minutes)"
        return
    }
}

$entries = @()
if (Test-Path -LiteralPath $workLog) {
    $cutoff = (Get-Date).AddDays(-[Math]::Max(1, $LookbackDays))
    $entries = @(Import-Csv -LiteralPath $workLog | Where-Object {
        $stamp = Parse-LocalStamp -Text ([string]$_.start_local)
        $null -ne $stamp -and $stamp -ge $cutoff
    })
}

$maintenance = @()
foreach ($entry in $entries) {
    if ((Classify-WorkEntryKind -Entry $entry) -ne "maintenance") {
        continue
    }
    $minutes = Get-EntryMinutes -Entry $entry
    if ($minutes -le 0) {
        continue
    }
    $maintenance += [pscustomobject]@{
        summary = [string]$entry.summary
        minutes = [double]$minutes
    }
}

$top = @(
    $maintenance |
        Group-Object -Property summary |
        ForEach-Object {
            [pscustomobject]@{
                summary = $_.Name
                minutes = [double](($_.Group | Measure-Object -Property minutes -Sum).Sum)
            }
        } |
        Sort-Object -Property minutes -Descending |
        Select-Object -First ([Math]::Max(1, $TopN))
)

$lines = @()
foreach ($item in $top) {
    $lines += ("- {0} min: {1}" -f (Format-Minutes -Minutes $item.minutes), $item.summary)
}
if ($lines.Count -eq 0) {
    $lines = @("- (nenalezeny zadne maintenance polozky v lookback okne)")
}

$proposal = @(
    "Pokud maintenance zatez zustava vysoka, toto jsou nejhlucnejsi maintenance polozky z poslednich $LookbackDays dnu (podle .codex_local/codex_work_log.csv):",
    "",
    ($lines -join "`n"),
    "",
    "Navrh: zjednodusit nebo zridit frekvenci jen u polozek, ktere neprinasi jasne riziko (bezpecnost/privacy/blocker) a opakuji se."
) -join "`n"

$benefit = "Udrzi Free Work soustredeny na produkt/runtime bez ztraty kritickych kontrol."
$why = "maintenance_balance.json hlasi $($balance.maintenance_percent)% maintenance ($($balance.maintenance_minutes) min) v lookback okne."
$next = "Vyber 1-2 polozky z vyse uvedeneho seznamu a definuj konkretni zjednoduseni (napr. snizit cadence, sloucit reporty, odstranit duplicitni check)."

& (Join-Path $PSScriptRoot "add_user_suggestion.ps1") `
    -Id $SuggestionId `
    -Title "Navrhnout zjednoduseni hlucnych kontrol/reportu" `
    -Category "workflow" `
    -Severity "normal" `
    -Status "realizovano" `
    -Proposal $proposal `
    -Benefit $benefit `
    -Why $why `
    -Next $next `
    -ReplaceExisting | Out-Null

Write-Host "maintenance_simplifications_written=true"
Write-Host "suggestion_id=$SuggestionId"

