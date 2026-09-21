[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',

    [switch]$NoBuild,

    [int]$TestTimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = $PSScriptRoot
$artifactRoot = Join-Path $repositoryRoot 'artifacts'
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$sourceVersion = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'VERSION.txt')).Trim()
$sourceCommit = (& git -C $repositoryRoot rev-parse --verify HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Could not resolve source commit: $sourceCommit"
}
$sourceChanges = @(& git -C $repositoryRoot status --porcelain=v1 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect source state: $($sourceChanges -join [Environment]::NewLine)"
}

function Find-Dumpbin {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath)) {
        throw "Visual Studio Installer discovery tool was not found at $vswherePath"
    }
    $installationPath = (& $vswherePath -latest -products * -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if (-not $installationPath) {
        throw 'Visual Studio 2022 with the MSVC x64 tools was not found.'
    }
    $toolsRoot = Join-Path $installationPath 'VC\Tools\MSVC'
    $dumpbin = Get-ChildItem -LiteralPath $toolsRoot -Filter dumpbin.exe -File -Recurse |
        Where-Object { $_.FullName -match '\\bin\\Hostx64\\x64\\dumpbin\.exe$' } |
        Sort-Object { [version]$_.Directory.Parent.Parent.Parent.Name } -Descending |
        Select-Object -First 1
    if ($null -eq $dumpbin) {
        throw "dumpbin.exe was not found below $toolsRoot"
    }
    return $dumpbin.FullName
}

function Invoke-NativeTest {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [string[]]$Arguments = @()
    )

    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = ($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '

    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Could not start $Executable"
    }

    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TestTimeoutSeconds * 1000)
    if ($timedOut) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        $process.WaitForExit()
    }

    return [ordered]@{
        Name = [IO.Path]::GetFileName($Executable)
        ExitCode = if ($timedOut) { $null } else { $process.ExitCode }
        TimedOut = $timedOut
        StandardOutput = $stdoutTask.Result.Trim()
        StandardError = $stderrTask.Result.Trim()
    }
}

function Assert-NativeTestPassed {
    param([Parameter(Mandatory = $true)]$Result)
    if ($Result.TimedOut) {
        throw "$($Result.Name) exceeded the $TestTimeoutSeconds-second timeout."
    }
    if ($Result.ExitCode -ne 0) {
        throw "$($Result.Name) exited with $($Result.ExitCode).`n$($Result.StandardError)"
    }
}

if (-not $NoBuild) {
    & (Join-Path $repositoryRoot 'Build-VT7Pty.ps1') -Configuration $Configuration
}

$dumpbinPath = Find-Dumpbin
$dumpbinVersion = (Get-Item -LiteralPath $dumpbinPath).VersionInfo.FileVersion
$configurations = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
$expectedExports = @(
    'winpty_agent_process', 'winpty_conerr_name', 'winpty_config_free',
    'winpty_config_new', 'winpty_config_set_agent_timeout',
    'winpty_config_set_initial_size', 'winpty_config_set_mouse_mode',
    'winpty_conin_name', 'winpty_conout_name', 'winpty_error_code',
    'winpty_error_free', 'winpty_error_msg', 'winpty_free',
    'winpty_get_console_process_list', 'winpty_open', 'winpty_set_size',
    'winpty_spawn', 'winpty_spawn_config_free', 'winpty_spawn_config_new'
)
$expectedImports = [ordered]@{
    'winpty.dll' = @('ADVAPI32.dll', 'KERNEL32.dll', 'USER32.dll')
    'winpty-agent.exe' = @('ADVAPI32.dll', 'KERNEL32.dll', 'SHELL32.dll', 'USER32.dll')
    'winpty-debugserver.exe' = @('ADVAPI32.dll', 'KERNEL32.dll')
    'trivial_test.exe' = @('KERNEL32.dll', 'winpty.dll')
    'StringBuilderTest.exe' = @('KERNEL32.dll')
    'fixture-console-color-grid.exe' = @('KERNEL32.dll')
    'fixture-output-lines.exe' = @('KERNEL32.dll')
    'fixture-show-argv.exe' = @('KERNEL32.dll')
    'fixture-show-console-input.exe' = @('KERNEL32.dll')
    'fixture-utf16-echo.exe' = @('KERNEL32.dll')
    'fixture-win32-echo1.exe' = @('KERNEL32.dll')
    'fixture-win32-echo2.exe' = @('KERNEL32.dll')
    'fixture-win32-write1.exe' = @('KERNEL32.dll')
    'fixture-write-console.exe' = @('KERNEL32.dll')
    'tool-conin-mode.exe' = @('KERNEL32.dll')
    'tool-conout-mode.exe' = @('KERNEL32.dll')
}

