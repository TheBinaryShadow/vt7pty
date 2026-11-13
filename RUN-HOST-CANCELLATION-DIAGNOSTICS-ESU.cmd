@echo off
setlocal
set "diagnostic_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "diagnostic_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%diagnostic_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostCancellationDiagnostics.ps1" -Tier ESU %*
set "host_diagnostic_result=%errorlevel%"
echo.
echo Keep the entire results folder, including failed control and traced trials.
pause
exit /b %host_diagnostic_result%
