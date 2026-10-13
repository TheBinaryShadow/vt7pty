@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostIdentityProbe.ps1" -Tier Legacy %*
set "host_identity_result=%errorlevel%"
echo.
echo Keep the entire results folder, including failures.
pause
exit /b %host_identity_result%
