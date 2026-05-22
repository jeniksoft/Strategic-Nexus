param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json",
    [string]$StopFilePath = "dist/private_reports/user_task_board.stop",
    [int]$RestartDelaySeconds = 3
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

Remove-Item -LiteralPath $stopPath -Force -ErrorAction SilentlyContinue

try {
    while (-not (Test-Path -LiteralPath $stopPath)) {
        $arguments = "-NoProfile -STA -ExecutionPolicy Bypass -File `"$boardScript`" -TaskFilePath `"$TaskFilePath`" -StopFilePath `"$StopFilePath`""

        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
        $startInfo.Arguments = $arguments
        $startInfo.WorkingDirectory = $repoRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true

        $process = [System.Diagnostics.Process]::Start($startInfo)

        Wait-Process -Id $process.Id

        if (-not (Test-Path -LiteralPath $stopPath)) {
            Start-Sleep -Seconds ([Math]::Max(1, $RestartDelaySeconds))
        }
    }
} finally {
    $singleInstanceMutex.ReleaseMutex()
    $singleInstanceMutex.Dispose()
}
