param(
    [string]$ArchiveRoot,
    [string]$SessionId = "stellaris-live-v0",
    [int]$PollSeconds = 2,
    [int]$StabilityDelayMs = 750
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$watcherScript = Join-Path $PSScriptRoot "watch_stellaris_live_autosaves.ps1"

if (-not (Test-Path -LiteralPath $watcherScript)) {
    throw "Missing watcher script: $watcherScript"
}

if ([string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    $ArchiveRoot = Join-Path $repoRoot "dist/live_autosave_archive"
}

if ($PollSeconds -lt 1) {
    $PollSeconds = 1
}

$existing = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
        $_.CommandLine -and
        $_.CommandLine -like "*watch_stellaris_live_autosaves.ps1*" -and
        $_.CommandLine -like "*$SessionId*"
    } |
    Select-Object -First 1

if ($null -ne $existing) {
    [pscustomobject]@{
        Started = $false
        Reason = "already_running"
        ProcessId = $existing.ProcessId
        SessionId = $SessionId
        ArchiveRoot = $ArchiveRoot
    }
    return
}

$logRoot = Join-Path $repoRoot "dist/live_autosave_watch_logs"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
New-Item -ItemType Directory -Force -Path $ArchiveRoot | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$stdoutPath = Join-Path $logRoot "$SessionId-$stamp.out.log"
$stderrPath = Join-Path $logRoot "$SessionId-$stamp.err.log"

function Quote-Arg {
    param([Parameter(Mandatory = $true)][string]$Value)
    '"' + ($Value -replace '"', '\"') + '"'
}

$arguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", (Quote-Arg $watcherScript),
    "-ArchiveRoot", (Quote-Arg $ArchiveRoot),
    "-SessionId", (Quote-Arg $SessionId),
    "-PollSeconds", $PollSeconds,
    "-StabilityDelayMs", $StabilityDelayMs
) -join " "

$process = Start-Process `
    -FilePath "powershell.exe" `
    -ArgumentList $arguments `
    -WorkingDirectory $repoRoot `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

[pscustomobject]@{
    Started = $true
    ProcessId = $process.Id
    SessionId = $SessionId
    ArchiveRoot = $ArchiveRoot
    Stdout = $stdoutPath
    Stderr = $stderrPath
}
