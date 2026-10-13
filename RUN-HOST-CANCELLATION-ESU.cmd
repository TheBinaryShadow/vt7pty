@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostCancellationProbe.ps1" -Tier ESU %*
set "host_cancellation_result=%errorlevel%"
echo.
echo Keep the entire results folder, including failures.
pause
exit /b %host_cancellation_result%
