@echo off
setlocal
if "%~1"=="" (
  echo Usage: %~nx0 NonESU ^| ESU
  exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\platform\Invoke-Windows7Acceptance.ps1" -Tier "%~1"
exit /b %errorlevel%
