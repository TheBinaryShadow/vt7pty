@echo off
setlocal
set "coordination_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "coordination_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%coordination_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-HostGuards.ps1" -Tier ESU %*
set "coordination_result=%errorlevel%"
echo.
echo Keep all results, including selected-state diagnostic failures.
pause
exit /b %coordination_result%
