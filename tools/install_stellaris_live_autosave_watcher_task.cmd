@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install_stellaris_live_autosave_watcher_task.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"
exit /b %EXIT_CODE%