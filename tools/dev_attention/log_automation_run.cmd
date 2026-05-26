@echo off
setlocal

REM Wrapper for environments where PowerShell script execution is restricted.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0log_automation_run.ps1" %*

