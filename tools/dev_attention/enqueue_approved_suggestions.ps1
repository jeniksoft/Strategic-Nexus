param(
    [string]$SuggestionFilePath = "dist/private_reports/user_suggestions.json",
    [string]$QueuePath = "tools/dev_attention/v0_sprint_chunk_queue.json",
    [string]$LedgerPath = ".codex_local/v0_sprint_chunk_ledger.json"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
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

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    Ensure-Directory $Path
    $json = $Value | ConvertTo-Json -Depth 30
    $lines = $json -split "`r?`n" | ForEach-Object {
        $line = $_ -replace '":  ', '": '
        if ($line -match '^(\s+)(.*)$') {
            (' ' * ([int]($matches[1].Length / 2))) + $matches[2]
        } else {
            $line
        }
    }

    $temporaryPath = "$Path.tmp.$PID.$([Guid]::NewGuid().ToString('N'))"
    [System.IO.File]::WriteAllText(
        (Join-Path (Resolve-Path -LiteralPath (Split-Path -Parent $Path)).Path (Split-Path -Leaf $temporaryPath)),
        (($lines -join "`n") + "`n"),
        [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
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

function Get-SafeIdPart {
    param([string]$Text)

    $safe = ([string]$Text).ToLowerInvariant() -replace "[^a-z0-9]+", "-"
    $safe = $safe.Trim("-")

    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "approved-suggestion"
    }

    if ($safe.Length -gt 84) {
        return $safe.Substring(0, 84).Trim("-")
    }

    return $safe
}

function Test-ImplementationApprovedStatus {
    param([string]$Status)

    $normalized = ([string]$Status).Trim().ToLowerInvariant()
    return @(
        "schvaleno-k-realizaci",
        "prijato-k-implementaci",
        "schvaleno-k-implementaci"
    ) -contains $normalized
}

function Get-TextOrFallback {
    param(
        [object]$Value,
        [string]$Fallback
    )

    $text = ([string]$Value).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $Fallback
    }

    return $text
}

$suggestionPath = Resolve-ProjectPath $SuggestionFilePath
$queueFullPath = Resolve-ProjectPath $QueuePath
$ledgerFullPath = Resolve-ProjectPath $LedgerPath

$suggestionDocument = Read-JsonFile -Path $suggestionPath -DefaultValue ([pscustomobject]@{
    schema_version = 1
    updated_at = ""
    suggestions = @()
})
$ledger = Read-JsonFile -Path $ledgerFullPath -DefaultValue ([pscustomobject]@{
    schema_version = 1
    active_claims = @()
    completed = @()
    blocked = @()
    history = @()
})

$completedChunkIds = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($completed in (Convert-Array $ledger.completed)) {
    $id = [string]$completed.chunk_id
    if (-not [string]::IsNullOrWhiteSpace($id)) {
        [void]$completedChunkIds.Add($id)
    }
}

$createdIds = New-Object System.Collections.Generic.List[string]
$alreadyQueuedCount = 0
$notApprovedCount = 0

$lockPath = "$queueFullPath.lock"
Ensure-Directory $lockPath
$lockStream = $null
$deadline = [DateTime]::UtcNow.AddSeconds(10)
while ($null -eq $lockStream) {
    try {
        $lockStream = [System.IO.File]::Open(
            $lockPath,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None)
    } catch {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Timed out waiting for sprint queue lock: $lockPath"
        }
        Start-Sleep -Milliseconds 50
    }
}

