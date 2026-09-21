[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',

    [string]$VisualStudioVersion = '[17.0,18.0)',

    [string]$VCToolsVersion = '14.44',

    [string]$WindowsSdkVersion = '10.0.26100.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$baselineRoot = Join-Path $repositoryRoot 'build\baseline'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Invoke-Git {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $result = & git -C $repositoryRoot @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed:`n$($result -join [Environment]::NewLine)"
    }
    return @($result)
}

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "Visual Studio Installer discovery tool was not found at $vswhere"
    }

    $installationPath = (& $vswhere -latest -products * -version $VisualStudioVersion `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if (-not $installationPath) {
        throw "No Visual Studio installation matching $VisualStudioVersion with MSVC was found."
    }

    $devCommand = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $devCommand)) {
        throw "VsDevCmd.bat was not found below $installationPath"
    }
    return $devCommand
}

function Get-MakeSources {
    param([Parameter(Mandatory = $true)][string]$Component)

    $makePath = Join-Path $repositoryRoot "src\$Component\subdir.mk"
    $makeText = [IO.File]::ReadAllText($makePath)
    $pattern = 'build/' + [regex]::Escape($Component) + '/([^\s]+)\.o'
    $sources = @(
        [regex]::Matches($makeText, $pattern) |
            ForEach-Object { 'src/' + $_.Groups[1].Value + '.cc' } |
            Select-Object -Unique
    )
    if ($sources.Count -eq 0) {
        throw "No sources were found in $makePath"
    }
    return $sources
}

function Invoke-BaselineTest {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$ResultDirectory,
        [int]$TimeoutMilliseconds = 60000
    )

    $name = [IO.Path]::GetFileNameWithoutExtension($Executable)
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Could not start $Executable"
    }

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutMilliseconds)
    if ($timedOut) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        $process.WaitForExit()
    }

    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    [IO.File]::WriteAllText((Join-Path $ResultDirectory "$name.stdout.txt"), $stdout, $utf8NoBom)
    [IO.File]::WriteAllText((Join-Path $ResultDirectory "$name.stderr.txt"), $stderr, $utf8NoBom)

    return [ordered]@{
        Name = $name
        ExitCode = if ($timedOut) { $null } else { $process.ExitCode }
        TimedOut = $timedOut
        StandardOutput = $stdout.Trim()
        StandardError = $stderr.Trim()
    }
}

$sourceDifference = @(Invoke-Git @('diff', '--name-only', 'milestone-0-start', '--', 'src', 'VERSION.txt'))
if ($sourceDifference.Count -ne 0) {
    throw "Runtime source differs from milestone-0-start:`n$($sourceDifference -join [Environment]::NewLine)"
}

$sourceCommit = ((Invoke-Git @('rev-parse', 'HEAD')) -join '').Trim()
$sourceVersion = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'VERSION.txt')).Trim()
$workingTreeState = @(Invoke-Git @('status', '--porcelain=v1'))
$devCommand = Find-VisualStudio
$configurations = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }

$targets = @(
    [ordered]@{ Name = 'winpty'; Component = 'libwinpty'; Define = 'COMPILING_WINPTY_DLL'; Extension = 'dll' },
    [ordered]@{ Name = 'winpty-agent'; Component = 'agent'; Define = 'WINPTY_AGENT_ASSERT'; Extension = 'exe' },
    [ordered]@{ Name = 'winpty-debugserver'; Component = 'debugserver'; Define = ''; Extension = 'exe' },
    [ordered]@{ Name = 'trivial_test'; Sources = @('src/tests/trivial_test.cc'); Define = ''; Extension = 'exe'; Test = $true },
    [ordered]@{ Name = 'StringBuilderTest'; Sources = @('src/shared/StringBuilderTest.cc'); Define = ''; Extension = 'exe'; Test = $true }
)

