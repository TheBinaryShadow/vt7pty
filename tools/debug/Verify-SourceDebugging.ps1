[CmdletBinding()]
param(
    [switch]$NoBuild,

    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$binaryDirectory = Join-Path $repositoryRoot 'artifacts\bin\x64\Debug'
$debuggee = Join-Path $binaryDirectory 'trivial_test.exe'
$resultDirectory = Join-Path $repositoryRoot 'artifacts\debugging'
$resultPath = Join-Path $resultDirectory 'source-debugging.txt'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Find-Cdb {
    $roots = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\cdb.exe'),
        (Join-Path $env:ProgramFiles 'Windows Kits\10\Debuggers\x64\cdb.exe')
    )
    foreach ($candidate in $roots) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw 'cdb.exe was not found. Install Debugging Tools for Windows from the Windows SDK installer.'
}

if (-not $NoBuild) {
    & (Join-Path $repositoryRoot 'Build-VT7Pty.ps1') -Configuration Debug
}
if (-not (Test-Path -LiteralPath $debuggee -PathType Leaf)) {
    throw "Missing Debug test program: $debuggee"
}

$cdbPath = Find-Cdb
$commands = 'bu winpty!winpty_config_new; g; .lines -e; ln @rip; k; q'
$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = $cdbPath
$startInfo.WorkingDirectory = $binaryDirectory
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.Arguments = '-lines -sins -y "{0}" -srcpath "{1}" -o -G -c "{2}" "{3}"' -f `
    $binaryDirectory, $repositoryRoot, $commands, $debuggee

$process = New-Object Diagnostics.Process
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw "Could not start $cdbPath"
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
$timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    & taskkill.exe /PID $process.Id /T /F | Out-Null
    $process.WaitForExit()
}

$output = $stdoutTask.Result + $stderrTask.Result
[IO.Directory]::CreateDirectory($resultDirectory) | Out-Null
[IO.File]::WriteAllText($resultPath, $output, $utf8NoBom)

if ($timedOut) {
    throw "Source-debugging check exceeded the $TimeoutSeconds-second timeout. See $resultPath"
}
if ($process.ExitCode -ne 0) {
    throw "cdb.exe exited with $($process.ExitCode). See $resultPath"
}
if ($output -notmatch '(?im)^Breakpoint \d+ hit$' -or
        $output -notmatch '(?i)winpty!winpty_config_new') {
    throw "The debugger did not hit winpty_config_new. See $resultPath"
}
if ($output -notmatch '(?i)src\\libwinpty\\winpty\.cc @ \d+') {
    throw "The debugger did not resolve winpty.cc source lines. See $resultPath"
}
if ($output -notmatch '(?i)src\\tests\\trivial_test\.cc @ \d+') {
    throw "The debugger did not resolve the test caller's source lines. See $resultPath"
}

Write-Host "Source-level debugging verified with $cdbPath"
Write-Host "Debugger transcript: $resultPath"
