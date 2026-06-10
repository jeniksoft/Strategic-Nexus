param()

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath '.').Path
$claimScript = Join-Path $repoRoot 'tools/dev_attention/claim_v0_sprint_chunk.ps1'

function Assert-Equal {
    param(
        [object]$Actual,
        [object]$Expected,
        [string]$Message
    )

    if ([string]$Actual -ne [string]$Expected) {
        throw "$Message Expected '$Expected', got '$Actual'."
    }
}

function Write-TestQueue {
    param(
        [string]$Path,
        [object[]]$Chunks
    )

    [ordered]@{
        schema_version = 1
        mode = 'claim_helper_test'
        updated_at = [DateTimeOffset]::Now.ToString('o')
        chunks = @($Chunks)
    } | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Read-TestQueue {
    param([string]$Path)

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-TestChunkStatus {
    param(
        [string]$Path,
        [string]$ChunkId
    )

    $queue = Read-TestQueue -Path $Path
    $chunk = @($queue.chunks) | Where-Object { [string]$_.id -eq $ChunkId } | Select-Object -First 1
    if ($null -eq $chunk) {
        throw "Missing test chunk '$ChunkId'."
    }

    return [string]$chunk.status
}

function Invoke-ClaimHelper {
    param([string[]]$Arguments)

    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $claimScript @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "claim_v0_sprint_chunk.ps1 failed with exit code $LASTEXITCODE."
    }

    return (($output | Out-String).Trim() | ConvertFrom-Json)
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("sn-claim-helper-test-{0}" -f ([Guid]::NewGuid().ToString('N')))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    $queuePath = Join-Path $tempRoot 'queue.json'
    $ledgerPath = Join-Path $tempRoot 'ledger.json'
    $logPath = Join-Path $tempRoot 'automation_log.csv'

    Write-TestQueue -Path $queuePath -Chunks @(
        [ordered]@{ id = 'alpha'; title = 'Alpha'; status = 'ready'; dependencies = @() },
        [ordered]@{ id = 'beta'; title = 'Beta'; status = 'ready'; dependencies = @() }
    )

    $claim = Invoke-ClaimHelper -Arguments @(
        '-AutomationId', 'test-worker',
        '-RunId', 'run-alpha',
        '-QueuePath', $queuePath,
        '-LedgerPath', $ledgerPath,
        '-AutomationLogPath', $logPath
    )
    Assert-Equal -Actual $claim.state -Expected 'claimed' -Message 'First claim state mismatch.'
    Assert-Equal -Actual $claim.chunk.id -Expected 'alpha' -Message 'First claim chunk mismatch.'

    $complete = Invoke-ClaimHelper -Arguments @(
        '-Mode', 'complete',
        '-AutomationId', 'test-worker',
        '-RunId', 'run-alpha',
        '-ChunkId', 'alpha',
        '-Evidence', 'test complete',
        '-Commit', 'abc123',
        '-QueuePath', $queuePath,
        '-LedgerPath', $ledgerPath,
        '-AutomationLogPath', $logPath
    )
    Assert-Equal -Actual $complete.state -Expected 'completed' -Message 'Complete state mismatch.'
    Assert-Equal -Actual (Get-TestChunkStatus -Path $queuePath -ChunkId 'alpha') -Expected 'done' -Message 'Completed chunk queue status mismatch.'

    $claim2 = Invoke-ClaimHelper -Arguments @(
        '-AutomationId', 'test-worker',
        '-RunId', 'run-beta',
        '-QueuePath', $queuePath,
        '-LedgerPath', $ledgerPath,
        '-AutomationLogPath', $logPath
    )
    Assert-Equal -Actual $claim2.state -Expected 'claimed' -Message 'Second claim state mismatch.'
    Assert-Equal -Actual $claim2.chunk.id -Expected 'beta' -Message 'Second claim chunk mismatch.'

    $block = Invoke-ClaimHelper -Arguments @(
        '-Mode', 'block',
        '-AutomationId', 'test-worker',
        '-RunId', 'run-beta',
        '-ChunkId', 'beta',
        '-Evidence', 'test blocked',
        '-QueuePath', $queuePath,
        '-LedgerPath', $ledgerPath,
        '-AutomationLogPath', $logPath
    )
    Assert-Equal -Actual $block.state -Expected 'blocked' -Message 'Block state mismatch.'
    Assert-Equal -Actual (Get-TestChunkStatus -Path $queuePath -ChunkId 'beta') -Expected 'blocked' -Message 'Blocked chunk queue status mismatch.'

    $claim3 = Invoke-ClaimHelper -Arguments @(
        '-AutomationId', 'test-worker',
        '-RunId', 'run-none',
        '-QueuePath', $queuePath,
        '-LedgerPath', $ledgerPath,
        '-AutomationLogPath', $logPath
    )
    Assert-Equal -Actual $claim3.state -Expected 'no_claim_available' -Message 'Blocked chunk was claimable again.'

    $queuePath2 = Join-Path $tempRoot 'queue-terminal.json'
    $ledgerPath2 = Join-Path $tempRoot 'ledger-terminal.json'
    $logPath2 = Join-Path $tempRoot 'automation_log_terminal.csv'

    Write-TestQueue -Path $queuePath2 -Chunks @(
        [ordered]@{ id = 'gamma'; title = 'Gamma'; status = 'ready'; dependencies = @() }
    )

    $claim4 = Invoke-ClaimHelper -Arguments @(
        '-AutomationId', 'test-worker',
        '-RunId', 'run-gamma',
        '-QueuePath', $queuePath2,
        '-LedgerPath', $ledgerPath2,
        '-AutomationLogPath', $logPath2
    )
    Assert-Equal -Actual $claim4.state -Expected 'claimed' -Message 'Terminal claim state mismatch.'
    Assert-Equal -Actual $claim4.chunk.id -Expected 'gamma' -Message 'Terminal claim chunk mismatch.'

    @([pscustomobject]@{
        timestamp_local = (Get-Date).AddSeconds(1).ToString('yyyy-MM-dd HH:mm:ss')
        timezone = 'Central Europe Standard Time'
        automation_id = 'test-worker'
        run_id = 'run-gamma'
        result = 'implemented'
        summary = 'terminal complete'
        details = 'from log'
    }) | Export-Csv -LiteralPath $logPath2 -NoTypeInformation -Encoding UTF8

    $sync = Invoke-ClaimHelper -Arguments @(
        '-Mode', 'sync',
        '-AutomationId', 'test-worker',
        '-RunId', 'run-sync',
        '-QueuePath', $queuePath2,
        '-LedgerPath', $ledgerPath2,
        '-AutomationLogPath', $logPath2
    )
    Assert-Equal -Actual $sync.state -Expected 'synced' -Message 'Sync state mismatch.'
    Assert-Equal -Actual (Get-TestChunkStatus -Path $queuePath2 -ChunkId 'gamma') -Expected 'done' -Message 'Terminal log completion did not update queue.'
    Assert-Equal -Actual $sync.active_claim_count -Expected 0 -Message 'Terminal log completion did not release active claim.'

    Write-Host 'claim_v0_sprint_chunk tests passed'
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
