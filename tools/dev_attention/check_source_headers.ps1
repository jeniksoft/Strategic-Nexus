param(
    [string[]]$Roots = @("src", "tests")
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$requiredLines = @(
    "// SPDX-License-Identifier: MPL-2.0",
    "// Copyright (c) 2026 Antonin Jenik"
)

$extensions = @(".cpp", ".h", ".hpp")
$missing = @()

foreach ($root in $Roots) {
    $resolvedRoot = Join-Path $repoRoot $root
    if (-not (Test-Path -LiteralPath $resolvedRoot)) {
        continue
    }

    $files = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
        Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() }

    foreach ($file in $files) {
        $lines = Get-Content -LiteralPath $file.FullName -TotalCount 5
        foreach ($requiredLine in $requiredLines) {
            if ($lines -notcontains $requiredLine) {
                $missing += [pscustomobject]@{
                    path = [System.IO.Path]::GetRelativePath($repoRoot, $file.FullName)
                    missing = $requiredLine
                }
            }
        }
    }
}

if ($missing.Count -gt 0) {
    $missing | ConvertTo-Json -Depth 3
    throw "source_header_check_failed=$($missing.Count)"
}

Write-Host "source_headers_ok=true"
