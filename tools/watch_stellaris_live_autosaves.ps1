param(
    [string[]]$SaveRoot = @(),
    [string]$ArchiveRoot,
    [string]$SessionId,
    [int]$PollSeconds = 2,
    [int]$StabilityDelayMs = 750,
    [int]$MaxIterations = 0,
    [switch]$StopWhenStellarisExits
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-ExePath {
    $candidatePaths = @(
        (Join-Path $repoRoot "dist/strategic_nexus_app_test.exe"),
        (Join-Path $repoRoot "Strategic Nexus.exe")
    )
    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw "Missing Strategic Nexus executable. Run cmd /c tools/run_v0_pipeline_tests.cmd first."
}

function Get-KeyValueLineValue {
    param(
        [object]$Lines,
        [string]$Key
    )

    $prefix = $Key + "="
    foreach ($line in @($Lines | ForEach-Object { [string]$_ })) {
        if ($line -like "$prefix*") {
            return $line.Substring($prefix.Length)
        }
    }
    return ""
}

function Resolve-SaveRoots {
    param([string]$ExePath)

    if ($SaveRoot.Count -gt 0) {
        return @($SaveRoot | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique)
    }

    $outputPath = Join-Path $repoRoot "dist/live_stellaris_save_roots.json"
    $output = & $ExePath --discover-stellaris-save-roots $outputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Save root discovery failed: $($output -join '; ')"
    }

    $json = Get-Content -Raw -LiteralPath $outputPath | ConvertFrom-Json
    return @($json.candidates |
        Where-Object { $_.exists -eq $true -and -not [string]::IsNullOrWhiteSpace([string]$_.path) } |
        ForEach-Object { [string]$_.path } |
        Select-Object -Unique)
}

function Invoke-LiveArchivePass {
    param(
        [string]$ExePath,
        [string[]]$Roots
    )

    $totalCopied = 0
    $totalSkipped = 0
    foreach ($root in $Roots) {
        $output = & $ExePath --archive-live-saves $root $ArchiveRoot $SessionId $StabilityDelayMs
        $exitCode = $LASTEXITCODE
        $text = $output -join "`n"
        if ($exitCode -ne 0) {
            Write-Warning "live autosave archive pass failed for '$root': $text"
            continue
        }
        $copied = Get-KeyValueLineValue -Lines $output -Key "live_autosave_archive_copied"
        $skipped = Get-KeyValueLineValue -Lines $output -Key "live_autosave_archive_skipped"
        if ($copied -match '^\d+$') {
            $totalCopied += [int]$copied
        }
        if ($skipped -match '^\d+$') {
            $totalSkipped += [int]$skipped
        }
    }
    [pscustomobject]@{
        Copied = $totalCopied
        Skipped = $totalSkipped
    }
}

if ([string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    $ArchiveRoot = Join-Path $repoRoot "dist/live_autosave_archive"
}

if ([string]::IsNullOrWhiteSpace($SessionId)) {
    $SessionId = "live-" + (Get-Date -Format "yyyyMMdd-HHmmss")
}

if ($PollSeconds -lt 1) {
    $PollSeconds = 1
}

$exe = Resolve-ExePath
$roots = Resolve-SaveRoots -ExePath $exe
if ($roots.Count -eq 0) {
    throw "No existing Stellaris save roots were found."
}

New-Item -ItemType Directory -Force -Path $ArchiveRoot | Out-Null

$script:pendingSweep = $true
$watchers = @()
$subscriptions = @()
try {
    foreach ($root in $roots) {
        $watcher = New-Object System.IO.FileSystemWatcher
        $watcher.Path = $root
        $watcher.Filter = "*.sav"
        $watcher.IncludeSubdirectories = $true
        $watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, LastWrite, Size'
        $watcher.EnableRaisingEvents = $true
        foreach ($eventName in @("Changed", "Created", "Renamed")) {
            $subscriptions += Register-ObjectEvent -InputObject $watcher -EventName $eventName -Action {
                $script:pendingSweep = $true
            }
        }
        $watchers += $watcher
    }

    $iteration = 0
    while ($true) {
        $stellarisRunning = $null -ne (Get-Process -Name stellaris -ErrorAction SilentlyContinue | Select-Object -First 1)
        if ($StopWhenStellarisExits -and -not $stellarisRunning -and $iteration -gt 0) {
            break
        }

        if ($script:pendingSweep -or $iteration -eq 0) {
            $result = Invoke-LiveArchivePass -ExePath $exe -Roots $roots
            $script:pendingSweep = $false
            Write-Host ("live_autosave_watch_iteration={0};copied={1};skipped={2};stellaris_running={3}" -f $iteration, $result.Copied, $result.Skipped, $stellarisRunning)
        }

        ++$iteration
        if ($MaxIterations -gt 0 -and $iteration -ge $MaxIterations) {
            break
        }

        Start-Sleep -Seconds $PollSeconds
        $script:pendingSweep = $true
    }
}
finally {
    foreach ($subscription in $subscriptions) {
        Unregister-Event -SubscriptionId $subscription.Id -ErrorAction SilentlyContinue
    }
    foreach ($watcher in $watchers) {
        $watcher.EnableRaisingEvents = $false
        $watcher.Dispose()
    }
}
