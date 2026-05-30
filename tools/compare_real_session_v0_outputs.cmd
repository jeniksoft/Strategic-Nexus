@echo off
setlocal
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%compare_real_session_v0_outputs.ps1" %*
set EXIT_CODE=%ERRORLEVEL%
exit /b %EXIT_CODE%
