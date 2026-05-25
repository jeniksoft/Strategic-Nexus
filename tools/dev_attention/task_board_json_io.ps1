function Ensure-TaskBoardDirectory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
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
