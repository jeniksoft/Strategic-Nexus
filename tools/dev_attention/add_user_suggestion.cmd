@echo off
setlocal

REM Wrapper for environments where PowerShell script execution is restricted.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0add_user_suggestion.ps1" %*

