function Ensure-TaskBoardDirectory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Test-TaskBoardPathMatchesDefault {
    param(
        [string]$RepoRoot,
        [string]$Path,
        [string]$DefaultRelativePath
    )

    $resolvedInputPath = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $RepoRoot $Path }
    $resolvedPath = [System.IO.Path]::GetFullPath($resolvedInputPath)
    $defaultPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $DefaultRelativePath))

    return $resolvedPath.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) -ieq $defaultPath.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
}

function Invoke-TaskBoardStateSync {
    $syncScript = Join-Path $PSScriptRoot "sync_user_task_board_state.cmd"
    if (-not (Test-Path -LiteralPath $syncScript)) {
        return $false
    }

    & $syncScript | Out-Null
    return $true
}

function Invoke-TaskBoardJsonMutation {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Mutate,
        [int]$TimeoutMs = 10000
    )

    Ensure-TaskBoardDirectory $Path

    $lockPath = "$Path.lock"
    $lockDirectory = Split-Path -Parent $lockPath
    if ($lockDirectory -and -not (Test-Path -LiteralPath $lockDirectory)) {
        New-Item -ItemType Directory -Force -Path $lockDirectory | Out-Null
    }

    $lockStream = $null
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ($null -eq $lockStream) {
        try {
            $lockStream = [System.IO.File]::Open(
                $lockPath,
                [System.IO.FileMode]::OpenOrCreate,
                [System.IO.FileAccess]::ReadWrite,
                [System.IO.FileShare]::None)
        } catch {
            if ([DateTime]::UtcNow -ge $deadline) {
                throw "Timed out waiting for task-board JSON lock: $lockPath"
            }
            Start-Sleep -Milliseconds 50
        }
    }

    try {
        $document = & $Mutate
        $temporaryPath = "$Path.tmp.$PID.$([Guid]::NewGuid().ToString('N'))"
        $document | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
        Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
    } finally {
        if ($lockStream) {
            $lockStream.Dispose()
        }
    }
}
