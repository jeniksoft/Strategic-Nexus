$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "dist\private_tools"
$outputPath = Join-Path $outputDir "StrategicNexusCompanionTray.exe"
$objectDir = Join-Path $outputDir "obj\StrategicNexusCompanionTray"
$resourceFile = Join-Path $repoRoot "resources\StrategicNexusCompanion.rc"
$resourceOutput = Join-Path $objectDir "StrategicNexusCompanionTray.res"

if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
if (-not (Test-Path -LiteralPath $objectDir)) {
    New-Item -ItemType Directory -Force -Path $objectDir | Out-Null
}
Get-ChildItem -LiteralPath $objectDir -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
$objectOutputArg = "/Fo$objectDir\\"

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

if (Test-Path -LiteralPath $resourceOutput) {
    Remove-Item -LiteralPath $resourceOutput -Force -ErrorAction SilentlyContinue
}

& rc.exe /nologo /fo $resourceOutput $resourceFile
if ($LASTEXITCODE -ne 0) {
    throw "SNC tray resource compile failed with exit code $LASTEXITCODE"
}

$sourceFiles = @(
    (Join-Path $repoRoot "src\SncTrayApp.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiver.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiveVerifier.cpp"),
    (Join-Path $repoRoot "src\AutosaveArchiveSummarizer.cpp"),
    (Join-Path $repoRoot "src\PostPlayPackageBuilder.cpp"),
    (Join-Path $repoRoot "src\SaveEntryPointAnalyzer.cpp"),
    (Join-Path $repoRoot "src\SaveParser.cpp"),
    (Join-Path $repoRoot "src\SncCandidateDecisionPackageBuilder.cpp"),
    (Join-Path $repoRoot "src\SncDecisionInputPackageBuilder.cpp"),
    (Join-Path $repoRoot "src\SncDslDraftPackageBuilder.cpp"),
    (Join-Path $repoRoot "src\SncFriendPackage.cpp"),
    (Join-Path $repoRoot "src\LocalLlmModelManager.cpp"),
    (Join-Path $repoRoot "src\SncPostPlayArtifactBackfiller.cpp"),
    (Join-Path $repoRoot "src\StrategicNexusCompanion.cpp"),
    (Join-Path $repoRoot "src\SncGeneratedOverlayPublishGate.cpp"),
    (Join-Path $repoRoot "src\SncGeneratedOverlayStager.cpp"),
    (Join-Path $repoRoot "src\StellarisProcessDetector.cpp"),
    (Join-Path $repoRoot "src\StellarisSavePathResolver.cpp"),
    (Join-Path $repoRoot "src\common\FileUtil.cpp"),
    (Join-Path $repoRoot "src\common\JsonExtract.cpp"),
    (Join-Path $repoRoot "src\common\JsonSanity.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\DslParser.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\DslValidator.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\GeneratedOverlayPublisher.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\ManifestVerifier.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\MpOverlayPackage.cpp"),
    (Join-Path $repoRoot "src\generated_overlay\OverlayCompiler.cpp")
)

& cl.exe `
    /nologo `
    /std:c++20 `
    /EHsc `
    /DUNICODE `
    /D_UNICODE `
    /I (Join-Path $repoRoot "src") `
    $objectOutputArg `
    $sourceFiles `
    /Fe:$outputPath `
    /link `
    /SUBSYSTEM:WINDOWS `
    $resourceOutput `
    Shell32.lib `
    User32.lib `
    Gdi32.lib

if ($LASTEXITCODE -ne 0) {
    throw "SNC tray build failed with exit code $LASTEXITCODE"
}

$sncTrayStagerObject = Join-Path $objectDir "SncGeneratedOverlayStager.obj"
if (-not (Test-Path -LiteralPath $sncTrayStagerObject)) {
    & cl.exe `
        /nologo `
        /std:c++20 `
        /EHsc `
        /DUNICODE `
        /D_UNICODE `
        /I (Join-Path $repoRoot "src") `
        "/Fo$objectDir\" `
        /c (Join-Path $repoRoot "src\SncGeneratedOverlayStager.cpp")
    if ($LASTEXITCODE -ne 0) {
        throw "SNC tray fallback rebuild for SncGeneratedOverlayStager.cpp failed with exit code $LASTEXITCODE"
    }
}

Write-Host "snc_tray_build_success=true"
Write-Host "snc_tray_exe=$outputPath"
