param(
    [string]$StatusPath,
    [string]$OutputPath = "dist/private_reports/snc_support_report_preview.txt",
    [string]$SupportEmail = "support@jeniksoft.cz"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Get-PathOrNull {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

function Resolve-ExistingStatusPath {
    param([string]$RequestedPath)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        $candidates += $RequestedPath
    } else {
        $candidates += @(
            "dist/private_reports/snc_tray_status.json",
            "dist/snc_status_snapshot.json",
            "dist/private_reports/snc_status_snapshot.json"
        )
    }

    foreach ($candidate in $candidates) {
        $resolved = Get-PathOrNull -Path $candidate
        if ($null -ne $resolved -and (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            return $resolved
        }
    }

    throw "No SNC status JSON found for support report preview."
}

function Get-ObjectValue {
    param(
        $Object,
        [string[]]$Path
    )

    $current = $Object
    foreach ($segment in $Path) {
        if ($null -eq $current) {
            return $null
        }

        $property = $current.PSObject.Properties[$segment]
        if ($null -eq $property) {
            return $null
        }

        $current = $property.Value
    }

    return $current
}

function Get-FirstValue {
    param(
        $Object,
        [object[]]$CandidatePaths
    )

    foreach ($candidatePath in $CandidatePaths) {
        $value = Get-ObjectValue -Object $Object -Path $candidatePath
        if ($null -ne $value -and -not [string]::IsNullOrWhiteSpace([string]$value)) {
            return [string]$value
        }
    }

    return ""
}

function Get-FirstBoolText {
    param(
        $Object,
        [object[]]$CandidatePaths
    )

    foreach ($candidatePath in $CandidatePaths) {
        $value = Get-ObjectValue -Object $Object -Path $candidatePath
        if ($value -is [bool]) {
            return ($(if ($value) { "true" } else { "false" }))
        }
        if ($null -ne $value -and -not [string]::IsNullOrWhiteSpace([string]$value)) {
            $text = [string]$value
            if ($text -eq "true" -or $text -eq "false") {
                return $text
            }
        }
    }

    return ""
}

function Format-PreviewPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    $normalized = $Path.Replace('\', '/')
    if ($normalized -match '^[A-Za-z]:/' -or $normalized.StartsWith('//')) {
        $leaf = Split-Path -Leaf $normalized
        if ([string]::IsNullOrWhiteSpace($leaf)) {
            return "[absolute-path-redacted]"
        }
        return "[absolute-path-redacted]/$leaf"
    }

    return $normalized
}

$resolvedStatusPath = Resolve-ExistingStatusPath -RequestedPath $StatusPath
$resolvedOutputPath = Get-PathOrNull -Path $OutputPath
if ($null -eq $resolvedOutputPath) {
    throw "Support report output path is empty."
}

$outputDirectory = Split-Path -Parent $resolvedOutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$status = Get-Content -Raw -LiteralPath $resolvedStatusPath | ConvertFrom-Json

$state = Get-FirstValue -Object $status -CandidatePaths @(
    @("state"),
    @("status_center", "state")
)
$reason = Get-FirstValue -Object $status -CandidatePaths @(
    @("reason"),
    @("status_center", "reason")
)
$nextAction = Get-FirstValue -Object $status -CandidatePaths @(
    @("next_action")
)
$nextActionReason = Get-FirstValue -Object $status -CandidatePaths @(
    @("next_action_reason")
)
$nextActionPath = Get-FirstValue -Object $status -CandidatePaths @(
    @("next_action_path")
)
$nextStepsBriefPath = Get-FirstValue -Object $status -CandidatePaths @(
    @("next_steps_brief_path")
)
$ownerTestPlaybookPath = Get-FirstValue -Object $status -CandidatePaths @(
    @("owner_test_playbook_path")
)
$startupLifecycleState = Get-FirstValue -Object $status -CandidatePaths @(
    @("startup_lifecycle_state")
)
$startWithWindowsEnabled = Get-FirstBoolText -Object $status -CandidatePaths @(
    @("start_with_windows_enabled")
)
$publishGateState = Get-FirstValue -Object $status -CandidatePaths @(
    @("generated_overlay_publish_gate_state"),
    @("generated_overlay_publish_gate_status", "state")
)
$publishGateReason = Get-FirstValue -Object $status -CandidatePaths @(
    @("generated_overlay_publish_gate_reason"),
    @("generated_overlay_publish_gate_status", "reason")
)
$mpPackageState = Get-FirstValue -Object $status -CandidatePaths @(
    @("mp_overlay_package_state"),
    @("mp_overlay_package_status", "state")
)
$mpPackageReason = Get-FirstValue -Object $status -CandidatePaths @(
    @("mp_overlay_package_reason"),
    @("mp_overlay_package_status", "reason")
)
$mode = Get-FirstValue -Object $status -CandidatePaths @(
    @("mode")
)
$generatedAtLocal = Get-FirstValue -Object $status -CandidatePaths @(
    @("updated_at_local"),
    @("generated_at_local")
)

$lines = @(
    "Strategic Nexus Companion - local support report preview",
    "Generated at: $generatedAtLocal",
    "Source mode: $mode",
    "Source status artifact: $(Format-PreviewPath -Path $resolvedStatusPath)",
    "",
    "Manual send only: explicit owner approval required before any send to $SupportEmail",
    "Raw saves included: no",
    "Privacy note: paths are summarized and raw save payloads stay excluded by default.",
    "",
    "Current status center state: $state",
    "Current status center reason: $reason",
    "Next action: $nextAction",
    "Next action reason: $nextActionReason",
    "Next action path: $(Format-PreviewPath -Path $nextActionPath)",
    "",
    "Startup lifecycle state: $startupLifecycleState",
    "Start with Windows enabled: $startWithWindowsEnabled",
    "Publish gate state: $publishGateState",
    "Publish gate reason: $publishGateReason",
    "MP package state: $mpPackageState",
    "MP package reason: $mpPackageReason",
    "",
    "Data categories included:",
    "- status center state and next-action guidance",
    "- startup lifecycle metadata",
    "- generated overlay publish-gate readiness",
    "- MP package readiness metadata",
    "- local artifact references with summarized paths",
    "",
    "Data categories intentionally excluded by default:",
    "- raw save files",
    "- usernames, machine identifiers, and unrelated personal data where avoidable",
    "",
    "Helpful local artifacts:",
    "- next steps brief: $(Format-PreviewPath -Path $nextStepsBriefPath)",
    "- owner test playbook: $(Format-PreviewPath -Path $ownerTestPlaybookPath)",
    "- support report preview: $(Format-PreviewPath -Path $resolvedOutputPath)",
    "",
    "Manual send target:",
    "- $SupportEmail"
)

Set-Content -LiteralPath $resolvedOutputPath -Value ($lines -join "`r`n") -Encoding UTF8

Write-Output ("support_report_preview_path=" + ($resolvedOutputPath.Replace('\', '/')))
Write-Output "support_report_preview_ready=true"
Write-Output "support_report_send_requires_approval=true"