try {
    $queue = Read-JsonFile -Path $queueFullPath -DefaultValue $null
    if ($null -eq $queue) {
        throw "Sprint queue not found or empty: $queueFullPath"
    }

    $chunks = @(Convert-Array $queue.chunks)
    $existingSourceIds = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    $existingChunkIds = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($chunk in $chunks) {
        $chunkId = [string]$chunk.id
        if (-not [string]::IsNullOrWhiteSpace($chunkId)) {
            [void]$existingChunkIds.Add($chunkId)
            if ($chunk.status -in @("done", "verified", "completed")) {
                [void]$completedChunkIds.Add($chunkId)
            }
        }

        if ($chunk.PSObject.Properties.Name -contains "source_suggestion_id") {
            $sourceId = [string]$chunk.source_suggestion_id
            if (-not [string]::IsNullOrWhiteSpace($sourceId)) {
                [void]$existingSourceIds.Add($sourceId)
            }
        }
    }

    $newChunks = New-Object System.Collections.Generic.List[object]

    foreach ($suggestion in (Convert-Array $suggestionDocument.suggestions)) {
        $sourceId = [string]$suggestion.id
        if ([string]::IsNullOrWhiteSpace($sourceId)) {
            continue
        }

        if (-not (Test-ImplementationApprovedStatus -Status $suggestion.status)) {
            $notApprovedCount++
            continue
        }

        if ($existingSourceIds.Contains($sourceId)) {
            $alreadyQueuedCount++
            continue
        }

        $chunkId = "owner-suggestion-$((Get-SafeIdPart -Text $sourceId))"
        if ($existingChunkIds.Contains($chunkId) -or $completedChunkIds.Contains($chunkId)) {
            $alreadyQueuedCount++
            [void]$existingSourceIds.Add($sourceId)
            continue
        }

        $title = Get-TextOrFallback -Value $suggestion.title -Fallback $sourceId
        $proposal = Get-TextOrFallback -Value $suggestion.proposal -Fallback "Implement the owner-approved Task Board suggestion."
        $why = Get-TextOrFallback -Value $suggestion.why -Fallback "Owner explicitly approved this suggestion for implementation."
        $next = Get-TextOrFallback -Value $suggestion.next -Fallback "Implement the smallest safe slice, or mark blocked with a concrete reason."

        $chunk = [ordered]@{
            id = $chunkId
            title = "Realizovat navrh: $title"
            status = "ready"
            lane = "owner_approved_suggestion"
            source = "task_board"
            source_suggestion_id = $sourceId
            source_suggestion_status = [string]$suggestion.status
            source_suggestion_title = $title
            roadmap_anchor = "Task Board owner-approved suggestions"
            goal = $proposal
            context = $why
            next_step = $next
            acceptance = @(
                "Re-read the approved suggestion and confirm it still makes sense with current roadmap, safety, and owner-facing workflow.",
                "Implement the smallest safe useful slice, or mark this chunk blocked/obsolete with a concrete reason if it cannot be done without owner input.",
                "After completion, update the source suggestion/report context with evidence, tests, commit id, or a clear not-applicable reason."
            )
            preferred_tests = @()
            dependencies = @()
        }

        [void]$newChunks.Add([pscustomobject]$chunk)
        [void]$existingSourceIds.Add($sourceId)
        [void]$existingChunkIds.Add($chunkId)
        [void]$createdIds.Add($chunkId)
    }

    if ($newChunks.Count -gt 0) {
        $ownerChunks = @()
        $otherChunks = @()
        foreach ($chunk in $chunks) {
            if ([string]$chunk.lane -eq "owner_approved_suggestion") {
                $ownerChunks += $chunk
            } else {
                $otherChunks += $chunk
            }
        }

        $queue.chunks = @($ownerChunks + @($newChunks.ToArray()) + $otherChunks)
        $queue.updated_at = (Get-Date).ToString("o")
        $queue.current_source = "Task Board owner-approved suggestions + DEVELOPMENT_ROADMAP.md"
        $queue.selection_rule = "Take owner-approved suggestion chunks first, then the first ready unblocked roadmap chunk in order, unless current code already satisfies it; then verify and record evidence."
    }

    if ($newChunks.Count -gt 0) {
        Write-JsonFile -Path $queueFullPath -Value $queue
    }
} finally {
    if ($lockStream) {
        $lockStream.Dispose()
    }
    Remove-Item -LiteralPath $lockPath -ErrorAction SilentlyContinue
}

Write-Host "approved_suggestion_chunks_created=$($createdIds.Count)"
Write-Host "approved_suggestion_chunks_already_queued=$alreadyQueuedCount"
Write-Host "approved_suggestion_source=$SuggestionFilePath"
Write-Host "approved_suggestion_queue=$QueuePath"
if ($createdIds.Count -gt 0) {
    Write-Host "approved_suggestion_chunk_ids=$($createdIds -join ',')"
}
