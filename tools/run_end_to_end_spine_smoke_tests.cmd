@echo off
setlocal

REM Ensure CODEX_HOME is available for automations that use it.
if "%CODEX_HOME%"=="" set "CODEX_HOME=%USERPROFILE%\.codex"

REM Prefer UTF-8 console to reduce mojibake in outputs.
chcp 65001 >nul

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_end_to_end_spine_smoke_tests.ps1" %*
exit /b %errorlevel%
