@echo off
setlocal

REM Ensure CODEX_HOME is available for automations that use it.
if "%CODEX_HOME%"=="" set "CODEX_HOME=%USERPROFILE%\.codex"

REM Prefer UTF-8 console to reduce mojibake in outputs.
chcp 65001 >nul

REM Wrapper for environments where PowerShell script execution is restricted.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0check_docs_encoding.ps1" %*
exit /b %errorlevel%
