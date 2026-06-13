param(
    [string]$CodexExe = "",
    [string]$OutputPath = "dist/private_reports/codex_rate_limits.json",
    [string]$UsageBudgetStatePath = ".codex_local/usage_budget_state.md",
    [string]$UsageBudgetLogPath = ".codex_local/usage_budget_log.csv",
    [switch]$UpdateUsageBudgetState
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

try {

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
        # Windows Store-installed binaries under WindowsApps are frequently non-startable directly
        # (Access denied) outside the interactive app context. Prefer LocalAppData installs.
        if ($cmd.Source -match "\\WindowsApps\\") {
            throw "Found codex.exe under WindowsApps, but it may be non-startable. Install/ensure the LocalAppData Codex bin exists under %LOCALAPPDATA%\\OpenAI\\Codex\\bin, or pass -CodexExe explicitly."
        }
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
    $Process.StandardInput.Flush()
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

function New-RateLimitSnapshot {
    param(
        [object]$Response,
        [string]$CodexExePath
    )

    $primary = Convert-RateWindow -Window $Response.rateLimits.primary
    $secondary = Convert-RateWindow -Window $Response.rateLimits.secondary

    return [pscustomobject]@{
        schema_version = 1
        collected_at_local = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
        source = "codex app-server account/rateLimits/read"
        codex_exe = $CodexExePath
        limit_id = $Response.rateLimits.limitId
        limit_name = $Response.rateLimits.limitName
        plan_type = $Response.rateLimits.planType
        rate_limit_reached_type = $Response.rateLimits.rateLimitReachedType
        primary = $primary
        secondary = $secondary
        credits = $Response.rateLimits.credits
        raw_rate_limits_by_limit_id = $Response.rateLimitsByLimitId
    }
}

function Read-CodexRateLimitsWithNode {
    param([string]$CodexExePath)

    $nodeCandidates = @(
        (Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe")
    )
    $nodeCommand = Get-Command node -ErrorAction SilentlyContinue
    if ($nodeCommand -and $nodeCommand.Source) {
        $nodeCandidates += $nodeCommand.Source
    }

    $nodeExe = $nodeCandidates |
        Where-Object { $_ -and (Test-Path -LiteralPath $_) -and ($_ -notmatch "\\WindowsApps\\") } |
        Select-Object -First 1
    if (-not $nodeExe) {
        return $null
    }

    $helper = Join-Path $PSScriptRoot "read_codex_rate_limits_appserver.js"
    if (-not (Test-Path -LiteralPath $helper)) {
        return $null
    }

    $stderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $stdout = (& $nodeExe $helper $CodexExePath 2> $stderrPath | Out-String)
        $stderr = if (Test-Path -LiteralPath $stderrPath) {
            Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue
        } else {
            ""
        }

        if ($LASTEXITCODE -ne 0 -or -not $stdout.Trim()) {
            return $null
        }

        try {
            return ($stdout | ConvertFrom-Json)
        } catch {
            return $null
        }
    } finally {
        Remove-Item -LiteralPath $stderrPath -ErrorAction SilentlyContinue
    }
}

function Read-CodexRateLimits {
    $exe = Find-CodexExe

    $nodeResponse = Read-CodexRateLimitsWithNode -CodexExePath $exe
    if ($nodeResponse) {
        return New-RateLimitSnapshot -Response $nodeResponse -CodexExePath $exe
    }

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $exe
    $psi.Arguments = "app-server --listen stdio://"
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    try { $psi.StandardInputEncoding = $utf8NoBom } catch {}
    try { $psi.StandardOutputEncoding = $utf8NoBom } catch {}
    try { $psi.StandardErrorEncoding = $utf8NoBom } catch {}

    $process = [System.Diagnostics.Process]::Start($psi)

    function Read-JsonRpcMessage {
        param(
            [System.Diagnostics.Process]$Process,
            [int]$TimeoutMs
        )

        $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
        while ([DateTime]::UtcNow -lt $deadline -and -not $Process.HasExited) {
            $remainingMs = [int][Math]::Max(1, ($deadline - [DateTime]::UtcNow).TotalMilliseconds)
            $readTask = $Process.StandardOutput.ReadLineAsync()
            if (-not $readTask.Wait($remainingMs)) {
                return $null
            }
            $jsonText = $readTask.Result
            if ($null -eq $jsonText) {
                return $null
            }
            if (-not $jsonText.Trim()) {
                continue
            }
            try {
                return ($jsonText | ConvertFrom-Json)
            } catch {
                continue
            }
        }

        return $null
    }

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

        $initialized = $false
        $initializeError = $null
        $initDeadline = (Get-Date).AddSeconds(20)
        while ((Get-Date) -lt $initDeadline -and -not $initialized -and -not $process.HasExited) {
            $remainingMs = [int][Math]::Max(1, ($initDeadline - (Get-Date)).TotalMilliseconds)
            $message = Read-JsonRpcMessage -Process $process -TimeoutMs $remainingMs
            if ($null -eq $message) {
                break
            }
            if ($message.id -eq 1 -and $message.result) {
                $initialized = $true
            }
            if ($message.id -eq 1 -and $message.error) {
                $initializeError = $message.error
                break
            }
        }
        if (-not $initialized) {
            if ($initializeError -and $initializeError.message) {
                throw "Codex app-server initialize failed: $($initializeError.message)"
            }
            throw "Codex app-server did not initialize before timeout"
        }

        Send-AppServerMessage -Process $process -Message @{ method = "initialized" }

        Send-AppServerMessage -Process $process -Message @{
            id = 2
            method = "account/rateLimits/read"
        }

        $response = $null
        $errorResponse = $null
        $rateLimitsDeadline = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $rateLimitsDeadline -and -not $response -and -not $errorResponse -and -not $process.HasExited) {
            $remainingMs = [int][Math]::Max(1, ($rateLimitsDeadline - (Get-Date)).TotalMilliseconds)
            $message = Read-JsonRpcMessage -Process $process -TimeoutMs $remainingMs
            if ($null -eq $message) {
                break
            }
            if ($message.id -eq 2 -and $message.result) {
                $response = $message.result
            }
            if ($message.id -eq 2 -and $message.error) {
                $errorResponse = $message.error
            }
        }

        if (-not $process.HasExited) {
            $process.StandardInput.Close()
            if (-not $process.WaitForExit(5000)) {
                $process.Kill()
                $process.WaitForExit(5000) | Out-Null
            }
        }
        $stderr = $process.StandardError.ReadToEnd()
    } finally {
        if (-not $process.HasExited) {
            $process.Kill()
        }
    }

    if (-not $response) {
        if ($errorResponse -and $errorResponse.message) {
            throw "Codex app-server returned an error for account/rateLimits/read: $($errorResponse.message)"
        }
        $exitCode = if ($null -ne $process.ExitCode) { $process.ExitCode } else { "unknown" }
        throw "Codex app-server did not return account/rateLimits/read. exit=$exitCode stderr: $stderr"
    }

    return New-RateLimitSnapshot -Response $response -CodexExePath $exe
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
    $verifiedOn = "$($Snapshot.collected_at_local) Europe/Prague"
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
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Latest Verified Local App-Server Reading:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + "$remaining%" + $m.Groups[2].Value
            }
        )
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Verified remaining weekly usage from `account/rateLimits/read`:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + "$remaining%" + $m.Groups[2].Value
            }
        )
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Verified used weekly usage from `account/rateLimits/read`:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + "$used%" + $m.Groups[2].Value
            }
        )
        $text = [System.Text.RegularExpressions.Regex]::Replace(
            $text,
            '(?ms)(Verified on:\s*```text\s*)[^`]+?(\s*```)',
            [System.Text.RegularExpressions.MatchEvaluator]{
                param($m)
                return $m.Groups[1].Value + $verifiedOn + $m.Groups[2].Value
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

exit 0

} catch {
    $msg = "read_codex_rate_limits_failed=" + $_.Exception.Message
    $details = @($msg)
    if ($_.ScriptStackTrace) {
        $details += "stacktrace_begin"
        $details += $_.ScriptStackTrace
        $details += "stacktrace_end"
    }

    try {
        $statePath = Resolve-ProjectPath $UsageBudgetStatePath
        if (Test-Path -LiteralPath $statePath) {
            $stateText = Get-Content -Raw -LiteralPath $statePath
            $remainingMatch = [regex]::Match(
                $stateText,
                '(?ms)Latest declared remaining weekly usage:\s*```text\s*(\d+)%\s*```'
            )
            if ($remainingMatch.Success) {
                $remaining = [int]$remainingMatch.Groups[1].Value
                $usedMatch = [regex]::Match(
                    $stateText,
                    '(?ms)Latest declared used weekly usage:\s*```text\s*(\d+)%\s*```'
                )
                $used = if ($usedMatch.Success) { [int]$usedMatch.Groups[1].Value } else { [Math]::Max(0, 100 - $remaining) }
                $declaredMatch = [regex]::Match(
                    $stateText,
                    '(?ms)Declared on:\s*```text\s*([^`]+?)\s*```'
                )
                $resetMatch = [regex]::Match(
                    $stateText,
                    '(?ms)Estimated next reset:\s*```text\s*([^`]+?)\s*```'
                )
                $fallbackWindow = [pscustomobject]@{
                    used_percent = $used
                    remaining_percent = $remaining
                    window_duration_minutes = $null
                    resets_at_unix = $null
                    resets_at_utc = $null
                    resets_at_local = if ($resetMatch.Success) { $resetMatch.Groups[1].Value.Trim() } else { $null }
                }
                $snapshot = [pscustomobject]@{
                    schema_version = 1
                    collected_at_local = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
                    collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
                    source = "local usage_budget_state fallback"
                    warning = $msg
                    codex_exe = $null
                    limit_id = "local-declared"
                    limit_name = "Local declared weekly usage"
                    plan_type = $null
                    rate_limit_reached_type = $null
                    primary = $null
                    secondary = $fallbackWindow
                    credits = $null
                    raw_rate_limits_by_limit_id = $null
                    declared_on = if ($declaredMatch.Success) { $declaredMatch.Groups[1].Value.Trim() } else { $null }
                }

                $outputResolvedPath = Resolve-ProjectPath $OutputPath
                $outputDir = Split-Path -Parent $outputResolvedPath
                if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
                    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
                }
                $snapshot | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $outputResolvedPath -Encoding UTF8

                Write-Host "codex_rate_limits_written=$outputResolvedPath"
                Write-Host "rate_limits_source=local_usage_budget_state_fallback"
                Write-Host "primary_used_percent="
                Write-Host "primary_resets_at_local="
                Write-Host "secondary_used_percent=$used"
                Write-Host "secondary_resets_at_local=$($fallbackWindow.resets_at_local)"
                exit 0
            }
        }
    } catch {
        $details += "fallback_failed=$($_.Exception.Message)"
    }

    try {
        $errPath = "dist/private_reports/read_codex_rate_limits_error.txt"
        $errDir = Split-Path -Parent $errPath
        if ($errDir -and -not (Test-Path -LiteralPath $errDir)) {
            New-Item -ItemType Directory -Force -Path $errDir | Out-Null
        }
        $details -join "`n" | Set-Content -LiteralPath $errPath -Encoding UTF8
    } catch {
        # Last-resort: still try to surface *something* to the caller.
        "read_codex_rate_limits_failed_unable_to_write_error_file=$($_.Exception.Message)" | Write-Output
    }
    $details -join "`n" | Write-Output
    exit 2
}
