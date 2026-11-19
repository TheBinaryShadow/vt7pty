@echo off
setlocal
set "VT7PTY_PROFILE_ORIGINAL_POLICY_PRESENT=0"
set "VT7PTY_PROFILE_ORIGINAL_POLICY_VALUE="
if defined PSExecutionPolicyPreference (
    set "VT7PTY_PROFILE_ORIGINAL_POLICY_PRESENT=1"
    set "VT7PTY_PROFILE_ORIGINAL_POLICY_VALUE=%PSExecutionPolicyPreference%"
)
set "profile_powershell=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "profile_powershell=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%profile_powershell%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\profiles\Invoke-ProfileTests.ps1" -Tier Legacy %*
set "profile_result=%errorlevel%"
echo.
echo Keep every file in the complete profile result directories.
pause
exit /b %profile_result%
