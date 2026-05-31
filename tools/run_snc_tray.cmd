@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_snc_tray.ps1" %*
exit /b %ERRORLEVEL%