foreach ($configurationName in $configurations) {
    $binaryDirectory = Join-Path $artifactRoot "bin\x64\$configurationName"
    $resultDirectory = Join-Path $artifactRoot "verification\x64\$configurationName"
    [IO.Directory]::CreateDirectory($resultDirectory) | Out-Null

    $requiredArtifacts = @(
        'winpty.dll', 'winpty.lib', 'winpty.pdb',
        'winpty-agent.exe', 'winpty-agent.pdb',
        'winpty-debugserver.exe', 'winpty-debugserver.pdb',
        'StringBuilderTest.exe', 'StringBuilderTest.pdb',
        'trivial_test.exe', 'trivial_test.pdb',
        'fixture-console-color-grid.exe', 'fixture-console-color-grid.pdb',
        'fixture-output-lines.exe', 'fixture-output-lines.pdb',
        'fixture-show-argv.exe', 'fixture-show-argv.pdb',
        'fixture-show-console-input.exe', 'fixture-show-console-input.pdb',
        'fixture-utf16-echo.exe', 'fixture-utf16-echo.pdb',
        'fixture-win32-echo1.exe', 'fixture-win32-echo1.pdb',
        'fixture-win32-echo2.exe', 'fixture-win32-echo2.pdb',
        'fixture-win32-write1.exe', 'fixture-win32-write1.pdb',
        'fixture-write-console.exe', 'fixture-write-console.pdb',
        'tool-conin-mode.exe', 'tool-conin-mode.pdb',
        'tool-conout-mode.exe', 'tool-conout-mode.pdb'
    )
    foreach ($artifact in $requiredArtifacts) {
        $artifactPath = Join-Path $binaryDirectory $artifact
        if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
            throw "Missing $configurationName artifact: $artifactPath"
        }
    }

    $tests = @(
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'StringBuilderTest.exe') -WorkingDirectory $binaryDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'trivial_test.exe') -WorkingDirectory $binaryDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'winpty-agent.exe') -WorkingDirectory $binaryDirectory -Arguments @('--version')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'fixture-show-argv.exe') -WorkingDirectory $binaryDirectory -Arguments @('alpha', 'two words')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'fixture-output-lines.exe') -WorkingDirectory $binaryDirectory -Arguments @('3', '5')
    )
    foreach ($test in $tests) {
        Assert-NativeTestPassed -Result $test
    }
    $stringBuilderTest = $tests | Where-Object { $_.Name -eq 'StringBuilderTest.exe' }
    if ($stringBuilderTest.StandardError -notmatch 'All tests completed!' -or
            $stringBuilderTest.StandardError -match '(?m)^error:') {
        throw "$configurationName StringBuilderTest did not report a clean completion."
    }
    $agentVersion = $tests | Where-Object { $_.Name -eq 'winpty-agent.exe' }
    if ($agentVersion.StandardOutput -notmatch [regex]::Escape("winpty version $sourceVersion")) {
        throw "$configurationName agent did not report version $sourceVersion."
    }
    if ($agentVersion.StandardOutput -notmatch [regex]::Escape("commit $sourceCommit")) {
        throw "$configurationName agent did not report source commit $sourceCommit."
    }
    $argumentFixture = $tests | Where-Object { $_.Name -eq 'fixture-show-argv.exe' }
    if ($argumentFixture.StandardOutput -notmatch '(?m)^\[alpha\]\r?$' -or
            $argumentFixture.StandardOutput -notmatch '(?m)^\[two words\]\r?$') {
        throw "$configurationName argument fixture did not preserve its test arguments."
    }
    $outputFixture = $tests | Where-Object { $_.Name -eq 'fixture-output-lines.exe' }
    if ($outputFixture.StandardOutput -ne "1 XXXXX`r`n2 XXXXX`r`n3 XXXXX") {
        throw "$configurationName output-lines fixture did not produce its exact bounded output."
    }

    $binaryRecords = @()
    foreach ($binaryName in $expectedImports.Keys) {
        $binaryPath = Join-Path $binaryDirectory $binaryName
        $inspection = (& $dumpbinPath /headers /imports $binaryPath 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) {
            throw "dumpbin failed for $binaryPath"
        }
        [IO.File]::WriteAllText(
            (Join-Path $resultDirectory ($binaryName + '.headers-imports.txt')),
            $inspection,
            $utf8NoBom)
        if ($inspection -notmatch '(?im)^\s*8664 machine \(x64\)') {
            throw "$binaryName is not an x64 image."
        }
        if ($inspection -notmatch '(?im)^\s*6\.01 subsystem version') {
            throw "$binaryName does not declare Windows 7 subsystem version 6.01."
        }
        $imports = @(
            [regex]::Matches($inspection, '(?im)^\s+([A-Za-z0-9_.-]+\.dll)\s*$') |
                ForEach-Object { $_.Groups[1].Value } |
                Sort-Object -Unique
        )
        $expected = @($expectedImports[$binaryName] | Sort-Object)
        if (($imports -join '|').ToUpperInvariant() -ne ($expected -join '|').ToUpperInvariant()) {
            throw "$binaryName imports [$($imports -join ', ')], expected [$($expected -join ', ')]."
        }
        $file = Get-Item -LiteralPath $binaryPath
        $versionInfo = $file.VersionInfo
        if ($versionInfo.FileVersion -ne $sourceVersion -or
                $versionInfo.ProductVersion -ne $sourceVersion -or
                $versionInfo.ProductName -ne 'VT7Pty' -or
                $versionInfo.OriginalFilename -ne $binaryName) {
            throw "$binaryName has incorrect generated version-resource identity."
        }
        $binaryRecords += [ordered]@{
            Name = $binaryName
            Bytes = $file.Length
            Sha256 = (Get-FileHash -LiteralPath $binaryPath -Algorithm SHA256).Hash.ToLowerInvariant()
            Imports = $imports
            FileDescription = $versionInfo.FileDescription
            FileVersion = $versionInfo.FileVersion
            ProductName = $versionInfo.ProductName
            ProductVersion = $versionInfo.ProductVersion
            OriginalFilename = $versionInfo.OriginalFilename
        }
    }

    $exportInspection = (& $dumpbinPath /exports (Join-Path $binaryDirectory 'winpty.dll') 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "$configurationName winpty.dll export inspection failed."
    }
    [IO.File]::WriteAllText((Join-Path $resultDirectory 'winpty.dll.exports.txt'), $exportInspection, $utf8NoBom)
    $actualExports = @(
        [regex]::Matches(
            $exportInspection,
            '(?im)^\s+\d+\s+[0-9a-f]+\s+[0-9a-f]+\s+([a-z0-9_]+)(?:\s|$)') |
            ForEach-Object { $_.Groups[1].Value }
    )
    $actualExportKey = (@($actualExports | Sort-Object) -join '|')
    $expectedExportKey = (@($expectedExports | Sort-Object) -join '|')
    if ($actualExportKey -ne $expectedExportKey) {
        throw "$configurationName winpty.dll does not expose the complete inherited export surface."
    }

    $artifactRecords = @(
        foreach ($artifact in $requiredArtifacts) {
            $artifactPath = Join-Path $binaryDirectory $artifact
            $file = Get-Item -LiteralPath $artifactPath
            [ordered]@{
                Name = $artifact
                Bytes = $file.Length
                Sha256 = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
    )
    $windowsVersion = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    $record = [ordered]@{
        SchemaVersion = 1
        SourceCommit = $sourceCommit
        SourceVersion = $sourceVersion
        SourceTreeClean = ($sourceChanges.Count -eq 0)
        SourceTreeChanges = $sourceChanges
        Configuration = $configurationName
        Architecture = 'x64'
        WindowsApiFloor = 'Windows 7 SP1 (0x0601)'
        SubsystemVersion = '6.01'
        Host = [ordered]@{
            ProductName = $windowsVersion.ProductName
            Version = [Environment]::OSVersion.Version.ToString()
            BuildNumber = $windowsVersion.CurrentBuildNumber
            Architecture = $env:PROCESSOR_ARCHITECTURE
        }
        InspectionTool = [ordered]@{
            Path = $dumpbinPath
            FileVersion = $dumpbinVersion
        }
        Tests = $tests
        Artifacts = $artifactRecords
        Binaries = $binaryRecords
        Exports = $actualExports
    }
    $record | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $resultDirectory 'verification.json') -Encoding UTF8
    Write-Host "$configurationName verification passed: $resultDirectory"
}
