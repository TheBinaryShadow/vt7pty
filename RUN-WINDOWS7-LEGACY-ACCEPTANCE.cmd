@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\platform\Invoke-Windows7Acceptance.ps1" -Tier Legacy
exit /b %errorlevel%
