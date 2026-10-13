@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostDispatchProbe.ps1" -Tier NonESU %*
set "host_dispatch_result=%errorlevel%"
echo.
echo Keep the entire results folder, including failures.
pause
exit /b %host_dispatch_result%
