param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json",
    [string]$StopFilePath = "dist/private_reports/user_task_board.stop",
    [int]$RestartDelaySeconds = 3,
    [int]$MaxQuickRestarts = 3,
    [int]$QuickRestartWindowSeconds = 30,
    [int]$FailureCooldownSeconds = 300
)

$ErrorActionPreference = "Stop"

$singleInstanceMutexName = "StrategicNexusUserTaskBoardWatchdog"
$createdNewMutex = $false
$singleInstanceMutex = New-Object System.Threading.Mutex($true, $singleInstanceMutexName, [ref]$createdNewMutex)
if (-not $createdNewMutex) {
    return
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$boardScript = Join-Path $repoRoot "tools\dev_attention\user_task_board.ps1"
$stopPath = if ([System.IO.Path]::IsPathRooted($StopFilePath)) {
    $StopFilePath
} else {
    Join-Path $repoRoot $StopFilePath
}

$stopDirectory = Split-Path -Parent $stopPath
if ($stopDirectory -and -not (Test-Path -LiteralPath $stopDirectory)) {
    New-Item -ItemType Directory -Force -Path $stopDirectory | Out-Null
}
$watchdogLogPath = Join-Path $stopDirectory "user_task_board_watchdog.log"

Remove-Item -LiteralPath $stopPath -Force -ErrorAction SilentlyContinue

function Write-WatchdogLog {
    param([string]$Message)

    $timestamp = (Get-Date).ToString("o")
    Add-Content -LiteralPath $watchdogLogPath -Encoding UTF8 -Value "[$timestamp] $Message"
}

function Get-TerminalProcessIds {
    @(Get-Process WindowsTerminal,OpenConsole -ErrorAction SilentlyContinue | ForEach-Object { [int]$_.Id })
}

function Close-NewTaskBoardConsoleWindows {
    param([int[]]$ExistingIds)

    $existing = @{}
    foreach ($id in @($ExistingIds)) {
        $existing[$id] = $true
    }

    Start-Sleep -Milliseconds 750
    foreach ($terminal in @(Get-Process WindowsTerminal,OpenConsole -ErrorAction SilentlyContinue)) {
        if ($existing.ContainsKey([int]$terminal.Id)) {
            continue
        }

        $title = [string]$terminal.MainWindowTitle
        if ($title -eq "" -or $title -eq "Terminal" -or $title -eq "StrategicNexus.UserTaskBoard") {
            Stop-Process -Id $terminal.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

try {
    $quickRestartTimes = New-Object System.Collections.Generic.List[datetime]
    while (-not (Test-Path -LiteralPath $stopPath)) {
        $terminalIdsBeforeStart = Get-TerminalProcessIds
        $startedAt = Get-Date
        $arguments = "-NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$boardScript`" -TaskFilePath `"$TaskFilePath`" -StopFilePath `"$StopFilePath`""

        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
        $startInfo.Arguments = $arguments
        $startInfo.WorkingDirectory = $repoRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden

        $process = [System.Diagnostics.Process]::Start($startInfo)
        Write-WatchdogLog "started task board pid=$($process.Id)"
        Close-NewTaskBoardConsoleWindows -ExistingIds $terminalIdsBeforeStart

        Wait-Process -Id $process.Id
        $lifetimeSeconds = ((Get-Date) - $startedAt).TotalSeconds
        Write-WatchdogLog "task board exited pid=$($process.Id) lifetime_seconds=$([Math]::Round($lifetimeSeconds, 1)) exit_code=$($process.ExitCode)"

        if ($process.ExitCode -eq 0) {
            Write-WatchdogLog "normal task board exit; watchdog will not restart it"
            break
        }

        if ($lifetimeSeconds -le $QuickRestartWindowSeconds) {
            $quickRestartTimes.Add((Get-Date))
            $windowStart = (Get-Date).AddSeconds(-1 * [Math]::Max(1, $QuickRestartWindowSeconds))
            $quickRestartTimes = [System.Collections.Generic.List[datetime]]@($quickRestartTimes | Where-Object { $_ -ge $windowStart })
        } else {
            $quickRestartTimes.Clear()
        }

        if ($quickRestartTimes.Count -ge [Math]::Max(1, $MaxQuickRestarts)) {
            Write-WatchdogLog "restart storm detected quick_restarts=$($quickRestartTimes.Count); cooling down for $FailureCooldownSeconds seconds"
            Start-Sleep -Seconds ([Math]::Max(30, $FailureCooldownSeconds))
            $quickRestartTimes.Clear()
        }

        if (-not (Test-Path -LiteralPath $stopPath)) {
            Start-Sleep -Seconds ([Math]::Max(1, $RestartDelaySeconds))
        }
    }
} finally {
    $singleInstanceMutex.ReleaseMutex()
    $singleInstanceMutex.Dispose()
}
