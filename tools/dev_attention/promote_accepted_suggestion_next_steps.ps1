param(
    [string]$SuggestionFilePath = "dist/private_reports/user_suggestions.json"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "task_board_json_io.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$suggestionPath = if ([System.IO.Path]::IsPathRooted($SuggestionFilePath)) {
    $SuggestionFilePath
} else {
    Join-Path $repoRoot $SuggestionFilePath
}

function Get-SafeSuggestionIdPart {
    param([string]$Text)

    $safe = ($Text.ToLowerInvariant() -replace "[^a-z0-9]+", "-").Trim("-")
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "navazujici-krok"
    }

    if ($safe.Length -gt 64) {
        return $safe.Substring(0, 64).Trim("-")
    }

    return $safe
}

function Test-AcceptedSuggestionStatus {
    param([string]$Status)

    $normalized = ([string]$Status).Trim().ToLowerInvariant()
    return @("prijato", "schvaleno", "prijato-k-implementaci", "schvaleno-k-implementaci") -contains $normalized
}

$createdIds = New-Object System.Collections.Generic.List[string]

Invoke-TaskBoardJsonMutation -Path $suggestionPath -Mutate {
    if (Test-Path -LiteralPath $suggestionPath) {
        $document = Get-Content -Raw -LiteralPath $suggestionPath | ConvertFrom-Json
        $suggestions = @($document.suggestions)
    } else {
        $suggestions = @()
    }

    $existingFollowupSourceIds = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($suggestion in $suggestions) {
        if ($suggestion.PSObject.Properties.Name -contains "source_suggestion_id") {
            $sourceId = [string]$suggestion.source_suggestion_id
            if (-not [string]::IsNullOrWhiteSpace($sourceId)) {
                [void]$existingFollowupSourceIds.Add($sourceId)
            }
        }
    }

    foreach ($suggestion in @($suggestions)) {
        $sourceId = [string]$suggestion.id
        $next = [string]$suggestion.next

        if (-not (Test-AcceptedSuggestionStatus -Status $suggestion.status)) {
            continue
        }
        if ([string]::IsNullOrWhiteSpace($next)) {
            continue
        }
        if ($existingFollowupSourceIds.Contains($sourceId)) {
            continue
        }

        $idPart = Get-SafeSuggestionIdPart -Text $suggestion.title
        $followupId = "suggestion-followup-$idPart"
        if (@($suggestions | Where-Object { $_.id -eq $followupId }).Count -gt 0) {
            $stamp = (Get-Date).ToString("yyyyMMddHHmmss")
            $followupId = "suggestion-followup-$stamp-$idPart"
        }

        $followup = [pscustomobject]@{
            id = $followupId
            created_at = (Get-Date).ToString("o")
            title = "Navazujici krok: $($suggestion.title)"
            category = if ($suggestion.category) { $suggestion.category } else { "workflow" }
            severity = if ($suggestion.severity) { $suggestion.severity } else { "normal" }
            status = "navrh"
            proposal = $next
            benefit = "Dalsi prace z prijateho navrhu bude videt jako samostatna polozka a nezapadne v detailu."
            why = "Majitel nemusi rucne hledat navazujici kroky v uz prijatych navrzich."
            next = "Rozhodnout, jestli se tenhle navazujici krok ma prijmout a naplanovat."
            source_suggestion_id = $sourceId
            source_suggestion_title = [string]$suggestion.title
        }

        $suggestions += $followup
        [void]$existingFollowupSourceIds.Add($sourceId)
        [void]$createdIds.Add($followupId)
    }

    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        suggestions = @($suggestions)
    }
}

Write-Host "accepted_suggestion_followups_created=$($createdIds.Count)"
if ($createdIds.Count -gt 0) {
    Write-Host "accepted_suggestion_followup_ids=$($createdIds -join ',')"
}
