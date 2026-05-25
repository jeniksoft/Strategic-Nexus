param(
    [string]$DocsPath = "docs"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$root = if ([System.IO.Path]::IsPathRooted($DocsPath)) {
    $DocsPath
} else {
    Join-Path $repoRoot $DocsPath
}

if (-not (Test-Path -LiteralPath $root)) {
    throw "Docs path not found: $root"
}

$mojibakeMarkers = @(
    [string][char]0x00C3,
    [string][char]0x00C2,
    [string][char]0x00E2,
    [string][char]0x00C4,
    [string][char]0x00C5,
    [string][char]0x0102
)

$failures = @()

Get-ChildItem -LiteralPath $root -Filter "*.md" -Recurse | Sort-Object FullName | ForEach-Object {
    $path = $_.FullName
    $text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    $relativePath = Resolve-Path -LiteralPath $path -Relative

    $nonAscii = @($text.ToCharArray() | Where-Object { [int][char]$_ -gt 127 })
    if ($nonAscii.Count -gt 0) {
        $sample = ($nonAscii | Select-Object -First 5 | ForEach-Object { "U+{0:X4}" -f [int][char]$_ }) -join ","
        $failures += "$relativePath contains non-ASCII characters: $sample"
    }

    foreach ($marker in $mojibakeMarkers) {
        if ($text.Contains($marker)) {
            $failures += "$relativePath contains mojibake marker '$marker'"
            break
        }
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Host "docs_encoding_failure=$_" }
    exit 1
}

Write-Host "docs_encoding_ok=true"
