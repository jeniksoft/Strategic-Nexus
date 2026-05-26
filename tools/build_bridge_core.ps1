param(
    [string]$Output = "dist/bridge_core_test.exe"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path $repoRoot $Output
$outputDir = Split-Path -Parent $outputPath

if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/bridge_core") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "src/common") -Filter "*.cpp" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $knownVsDevCmdPaths = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    )
    $vsDevCmd = $knownVsDevCmdPaths |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1

    if (-not $vsDevCmd) {
        $vsDevCmd = Get-ChildItem -Path `
            "C:\Program Files\Microsoft Visual Studio", `
            "C:\Program Files (x86)\Microsoft Visual Studio" `
            -Recurse `
            -Filter "VsDevCmd.bat" `
            -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (-not $vsDevCmd) {
        throw "cl.exe was not found and VsDevCmd.bat could not be located. Install Visual Studio C++ tools or run from a Developer PowerShell."
    }

    $scriptPath = $MyInvocation.MyCommand.Path
    & cmd.exe /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && powershell -NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`" -Output `"$Output`""
    exit $LASTEXITCODE
}

Push-Location $repoRoot
try {
    & cl.exe /nologo /std:c++20 /EHsc /I "src" /I "src/bridge_core" $sourceFiles /Fe:$outputPath
} finally {
    Pop-Location
}
