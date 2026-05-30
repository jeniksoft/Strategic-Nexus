@echo off
setlocal

REM Ensure CODEX_HOME is available for automations that use it.
if "%CODEX_HOME%"=="" set "CODEX_HOME=%USERPROFILE%\.codex"

REM Prefer UTF-8 console to reduce mojibake in owner-facing outputs.
chcp 65001 >nul

REM Wrapper for environments where PowerShell script execution is restricted.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_user_task_board_shortcut.ps1" %*
exit /b %errorlevel%

