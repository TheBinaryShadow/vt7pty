@echo off
setlocal
set "h4_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "h4_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%h4_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\research\Invoke-H4Manual.ps1" -Check Elevated %*
set "h4_result=%errorlevel%"
echo.
echo Keep the complete H4 manual results, including any failure.
pause
exit /b %h4_result%
