param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json",
    [string]$GamingQuietProcessFilePath = ".codex_local/gaming_quiet_processes.txt",
    [string]$GamingQuietSessionStatePath = ".codex_local/gaming_quiet_session_state.json",
    [string]$GamingQuietSessionLogPath = ".codex_local/gaming_quiet_session_log.csv",
    [string]$ProjectProgressFilePath = "dist/private_reports/project_progress_estimate.json",
    [string]$RoadmapComplexityOutputPath = "dist/private_reports/roadmap_complexity_estimate.json"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "task_board_json_io.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Read-GamingQuietProcessNames {
    param([string]$Path)

    $names = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    [void]$names.Add("stellaris")

    if (Test-Path -LiteralPath $Path) {
        Get-Content -LiteralPath $Path | ForEach-Object {
            $line = ([string]$_).Trim()
            if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
                return
            }
            $processName = [System.IO.Path]::GetFileNameWithoutExtension($line)
            if (-not [string]::IsNullOrWhiteSpace($processName)) {
                [void]$names.Add($processName)
            }
        }
    }

    return @($names)
}

function ConvertTo-CsvField {
    param([object]$Value)

    $text = if ($null -eq $Value) { "" } else { [string]$Value }
    return '"' + $text.Replace('"', '""') + '"'
}

function Add-GamingQuietSessionLogEntry {
    param(
        [string]$LogPath,
        [string]$Event,
        [object]$Session,
        [string]$ObservedAt
    )

    Ensure-Directory $LogPath
    if (-not (Test-Path -LiteralPath $LogPath)) {
        Set-Content -LiteralPath $LogPath -Encoding UTF8 -Value "observed_at,event,process_name,process_id,start_time,detected_at,end_time,path"
    }

    $line = @(
        ConvertTo-CsvField $ObservedAt
        ConvertTo-CsvField $Event
        ConvertTo-CsvField $Session.process_name
        ConvertTo-CsvField $Session.process_id
        ConvertTo-CsvField $Session.start_time
        ConvertTo-CsvField $Session.detected_at
        ConvertTo-CsvField $(if ($Event -eq "end") { $ObservedAt } else { "" })
        ConvertTo-CsvField $Session.path
    ) -join ","

    Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value $line
}

function Read-GamingQuietSessionState {
    param([string]$StatePath)

    if (-not (Test-Path -LiteralPath $StatePath)) {
        return @()
    }

    try {
        $document = Get-Content -Raw -LiteralPath $StatePath | ConvertFrom-Json
        return @($document.active_sessions)
    } catch {
        return @()
    }
}

