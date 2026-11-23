@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tools\output\Invoke-OutputFidelity.ps1" -Suite Screen -Tier NonESU
set "screen_result=%errorlevel%"
echo.
echo Keep the complete screen result directory for review.
pause
exit /b %screen_result%
