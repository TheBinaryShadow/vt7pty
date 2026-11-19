@echo off
setlocal
set "input_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "input_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%input_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\input\Invoke-InputFidelity.ps1" -Tier NonESU %*
set "input_result=%errorlevel%"
echo.
echo Keep the complete input results, including failures.
pause
exit /b %input_result%