function Update-GamingQuietSessionState {
    param(
        [string]$StatePath,
        [string]$LogPath,
        [array]$RunningProcesses
    )

    $observedAt = (Get-Date).ToString("o")
    $previousSessions = @(Read-GamingQuietSessionState -StatePath $StatePath)
    $previousByKey = @{}
    foreach ($session in $previousSessions) {
        $key = "$($session.process_name)|$($session.process_id)"
        $previousByKey[$key] = $session
    }

    $activeSessions = @()
    $currentKeys = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($process in $RunningProcesses) {
        $key = "$($process.ProcessName)|$($process.Id)"
        [void]$currentKeys.Add($key)

        if ($previousByKey.ContainsKey($key)) {
            $activeSessions += $previousByKey[$key]
            continue
        }

        $startTime = ""
        try {
            $startTime = $process.StartTime.ToString("o")
        } catch {
        }

        $path = ""
        try {
            $path = [string]$process.Path
        } catch {
        }

        $session = [pscustomobject]@{
            process_name = [string]$process.ProcessName
            process_id = [int]$process.Id
            start_time = $startTime
            detected_at = $observedAt
            path = $path
        }
        $activeSessions += $session
        Add-GamingQuietSessionLogEntry -LogPath $LogPath -Event "start" -Session $session -ObservedAt $observedAt
    }

    foreach ($session in $previousSessions) {
        $key = "$($session.process_name)|$($session.process_id)"
        if (-not $currentKeys.Contains($key)) {
            Add-GamingQuietSessionLogEntry -LogPath $LogPath -Event "end" -Session $session -ObservedAt $observedAt
        }
    }

    Ensure-Directory $StatePath
    [pscustomobject]@{
        schema_version = 1
        updated_at = $observedAt
        active_sessions = @($activeSessions)
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $StatePath -Encoding UTF8
}

function Update-ProjectProgressEstimate {
    param(
        [string]$ProgressPath,
        [string]$ComplexityPath
    )

    $progressScript = Join-Path $PSScriptRoot "set_project_progress_estimate.ps1"
    if (-not (Test-Path -LiteralPath $progressScript)) {
        return $false
    }

    & powershell `
        -NoProfile `
        -ExecutionPolicy RemoteSigned `
        -File $progressScript `
        -ProjectProgressFilePath $ProgressPath `
        -RoadmapComplexityOutputPath $ComplexityPath | Out-Null

    return $true
}

function Update-MaintenanceBalance {
    $balanceScript = Join-Path $PSScriptRoot "measure_maintenance_balance.ps1"
    if (-not (Test-Path -LiteralPath $balanceScript)) {
        return $false
    }

    & powershell `
        -NoProfile `
        -ExecutionPolicy RemoteSigned `
        -File $balanceScript | Out-Null

    return $true
}

$taskPath = Resolve-ProjectPath $TaskFilePath
$gamingQuietProcessPath = Resolve-ProjectPath $GamingQuietProcessFilePath
$gamingQuietSessionState = Resolve-ProjectPath $GamingQuietSessionStatePath
$gamingQuietSessionLog = Resolve-ProjectPath $GamingQuietSessionLogPath
$projectProgressPath = Resolve-ProjectPath $ProjectProgressFilePath
$roadmapComplexityPath = Resolve-ProjectPath $RoadmapComplexityOutputPath
$projectProgressUpdated = Update-ProjectProgressEstimate -ProgressPath $projectProgressPath -ComplexityPath $roadmapComplexityPath
$maintenanceBalanceUpdated = Update-MaintenanceBalance
$quietProcessNames = @(Read-GamingQuietProcessNames -Path $gamingQuietProcessPath)
$runningQuietProcesses = @()

foreach ($processName in $quietProcessNames) {
    $runningQuietProcesses += @(Get-Process -Name $processName -ErrorAction SilentlyContinue)
}

$gamingQuietActive = $runningQuietProcesses.Count -gt 0
$removedQuietBlocker = $false

Update-GamingQuietSessionState `
    -StatePath $gamingQuietSessionState `
    -LogPath $gamingQuietSessionLog `
    -RunningProcesses $runningQuietProcesses

Invoke-TaskBoardJsonMutation -Path $taskPath -Mutate {
    if (-not (Test-Path -LiteralPath $taskPath)) {
        throw "Task file not found: $taskPath"
    }

    $document = Get-Content -Raw -LiteralPath $taskPath | ConvertFrom-Json
    $remaining = @()

    foreach ($task in @($document.tasks)) {
        if ($task.id -eq "blocker-gaming-quiet-mode-stellaris-running" -and -not $gamingQuietActive) {
            $script:removedQuietBlocker = $true
            continue
        }
        $remaining += $task
    }

    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        tasks = @($remaining)
    }
}

$runningNames = @($runningQuietProcesses | Select-Object -ExpandProperty ProcessName -Unique)
Write-Host "gaming_quiet_active=$gamingQuietActive"
Write-Host "gaming_quiet_processes=$($runningNames -join ',')"
Write-Host "gaming_quiet_blocker_removed=$removedQuietBlocker"
Write-Host "gaming_quiet_session_log=$gamingQuietSessionLog"
Write-Host "project_progress_updated=$projectProgressUpdated"
Write-Host "project_progress_file=$projectProgressPath"
Write-Host "roadmap_complexity_file=$roadmapComplexityPath"
