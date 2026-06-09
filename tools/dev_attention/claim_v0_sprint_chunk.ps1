param(
    [ValidateSet('claim', 'complete', 'block')]
    [string]$Mode = 'claim',

    [string]$AutomationId = 'manual',
    [string]$RunId,
    [string]$ChunkId,
    [string]$Evidence = '',
    [string]$Commit = '',
    [int]$LeaseMinutes = 90,
    [string]$QueuePath = 'tools/dev_attention/v0_sprint_chunk_queue.json',
    [string]$LedgerPath = '.codex_local/v0_sprint_chunk_ledger.json',
    [string]$AutomationLogPath = '.codex_local/automation_run_log.csv'
)

$ErrorActionPreference = 'Stop'

function Get-IsoNow {
    return [DateTimeOffset]::Now.ToString('o')
}

function Read-JsonFile {
    param(
        [string]$Path,
        [object]$DefaultValue
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $DefaultValue
    }

    $raw = Get-Content -LiteralPath $Path -Raw
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $DefaultValue
    }

    return $raw | ConvertFrom-Json
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    $tmp = "$Path.tmp"
    $Value | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $tmp -Encoding UTF8
    Move-Item -LiteralPath $tmp -Destination $Path -Force
}

function Convert-Array {
    param([object]$Value)

    if ($null -eq $Value) {
        return @()
    }

    if ($Value -is [System.Array]) {
        return @($Value)
    }

    return @($Value)
}

function Get-TerminalAutomationRuns {
    param([string]$Path)

    $terminalResults = @('implemented', 'blocked', 'quiet', 'no_safe_task', 'failed')
    $records = @{}

    if (-not (Test-Path -LiteralPath $Path)) {
        return $records
    }

    try {
        $rows = Import-Csv -LiteralPath $Path
    } catch {
        return $records
    }

    foreach ($row in $rows) {
        $runId = [string]$row.run_id
        $result = [string]$row.result
        if ([string]::IsNullOrWhiteSpace($runId) -or $result -notin $terminalResults) {
            continue
        }

        $timestamp = [DateTimeOffset]::MinValue
        if (-not [DateTimeOffset]::TryParse([string]$row.timestamp_local, [ref]$timestamp)) {
            $timestamp = [DateTimeOffset]::Now
        }

        $existing = $null
        if ($records.ContainsKey($runId)) {
            $existing = $records[$runId]
        }

        if ($null -eq $existing -or $timestamp -ge $existing.timestamp) {
            $detailsParts = @()
            if (-not [string]::IsNullOrWhiteSpace([string]$row.summary)) {
                $detailsParts += [string]$row.summary
            }
            if (-not [string]::IsNullOrWhiteSpace([string]$row.details)) {
                $detailsParts += [string]$row.details
            }

            $records[$runId] = [pscustomobject]@{
                timestamp = $timestamp
                automation_id = [string]$row.automation_id
                result = $result
                evidence = ($detailsParts -join '; ').Trim()
            }
        }
    }

    return $records
}

$repoRoot = (Resolve-Path -LiteralPath '.').Path

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

$queueFullPath = Resolve-ProjectPath -Path $QueuePath
$ledgerFullPath = Resolve-ProjectPath -Path $LedgerPath
$automationLogFullPath = Resolve-ProjectPath -Path $AutomationLogPath
$ledgerDir = Split-Path -Parent $ledgerFullPath
if ($ledgerDir -and -not (Test-Path -LiteralPath $ledgerDir)) {
    New-Item -ItemType Directory -Path $ledgerDir -Force | Out-Null
}

if (-not $RunId) {
    $RunId = 'v0sprint-{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([Guid]::NewGuid().ToString('N').Substring(0, 8))
}

$lockPath = "$ledgerFullPath.lock"
$lockStream = $null
for ($i = 0; $i -lt 40; $i++) {
    try {
        $lockStream = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        break
    } catch {
        Start-Sleep -Milliseconds 250
    }
}

if ($null -eq $lockStream) {
    throw "Could not acquire sprint ledger lock: $lockPath"
}

