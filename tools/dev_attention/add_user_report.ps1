param(
    [string]$Id = "",
    [string]$Title = "Report Strategic Nexus",
    [string]$Category = "prace",
    [string]$Summary = "",
    [string]$Why = "",
    [string]$Next = "",
    [string]$Severity = "normal",
    [string]$ReportFilePath = "dist/private_reports/user_reports.json",
    [switch]$ReplaceExisting
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "task_board_json_io.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$reportPath = if ([System.IO.Path]::IsPathRooted($ReportFilePath)) {
    $ReportFilePath
} else {
    Join-Path $repoRoot $ReportFilePath
}

if ([string]::IsNullOrWhiteSpace($Id)) {
    $stamp = (Get-Date).ToString("yyyyMMddHHmmss")
    $safeTitle = ($Title.ToLowerInvariant() -replace "[^a-z0-9]+", "-").Trim("-")
    if ([string]::IsNullOrWhiteSpace($safeTitle)) {
        $safeTitle = "report"
    }
    $Id = "report-$stamp-$safeTitle"
}

Invoke-TaskBoardJsonMutation -Path $reportPath -Mutate {
    if (Test-Path -LiteralPath $reportPath) {
        $document = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
        $reports = @($document.reports)
    } else {
        $reports = @()
    }

    if ($ReplaceExisting) {
        $reports = @($reports | Where-Object { $_.id -ne $Id })
    }

    $reports += [pscustomobject]@{
        id = $Id
        created_at = (Get-Date).ToString("o")
        title = $Title
        category = $Category
        severity = $Severity
        summary = $Summary
        why = $Why
        next = $Next
    }

    [pscustomobject]@{
        schema_version = 1
        updated_at = (Get-Date).ToString("o")
        reports = @($reports)
    }
}
Write-Host "user_report_written=$Id"
if (Test-TaskBoardPathMatchesDefault -RepoRoot $repoRoot -Path $reportPath -DefaultRelativePath "dist/private_reports/user_reports.json") {
    if (Invoke-TaskBoardStateSync) {
        Write-Host "task_board_state_sync_invoked=true"
    }
}
