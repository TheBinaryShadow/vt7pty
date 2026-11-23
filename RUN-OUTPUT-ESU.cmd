@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tools\output\Invoke-OutputFidelity.ps1" -Tier ESU
set "output_result=%errorlevel%"
echo.
echo Keep the complete output result directory for review.
pause
exit /b %output_result%
