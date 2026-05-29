@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_end_to_end_spine_smoke_tests.ps1" %*
exit /b %errorlevel%