foreach ($configurationName in $configurations) {
    $configurationRoot = Join-Path $baselineRoot $configurationName
    $resolvedBaselineRoot = [IO.Path]::GetFullPath($baselineRoot).TrimEnd('\') + '\'
    $resolvedConfigurationRoot = [IO.Path]::GetFullPath($configurationRoot)
    if (-not $resolvedConfigurationRoot.StartsWith($resolvedBaselineRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean baseline output outside $resolvedBaselineRoot"
    }
    if (Test-Path -LiteralPath $resolvedConfigurationRoot) {
        Remove-Item -LiteralPath $resolvedConfigurationRoot -Recurse -Force
    }

    $binDirectory = Join-Path $configurationRoot 'bin'
    $generatedDirectory = Join-Path $configurationRoot 'generated'
    $analysisDirectory = Join-Path $configurationRoot 'analysis'
    $logDirectory = Join-Path $configurationRoot 'logs'
    $objectDirectory = Join-Path $configurationRoot 'obj'

    foreach ($directory in @($binDirectory, $generatedDirectory, $analysisDirectory, $logDirectory, $objectDirectory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    $versionHeader = @(
        "const char GenVersion_Version[] = `"$sourceVersion`";"
        "const char GenVersion_Commit[] = `"$sourceCommit`";"
        ''
    ) -join "`r`n"
    [IO.File]::WriteAllText((Join-Path $generatedDirectory 'GenVersion.h'), $versionHeader, $utf8NoBom)

    $commands = @(
        '@echo off',
        ('call "' + $devCommand + '" -arch=x64 -host_arch=x64 -vcvars_ver=' + $VCToolsVersion + ' -winsdk=' + $WindowsSdkVersion + ' >nul'),
        'if errorlevel 1 exit /b 1',
        ('cd /d "' + $configurationRoot + '"'),
        ('where cl.exe > "' + (Join-Path $analysisDirectory 'tool-paths.txt') + '" 2>&1'),
        ('where link.exe >> "' + (Join-Path $analysisDirectory 'tool-paths.txt') + '" 2>&1'),
        ('where dumpbin.exe >> "' + (Join-Path $analysisDirectory 'tool-paths.txt') + '" 2>&1'),
        ('cl.exe /Bv > "' + (Join-Path $analysisDirectory 'toolchain.txt') + '" 2>&1')
    )

    foreach ($target in $targets) {
        $targetName = $target.Name
        $targetObjectDirectory = Join-Path $objectDirectory $targetName
        New-Item -ItemType Directory -Path $targetObjectDirectory -Force | Out-Null

        $sources = if ($target.Contains('Component')) {
            @(Get-MakeSources $target.Component)
        } else {
            @($target.Sources)
        }

        $isTest = $target.Contains('Test') -and $target.Test
        $compilerArguments = @(
            '/nologo', '/c', '/Brepro', '/EHsc', '/std:c++14', '/W3', '/Z7',
            '/DUNICODE', '/D_UNICODE', '/DNOMINMAX',
            '/D_WIN32_WINNT=0x0501', '/DWINVER=0x0501',
            '/D_CRT_SECURE_NO_WARNINGS',
            ('/I"' + (Join-Path $repositoryRoot 'src\include') + '"'),
            ('/I"' + $generatedDirectory + '"')
        )

        if ($configurationName -eq 'Debug') {
            $compilerArguments += @('/MTd', '/Od', '/RTC1', '/D_DEBUG')
        } else {
            $compilerArguments += @('/MT', '/O2', '/Ob2')
            if (-not $isTest) {
                $compilerArguments += '/DNDEBUG'
            }
        }

        if ($target.Define) {
            $compilerArguments += '/D' + $target.Define
        }

        $compileResponsePath = Join-Path $configurationRoot ($targetName + '-compile.rsp')
        [IO.File]::WriteAllLines($compileResponsePath, $compilerArguments, $utf8NoBom)
        $buildLog = Join-Path $logDirectory ($targetName + '-build.txt')
        $commands += ('type nul > "' + $buildLog + '"')

        $objectPaths = @()
        foreach ($source in $sources) {
            $objectName = ($source -replace '[\\/:. -]', '_') + '.obj'
            $objectPath = Join-Path $targetObjectDirectory $objectName
            $objectPaths += $objectPath
            $commands += @(
                ('cl.exe @"' + $compileResponsePath + '" "' + (Join-Path $repositoryRoot $source) + '" /Fo"' + $objectPath + '" >> "' + $buildLog + '" 2>&1'),
                'if errorlevel 1 exit /b 1'
            )
        }

        $linkArguments = @(
            '/NOLOGO', '/Brepro', '/DEBUG:FULL', '/INCREMENTAL:NO', '/MACHINE:X64',
            ('/OUT:"' + (Join-Path $binDirectory ($targetName + '.' + $target.Extension)) + '"'),
            ('/PDB:"' + (Join-Path $binDirectory ($targetName + '.pdb')) + '"'),
            'advapi32.lib', 'user32.lib', 'shell32.lib',
            @($objectPaths | ForEach-Object { '"' + $_ + '"' })
        )
        if ($configurationName -eq 'Release') {
            $linkArguments += @('/OPT:REF', '/OPT:ICF')
        }
        if ($target.Extension -eq 'exe') {
            $linkArguments += '/SUBSYSTEM:CONSOLE,6.01'
        }
        if ($target.Extension -eq 'dll') {
            $linkArguments += @(
                '/DLL',
                '/SUBSYSTEM:WINDOWS,6.01',
                ('/IMPLIB:"' + (Join-Path $binDirectory 'winpty.lib') + '"')
            )
        }
        if ($targetName -eq 'trivial_test') {
            $linkArguments += '"' + (Join-Path $binDirectory 'winpty.lib') + '"'
        }

        $linkResponsePath = Join-Path $configurationRoot ($targetName + '-link.rsp')
        [IO.File]::WriteAllLines($linkResponsePath, $linkArguments, $utf8NoBom)
        $commands += @(
            ('link.exe @"' + $linkResponsePath + '" >> "' + $buildLog + '" 2>&1'),
            'if errorlevel 1 exit /b 1'
        )
    }

    foreach ($binary in @('winpty.dll', 'winpty-agent.exe', 'winpty-debugserver.exe', 'trivial_test.exe', 'StringBuilderTest.exe')) {
        $binaryPath = Join-Path $binDirectory $binary
        $baseName = [IO.Path]::GetFileNameWithoutExtension($binary)
        $commands += @(
            ('dumpbin.exe /headers /imports "' + $binaryPath + '" > "' + (Join-Path $analysisDirectory ($baseName + '-headers-imports.txt')) + '" 2>&1'),
            'if errorlevel 1 exit /b 1'
        )
    }
    $commands += @(
        ('dumpbin.exe /exports "' + (Join-Path $binDirectory 'winpty.dll') + '" > "' + (Join-Path $analysisDirectory 'winpty-exports.txt') + '" 2>&1'),
        'if errorlevel 1 exit /b 1',
        'exit /b 0'
    )

    $buildCommandPath = Join-Path $configurationRoot 'build.cmd'
    [IO.File]::WriteAllLines($buildCommandPath, $commands, [Text.Encoding]::ASCII)
    & cmd.exe /d /c $buildCommandPath
    if ($LASTEXITCODE -ne 0) {
        throw "$configurationName baseline build failed with exit code $LASTEXITCODE. See $logDirectory"
    }

    $tests = @(
        Invoke-BaselineTest -Executable (Join-Path $binDirectory 'StringBuilderTest.exe') `
            -WorkingDirectory $binDirectory -ResultDirectory $logDirectory
        Invoke-BaselineTest -Executable (Join-Path $binDirectory 'trivial_test.exe') `
            -WorkingDirectory $binDirectory -ResultDirectory $logDirectory
    )

    foreach ($test in $tests) {
        if ($test.TimedOut -or $test.ExitCode -ne 0) {
            throw "$configurationName $($test.Name) failed or timed out."
        }
    }
    $stringBuilderResult = $tests | Where-Object { $_.Name -eq 'StringBuilderTest' }
    if ($stringBuilderResult.StandardError -notmatch 'All tests completed!' -or
            $stringBuilderResult.StandardError -match '(?m)^error:') {
        throw "$configurationName StringBuilderTest did not report a clean completion."
    }

    $artifactRecords = @(
        Get-ChildItem -LiteralPath $binDirectory -File |
            Sort-Object Name |
            ForEach-Object {
                [ordered]@{
                    Name = $_.Name
                    Bytes = $_.Length
                    Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
    )

    $windowsVersion = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    $record = [ordered]@{
        SchemaVersion = 1
        SourceCommit = $sourceCommit
        SourceVersion = $sourceVersion
        RuntimeSourceMatchesFoundationTag = $true
        WorkingTreeWasClean = ($workingTreeState.Count -eq 0)
        WorkingTreeChanges = @($workingTreeState)
        Configuration = $configurationName
        Architecture = 'x64'
        LanguageMode = 'C++14 (inherited baseline)'
        Win32Target = '0x0501 (inherited baseline)'
        SubsystemVersion = '6.01 (approved Windows 7 baseline floor)'
        RuntimeLibrary = if ($configurationName -eq 'Debug') { 'Static debug (/MTd)' } else { 'Static release (/MT)' }
        VCToolsRequest = $VCToolsVersion
        WindowsSdkRequest = $WindowsSdkVersion
        Host = [ordered]@{
            Caption = $windowsVersion.ProductName
            Version = [Environment]::OSVersion.Version.ToString()
            BuildNumber = $windowsVersion.CurrentBuildNumber
            Architecture = $env:PROCESSOR_ARCHITECTURE
        }
        Tests = $tests
        Artifacts = $artifactRecords
    }
    $record | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $configurationRoot 'baseline.json') -Encoding UTF8

    Write-Host "$configurationName baseline passed: $configurationRoot"
}
