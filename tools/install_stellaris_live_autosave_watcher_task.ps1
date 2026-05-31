param(
    [string]$TaskName = "Strategic Nexus Live Autosave Watcher",
    [string]$ArchiveRoot,
    [string]$SessionId = "stellaris-live-v0",
    [int]$PollSeconds = 2,
    [int]$StabilityDelayMs = 750,
    [switch]$StartNow
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$launcherScript = Join-Path $PSScriptRoot "start_stellaris_live_autosave_watcher.ps1"

if (-not (Test-Path -LiteralPath $launcherScript)) {
    throw "Missing launcher script: $launcherScript"
}

if ([string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    $ArchiveRoot = Join-Path $repoRoot "dist/live_autosave_archive"
}

function Quote-Arg {
    param([Parameter(Mandatory = $true)][string]$Value)
    '"' + ($Value -replace '"', '\"') + '"'
}

$argument = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", (Quote-Arg $launcherScript),
    "-ArchiveRoot", (Quote-Arg $ArchiveRoot),
    "-SessionId", (Quote-Arg $SessionId),
    "-PollSeconds", $PollSeconds,
    "-StabilityDelayMs", $StabilityDelayMs
) -join " "

$action = New-ScheduledTaskAction `
    -Execute "powershell.exe" `
    -Argument $argument `
    -WorkingDirectory $repoRoot

$trigger = New-ScheduledTaskTrigger -AtLogOn

$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Seconds 0) `
    -MultipleInstances IgnoreNew `
    -StartWhenAvailable

$startupMode = "scheduled_task"
$state = ""
$lastRunTime = $null
$lastTaskResult = $null
$runValueName = "StrategicNexusLiveAutosaveWatcher"

try {
    Register-ScheduledTask `
        -TaskName $TaskName `
        -Action $action `
        -Trigger $trigger `
        -Settings $settings `
        -Description "Starts Strategic Nexus live Stellaris autosave capture on user logon." `
        -Force | Out-Null

    if ($StartNow) {
        Start-ScheduledTask -TaskName $TaskName
    }

    $task = Get-ScheduledTask -TaskName $TaskName
    $info = Get-ScheduledTaskInfo -TaskName $TaskName
    $state = $task.State
    $lastRunTime = $info.LastRunTime
    $lastTaskResult = $info.LastTaskResult
}
catch {
    $startupMode = "hkcu_run"
    $runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
    New-Item -Path $runKey -Force | Out-Null
    New-ItemProperty `
        -Path $runKey `
        -Name $runValueName `
        -Value ("powershell.exe " + $argument) `
        -PropertyType String `
        -Force | Out-Null

    if ($StartNow) {
        Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList $argument `
            -WorkingDirectory $repoRoot `
            -WindowStyle Hidden | Out-Null
    }

    $state = "registered_in_hkcu_run"
}

[pscustomobject]@{
    StartupMode = $startupMode
    TaskName = $TaskName
    State = $state
    LastRunTime = $lastRunTime
    LastTaskResult = $lastTaskResult
    RunValueName = $runValueName
    ArchiveRoot = $ArchiveRoot
    SessionId = $SessionId
}
