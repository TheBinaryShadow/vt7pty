@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fidelity\Invoke-FidelityBaseline.ps1" -Tier ESU %*
set "fidelity_result=%errorlevel%"
echo.
echo The result files preserve all passes, failures, and cases not run.
pause
exit /b %fidelity_result%
