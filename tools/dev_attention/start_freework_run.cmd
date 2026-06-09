@echo off
setlocal
if "%CODEX_HOME%"=="" set "CODEX_HOME=%USERPROFILE%\.codex"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0start_freework_run.ps1" %*
exit /b %ERRORLEVEL%
