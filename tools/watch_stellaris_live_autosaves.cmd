@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%watch_stellaris_live_autosaves.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"
exit /b %EXIT_CODE%