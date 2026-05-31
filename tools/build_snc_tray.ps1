$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "dist\private_tools"
$outputPath = Join-Path $outputDir "StrategicNexusCompanionTray.exe"

if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

function Invoke-WithDeveloperShell {
    param(
        [string]$ScriptPath
    )

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

    & cmd.exe /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && powershell -NoProfile -ExecutionPolicy Bypass -File `"$ScriptPath`""
    exit $LASTEXITCODE
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Invoke-WithDeveloperShell -ScriptPath $MyInvocation.MyCommand.Path
}

$sourceFiles = @(
    (Join-Path $repoRoot "src\SncTrayApp.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src\StellarisProcessDetector.cpp"),
    (Join-Path $repoRoot "src\StellarisSavePathResolver.cpp"),
    (Join-Path $repoRoot "src\common\FileUtil.cpp"),
    (Join-Path $repoRoot "src\common\JsonSanity.cpp")
)

& cl.exe `
    /nologo `
    /std:c++20 `
    /EHsc `
    /DUNICODE `
    /D_UNICODE `
    /I (Join-Path $repoRoot "src") `
    $sourceFiles `
    /Fe:$outputPath `
    /link `
    /SUBSYSTEM:WINDOWS `
    Shell32.lib `
    User32.lib `
    Gdi32.lib

if ($LASTEXITCODE -ne 0) {
    throw "SNC tray build failed with exit code $LASTEXITCODE"
}

Write-Host "snc_tray_build_success=true"
Write-Host "snc_tray_exe=$outputPath"
