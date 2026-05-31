@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0smoke_snc_tray.ps1" %*
exit /b %ERRORLEVEL%