try {
    $queue = Read-JsonFile -Path $queueFullPath -DefaultValue $null
    if ($null -eq $queue) {
        throw "Sprint queue not found or empty: $queueFullPath"
    }

    $defaultLedger = [pscustomobject]@{
        schema_version = 1
        updated_at = Get-IsoNow
        active_claims = @()
        completed = @()
        blocked = @()
        history = @()
    }

    $ledger = Read-JsonFile -Path $ledgerFullPath -DefaultValue $defaultLedger
    $terminalRuns = Get-TerminalAutomationRuns -Path $automationLogFullPath
    $completedChunkIds = @{}
    foreach ($done in (Convert-Array $ledger.completed)) {
        if ($done.chunk_id) {
            $completedChunkIds[[string]$done.chunk_id] = $true
        }
    }
    foreach ($chunk in (Convert-Array $queue.chunks)) {
        if ($chunk.id -and $chunk.status -in @('done', 'verified', 'completed')) {
            $completedChunkIds[[string]$chunk.id] = $true
        }
    }

    $now = [DateTimeOffset]::Now
    $activeClaims = New-Object System.Collections.Generic.List[object]
    foreach ($claim in (Convert-Array $ledger.active_claims)) {
        $expires = [DateTimeOffset]::MinValue
        if ([DateTimeOffset]::TryParse([string]$claim.lease_expires_at, [ref]$expires) -and $expires -le $now) {
            $expired = [ordered]@{
                event = 'expired'
                chunk_id = $claim.chunk_id
                run_id = $claim.run_id
                automation_id = $claim.automation_id
                at = Get-IsoNow
            }
            $history = @(Convert-Array $ledger.history)
            $ledger.history = @($history + $expired)
            continue
        }

        if ($claim.chunk_id -and $completedChunkIds.ContainsKey([string]$claim.chunk_id)) {
            $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
                event = 'released_after_superseded_completion'
                chunk_id = $claim.chunk_id
                run_id = $claim.run_id
                automation_id = $claim.automation_id
                at = Get-IsoNow
            }))
            continue
        }

        $terminalRecord = $null
        if ($claim.run_id -and $terminalRuns.ContainsKey([string]$claim.run_id)) {
            $terminalRecord = $terminalRuns[[string]$claim.run_id]
        }

        if ($null -ne $terminalRecord -and [string]$terminalRecord.automation_id -eq [string]$claim.automation_id) {
            $claimStartedAt = [DateTimeOffset]::MinValue
            $claimStartedParsed = [DateTimeOffset]::TryParse([string]$claim.claimed_at, [ref]$claimStartedAt)
            if (-not $claimStartedParsed -or $terminalRecord.timestamp -ge $claimStartedAt) {
                $evidenceText = [string]$terminalRecord.evidence
                if ($terminalRecord.result -eq 'implemented') {
                    $ledger.completed = @(@(Convert-Array $ledger.completed) + ([ordered]@{
                        chunk_id = $claim.chunk_id
                        run_id = $claim.run_id
                        automation_id = $claim.automation_id
                        at = Get-IsoNow
                        evidence = $evidenceText
                        commit = ''
                    }))
                    $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
                        event = 'completed_from_terminal_log'
                        chunk_id = $claim.chunk_id
                        run_id = $claim.run_id
                        automation_id = $claim.automation_id
                        at = Get-IsoNow
                        evidence = $evidenceText
                    }))
                } elseif ($terminalRecord.result -eq 'blocked') {
                    $ledger.blocked = @(@(Convert-Array $ledger.blocked) + ([ordered]@{
                        chunk_id = $claim.chunk_id
                        run_id = $claim.run_id
                        automation_id = $claim.automation_id
                        at = Get-IsoNow
                        evidence = $evidenceText
                        commit = ''
                    }))
                    $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
                        event = 'blocked_from_terminal_log'
                        chunk_id = $claim.chunk_id
                        run_id = $claim.run_id
                        automation_id = $claim.automation_id
                        at = Get-IsoNow
                        evidence = $evidenceText
                    }))
                } else {
                    $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
                        event = 'released_after_terminal_log'
                        chunk_id = $claim.chunk_id
                        run_id = $claim.run_id
                        automation_id = $claim.automation_id
                        at = Get-IsoNow
                        evidence = "$($terminalRecord.result): $evidenceText".TrimEnd(':', ' ')
                    }))
                }
                continue
            }
        }

        $activeClaims.Add($claim)
    }
    $ledger.active_claims = @($activeClaims.ToArray())

    if ($Mode -eq 'claim') {
        $completedIds = @{}
        foreach ($done in (Convert-Array $ledger.completed)) {
            if ($done.chunk_id) {
                $completedIds[[string]$done.chunk_id] = $true
            }
        }
        foreach ($chunk in (Convert-Array $queue.chunks)) {
            if ($chunk.status -in @('done', 'verified', 'completed')) {
                $completedIds[[string]$chunk.id] = $true
            }
        }

        $activeIds = @{}
        foreach ($claim in (Convert-Array $ledger.active_claims)) {
            if ($claim.chunk_id) {
                $activeIds[[string]$claim.chunk_id] = $true
            }
        }

        $selected = $null
        foreach ($chunk in (Convert-Array $queue.chunks)) {
            $id = [string]$chunk.id
            if (-not $id) {
                continue
            }
            if ($completedIds.ContainsKey($id) -or $activeIds.ContainsKey($id)) {
                continue
            }
            if ($chunk.status -notin @('ready', 'not_started')) {
                continue
            }

            $depsSatisfied = $true
            foreach ($dep in (Convert-Array $chunk.dependencies)) {
                if (-not $completedIds.ContainsKey([string]$dep)) {
                    $depsSatisfied = $false
                    break
                }
            }
            if (-not $depsSatisfied) {
                continue
            }

            $selected = $chunk
            break
        }

        if ($null -eq $selected) {
            $ledger.updated_at = Get-IsoNow
            Write-JsonFile -Path $ledgerFullPath -Value $ledger
            [ordered]@{
                state = 'no_claim_available'
                run_id = $RunId
                queue_path = $QueuePath
                ledger_path = $LedgerPath
            } | ConvertTo-Json -Depth 20
            return
        }

        $claimRecord = [ordered]@{
            chunk_id = $selected.id
            title = $selected.title
            automation_id = $AutomationId
            run_id = $RunId
            claimed_at = Get-IsoNow
            lease_expires_at = $now.AddMinutes($LeaseMinutes).ToString('o')
        }

        $ledger.active_claims = @(@(Convert-Array $ledger.active_claims) + $claimRecord)
        $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
            event = 'claimed'
            chunk_id = $selected.id
            run_id = $RunId
            automation_id = $AutomationId
            at = Get-IsoNow
        }))
        $ledger.updated_at = Get-IsoNow
        Write-JsonFile -Path $ledgerFullPath -Value $ledger

        [ordered]@{
            state = 'claimed'
            run_id = $RunId
            automation_id = $AutomationId
            lease_expires_at = $claimRecord.lease_expires_at
            chunk = $selected
            ledger_path = $LedgerPath
        } | ConvertTo-Json -Depth 30
        return
    }

    if (-not $ChunkId) {
        throw "-ChunkId is required for Mode '$Mode'"
    }

    $remainingClaims = @()
    foreach ($claim in (Convert-Array $ledger.active_claims)) {
        if ([string]$claim.chunk_id -ne $ChunkId -or ([string]$claim.run_id -ne $RunId -and $RunId)) {
            $remainingClaims += $claim
        }
    }
    $ledger.active_claims = @($remainingClaims)

    $eventName = if ($Mode -eq 'complete') { 'completed' } else { 'blocked' }
    $record = [ordered]@{
        chunk_id = $ChunkId
        run_id = $RunId
        automation_id = $AutomationId
        at = Get-IsoNow
        evidence = $Evidence
        commit = $Commit
    }

    if ($Mode -eq 'complete') {
        $ledger.completed = @(@(Convert-Array $ledger.completed) + $record)
    } else {
        $ledger.blocked = @(@(Convert-Array $ledger.blocked) + $record)
    }

    $ledger.history = @(@(Convert-Array $ledger.history) + ([ordered]@{
        event = $eventName
        chunk_id = $ChunkId
        run_id = $RunId
        automation_id = $AutomationId
        at = Get-IsoNow
        evidence = $Evidence
        commit = $Commit
    }))
    $ledger.updated_at = Get-IsoNow
    Write-JsonFile -Path $ledgerFullPath -Value $ledger

    [ordered]@{
        state = $eventName
        run_id = $RunId
        automation_id = $AutomationId
        chunk_id = $ChunkId
        ledger_path = $LedgerPath
    } | ConvertTo-Json -Depth 20
} finally {
    $lockStream.Close()
    Remove-Item -LiteralPath $lockPath -Force -ErrorAction SilentlyContinue
}
