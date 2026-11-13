@echo off
setlocal
set "freeze_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "freeze_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%freeze_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostFreezeDiagnostics.ps1" -Tier NonESU %*
set "freeze_result=%errorlevel%"
echo.
echo Keep the entire results directory, including selected-state failures.
pause
exit /b %freeze_result%
