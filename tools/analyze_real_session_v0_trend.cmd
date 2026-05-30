@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "CODEX_HOME_FALLBACK=%USERPROFILE%\.codex"
if not defined CODEX_HOME set "CODEX_HOME=%CODEX_HOME_FALLBACK%"
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%analyze_real_session_v0_trend.ps1" %*
set "EXITCODE=%ERRORLEVEL%"
exit /b %EXITCODE%
