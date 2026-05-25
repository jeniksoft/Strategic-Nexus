param(
    [string]$Id = "",
    [string]$Title = "Navrh Strategic Nexus",
    [string]$Category = "zlepseni",
    [string]$Proposal = "",
    [string]$Benefit = "",
    [string]$Why = "",
    [string]$Next = "",
    [string]$Severity = "normal",
    [string]$Status = "navrh",
    [string]$SuggestionFilePath = "dist/private_reports/user_suggestions.json",
    [switch]$ReplaceExisting
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "task_board_json_io.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$suggestionPath = if ([System.IO.Path]::IsPathRooted($SuggestionFilePath)) {
    $SuggestionFilePath
} else {
    Join-Path $repoRoot $SuggestionFilePath
}

if ([string]::IsNullOrWhiteSpace($Id)) {
    $stamp = (Get-Date).ToString("yyyyMMddHHmmss")
    $safeTitle = ($Title.ToLowerInvariant() -replace "[^a-z0-9]+", "-").Trim("-")
    if ([string]::IsNullOrWhiteSpace($safeTitle)) {
        $safeTitle = "navrh"
    }
    $Id = "suggestion-$stamp-$safeTitle"
}

Invoke-TaskBoardJsonMutation -Path $suggestionPath -Mutate {
    if (Test-Path -LiteralPath $suggestionPath) {
        $document = Get-Content -Raw -LiteralPath $suggestionPath | ConvertFrom-Json
        $suggestions = @($document.suggestions)
    } else {
        $suggestions = @()
    }

    if ($ReplaceExisting) {
        $suggestions = @($suggestions | Where-Object { $_.id -ne $Id })
    }

    $suggestions += [pscustomobject]@{
        id = $Id
        created_at = (Get-Date).ToString("o")
        title = $Title
        category = $Category
        severity = $Severity
        status = $Status
        proposal = $Proposal
        benefit = $Benefit
        why = $Why
        next = $Next
    }

    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        suggestions = @($suggestions)
    }
}
Write-Host "user_suggestion_written=$Id"
