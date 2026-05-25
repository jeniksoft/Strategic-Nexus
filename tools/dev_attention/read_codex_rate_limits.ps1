param(
    [string]$CodexExe = "",
    [string]$OutputPath = "dist/private_reports/codex_rate_limits.json",
    [string]$UsageBudgetStatePath = ".codex_local/usage_budget_state.md",
    [string]$UsageBudgetLogPath = ".codex_local/usage_budget_log.csv",
    [switch]$UpdateUsageBudgetState
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

function Find-CodexExe {
    if ($CodexExe -and (Test-Path -LiteralPath $CodexExe)) {
        return (Resolve-Path -LiteralPath $CodexExe).Path
    }

    $localBins = Join-Path $env:LOCALAPPDATA "OpenAI\Codex\bin"
    if (Test-Path -LiteralPath $localBins) {
        $candidate = Get-ChildItem -LiteralPath $localBins -Recurse -Filter "codex.exe" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    $cmd = Get-Command codex -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -and (Test-Path -LiteralPath $cmd.Source)) {
        return $cmd.Source
    }

    throw "codex.exe was not found."
}

function Send-AppServerMessage {
    param(
        [System.Diagnostics.Process]$Process,
        [object]$Message
    )

    $json = $Message | ConvertTo-Json -Depth 20 -Compress
    $Process.StandardInput.WriteLine($json)
}

function Convert-RateWindow {
    param([object]$Window)

    if ($null -eq $Window) {
        return $null
    }

    $resetUtc = $null
    $resetLocal = $null
    if ($null -ne $Window.resetsAt) {
        $resetUtc = [DateTimeOffset]::FromUnixTimeSeconds([int64]$Window.resetsAt).UtcDateTime
        $resetLocal = [DateTimeOffset]::FromUnixTimeSeconds([int64]$Window.resetsAt).ToLocalTime().DateTime
    }

    $used = [int]$Window.usedPercent
    return [pscustomobject]@{
        used_percent = $used
        remaining_percent = [Math]::Max(0, 100 - $used)
        window_duration_minutes = $Window.windowDurationMins
        resets_at_unix = $Window.resetsAt
        resets_at_utc = if ($resetUtc) { $resetUtc.ToString("o") } else { $null }
        resets_at_local = if ($resetLocal) { $resetLocal.ToString("yyyy-MM-dd HH:mm:ss") } else { $null }
    }
}

function Read-CodexRateLimits {
    $exe = Find-CodexExe

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $exe
    $psi.Arguments = "app-server --listen stdio://"
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false

    $process = [System.Diagnostics.Process]::Start($psi)
    try {
        Send-AppServerMessage -Process $process -Message @{
            id = 1
            method = "initialize"
            params = @{
                clientInfo = @{
                    name = "strategic-nexus-usage-probe"
                    title = "Strategic Nexus Usage Probe"
                    version = "0.1"
                }
                capabilities = @{
                    experimentalApi = $true
                    requestAttestation = $false
                }
            }
        }

        Start-Sleep -Milliseconds 250
        Send-AppServerMessage -Process $process -Message @{ method = "initialized" }

        Start-Sleep -Milliseconds 600
        Send-AppServerMessage -Process $process -Message @{
            id = 2
            method = "account/rateLimits/read"
        }

        Start-Sleep -Milliseconds 2500
        $process.StandardInput.Close()

        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit(10000) | Out-Null
    } finally {
        if (-not $process.HasExited) {
            $process.Kill()
        }
    }

    $response = $null
    foreach ($line in ($stdout -split "`r?`n")) {
        if (-not $line.Trim()) {
            continue
        }
        try {
            $message = $line | ConvertFrom-Json
        } catch {
            continue
        }
        if ($message.id -eq 2 -and $message.result) {
            $response = $message.result
        }
    }

    if (-not $response) {
        throw "Codex app-server did not return account/rateLimits/read. stderr: $stderr"
    }

    $primary = Convert-RateWindow -Window $response.rateLimits.primary
    $secondary = Convert-RateWindow -Window $response.rateLimits.secondary

    return [pscustomobject]@{
        schema_version = 1
        collected_at_local = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
        source = "codex app-server account/rateLimits/read"
        codex_exe = $exe
        limit_id = $response.rateLimits.limitId
        limit_name = $response.rateLimits.limitName
        plan_type = $response.rateLimits.planType
        rate_limit_reached_type = $response.rateLimits.rateLimitReachedType
        primary = $primary
        secondary = $secondary
        credits = $response.rateLimits.credits
        raw_rate_limits_by_limit_id = $response.rateLimitsByLimitId
    }
}

function Set-UsageBudgetStateFromRateLimits {
    param([object]$Snapshot)

    if ($null -eq $Snapshot.secondary) {
        return
    }

    $statePath = Resolve-ProjectPath $UsageBudgetStatePath
    $logPath = Resolve-ProjectPath $UsageBudgetLogPath
    $remaining = [int]$Snapshot.secondary.remaining_percent
    $used = [int]$Snapshot.secondary.used_percent
    $declaredOn = "$($Snapshot.collected_at_local) Europe/Prague"
    $resetDate = if ($Snapshot.secondary.resets_at_local) {
        ([DateTime]::Parse($Snapshot.secondary.resets_at_local)).ToString(
            "yyyy-MM-dd HH:mm",
            [System.Globalization.CultureInfo]::InvariantCulture
        ) + " Europe/Prague"
    } else {
        ""
    }

    if (Test-Path -LiteralPath $statePath) {
        $lines = [System.Collections.Generic.List[string]]::new()
        foreach ($line in (Get-Content -LiteralPath $statePath)) {
            $lines.Add($line)
        }

        $blockUpdates = @{
            "Latest declared remaining weekly usage:" = "$remaining%"
            "Declared on:" = $declaredOn
            "Latest declared used weekly usage:" = "$used%"
        }
        if ($resetDate) {
            $blockUpdates["Estimated next reset:"] = $resetDate
        }

        foreach ($heading in $blockUpdates.Keys) {
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -ne $heading) {
                    continue
                }

                for ($j = $i + 1; $j -lt $lines.Count; $j++) {
                    if ($lines[$j] -eq "```text") {
                        if ($j + 1 -lt $lines.Count) {
                            $lines[$j + 1] = $blockUpdates[$heading]
                        }
                        break
                    }
                }
                break
            }
        }

        $text = $lines -join "`n"
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Latest declared remaining weekly usage:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + "$remaining%" + $m.Groups[2].Value
            }
        )
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Declared on:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + $declaredOn + $m.Groups[2].Value
            }
        )
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Latest declared used weekly usage:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + "$used%" + $m.Groups[2].Value
            }
        )
        if ($resetDate) {
            $text = [System.Text.RegularExpressions.Regex]::Replace(
                $text,
                '(?ms)(Estimated next reset:\s*```text\s*)[^`]+?(\s*```)',
                [System.Text.RegularExpressions.MatchEvaluator]{
                    param($m)
                    return $m.Groups[1].Value + $resetDate + $m.Groups[2].Value
                }
            )
        }

        $entry = "$declaredOn | $remaining% remaining | $used% used | codex app-server account/rateLimits/read"
        if ($text -notmatch [regex]::Escape($entry)) {
            $text = [regex]::Replace($text, '(?ms)(Entries:\s*```text\s*)(.*?)(\s*```)', {
                param($m)
                $body = $m.Groups[2].Value.TrimEnd()
                return $m.Groups[1].Value + $body + "`n" + $entry + $m.Groups[3].Value
            })
        }
        Set-Content -LiteralPath $statePath -Encoding UTF8 -Value $text
    }

    $logDir = Split-Path -Parent $logPath
    if ($logDir -and -not (Test-Path -LiteralPath $logDir)) {
        New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    }
    if (-not (Test-Path -LiteralPath $logPath)) {
        Set-Content -LiteralPath $logPath -Encoding UTF8 -Value "timestamp_local,timezone,remaining_percent,used_percent,note"
    }
    $csvLine = '"' + $Snapshot.collected_at_local + '","Europe/Prague","' + $remaining + '","' + $used + '","codex app-server account/rateLimits/read"'
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $csvLine
}

$snapshot = Read-CodexRateLimits

$outputResolvedPath = Resolve-ProjectPath $OutputPath
$outputDir = Split-Path -Parent $outputResolvedPath
if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
$snapshot | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $outputResolvedPath -Encoding UTF8

if ($UpdateUsageBudgetState) {
    Set-UsageBudgetStateFromRateLimits -Snapshot $snapshot
}

Write-Host "codex_rate_limits_written=$outputResolvedPath"
Write-Host "primary_used_percent=$($snapshot.primary.used_percent)"
Write-Host "primary_resets_at_local=$($snapshot.primary.resets_at_local)"
Write-Host "secondary_used_percent=$($snapshot.secondary.used_percent)"
Write-Host "secondary_resets_at_local=$($snapshot.secondary.resets_at_local)"
