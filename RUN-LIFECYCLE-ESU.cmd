@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tools\output\Invoke-OutputFidelity.ps1" -Suite Lifecycle -Tier ESU
set "lifecycle_result=%errorlevel%"
echo.
echo Keep the complete lifecycle result directory for review.
pause
exit /b %lifecycle_result%
