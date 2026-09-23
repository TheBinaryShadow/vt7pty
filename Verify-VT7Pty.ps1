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
$apiVersionHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\include\vt7pty_version.h'))
$protocolHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\shared\Protocol.h'))
$apiMajorMatch = [regex]::Match($apiVersionHeader, '(?m)^#define VT7PTY_API_VERSION_MAJOR (?<value>\d+)\r?$')
$apiMinorMatch = [regex]::Match($apiVersionHeader, '(?m)^#define VT7PTY_API_VERSION_MINOR (?<value>\d+)\r?$')
if (-not $apiMajorMatch.Success -or -not $apiMinorMatch.Success) {
    throw 'Could not read the public API version from vt7pty_version.h.'
}
$apiVersion = "$($apiMajorMatch.Groups['value'].Value).$($apiMinorMatch.Groups['value'].Value)"
$protocolMatch = [regex]::Match(
    $protocolHeader,
    '(?m)^constexpr int32_t VT7PTY_PROTOCOL_VERSION = (?<value>\d+);\r?$')
if (-not $protocolMatch.Success) {
    throw 'Could not read the client-agent protocol version from Protocol.h.'
}
$protocolVersion = [int]$protocolMatch.Groups['value'].Value
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

function Find-ManifestTool {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $manifestTool = Get-ChildItem -LiteralPath $kitsRoot -Filter mt.exe -File -Recurse |
        Where-Object { $_.FullName -match '\\x64\\mt\.exe$' } |
        Sort-Object {
            $versionText = $_.Directory.Parent.Name
            $parsedVersion = New-Object Version
            if ([Version]::TryParse($versionText, [ref]$parsedVersion)) {
                $parsedVersion
            } else {
                [Version]'0.0'
            }
        } -Descending |
        Select-Object -First 1
    if ($null -eq $manifestTool) {
        throw "mt.exe was not found below $kitsRoot"
    }
    return $manifestTool.FullName
}

function Invoke-NativeTest {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [string[]]$Arguments = @(),
        [hashtable]$Environment = @{}
    )

    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = ($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '
    foreach ($name in $Environment.Keys) {
        $startInfo.EnvironmentVariables[$name] = [string]$Environment[$name]
    }

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

function Invoke-IsolatedAgentFailureTest {
    param(
        [Parameter(Mandatory = $true)][string]$BinaryDirectory,
        [Parameter(Mandatory = $true)][string]$ResultDirectory,
        [Parameter(Mandatory = $true)][string]$Case,
        [string]$ProtocolMode
    )

    $caseDirectory = Join-Path $ResultDirectory ("agent-failure-" + $Case)
    if (Test-Path -LiteralPath $caseDirectory) {
        Remove-Item -LiteralPath $caseDirectory -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($caseDirectory) | Out-Null
    Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'BackendSmokeTest.exe') -Destination $caseDirectory
    Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'VT7Pty.dll') -Destination $caseDirectory

    if ($ProtocolMode) {
        Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'ProtocolTestAgent.exe') `
            -Destination (Join-Path $caseDirectory 'VT7Pty-Agent.exe')
        $result = Invoke-NativeTest `
            -Executable (Join-Path $caseDirectory 'BackendSmokeTest.exe') `
            -WorkingDirectory $caseDirectory `
            -Arguments @('EXPECT_INCOMPATIBLE_AGENT') `
            -Environment @{ VT7PTY_PROTOCOL_TEST_MODE = $ProtocolMode }
    } else {
        $result = Invoke-NativeTest `
            -Executable (Join-Path $caseDirectory 'BackendSmokeTest.exe') `
            -WorkingDirectory $caseDirectory `
            -Arguments @('EXPECT_MISSING_AGENT')
    }
    $result.Name = "Agent rejection: $Case"
    return $result
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
    if ($Configuration -in @('Release', 'All')) {
        & (Join-Path $repositoryRoot 'Analyze-VT7Pty.ps1')
    }
}

function Invoke-DiagnosticTransportTest {
    param(
        [Parameter(Mandatory = $true)][string]$BinaryDirectory,
        [Parameter(Mandatory = $true)][string]$ResultDirectory
    )

    $logPath = Join-Path $ResultDirectory 'diagnostic-transport.jsonl'
    if (Test-Path -LiteralPath $logPath) {
        Remove-Item -LiteralPath $logPath -Force
    }
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = Join-Path $BinaryDirectory 'VT7Pty-DebugServer.exe'
    $startInfo.WorkingDirectory = $BinaryDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = '--output "' + $logPath + '" --max-bytes 4096 --max-messages 3'
    $server = New-Object Diagnostics.Process
    $server.StartInfo = $startInfo
    if (-not $server.Start()) {
        throw 'Could not start diagnostic server.'
    }
    $serverOutput = $server.StandardOutput.ReadToEndAsync()
    $serverError = $server.StandardError.ReadToEndAsync()
    Start-Sleep -Milliseconds 250
    $emitter = Invoke-NativeTest `
        -Executable (Join-Path $BinaryDirectory 'ProtocolTest.exe') `
        -WorkingDirectory $BinaryDirectory `
        -Arguments @('EMIT_DIAGNOSTICS') `
        -Environment @{ VT7PTY_DEBUG = 'trace' }
    $timedOut = -not $server.WaitForExit($TestTimeoutSeconds * 1000)
    if ($timedOut) {
        & taskkill.exe /PID $server.Id /T /F | Out-Null
        $server.WaitForExit()
    }
    $failure = $null
    if ($emitter.TimedOut -or $emitter.ExitCode -ne 0) {
        $failure = 'diagnostic emitter failed'
    } elseif ($timedOut -or $server.ExitCode -ne 0) {
        $failure = 'diagnostic server failed or timed out'
    } elseif (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        $failure = 'diagnostic server did not create its output file'
    } elseif ((Get-Item -LiteralPath $logPath).Length -gt 4096) {
        $failure = 'diagnostic server exceeded its configured file bound'
    } else {
        try {
            $records = @(Get-Content -LiteralPath $logPath | ForEach-Object {
                $_ | ConvertFrom-Json
            })
            if ($records.Count -ne 3 -or
                    @($records | Where-Object {
                        $_.product -ne 'VT7Pty' -or
                        $_.version -ne $sourceVersion -or
                        $_.commit -ne $sourceCommit -or
                        $null -eq $_.timestamp -or
                        $null -eq $_.severity -or
                        $null -eq $_.subsystem -or
                        $null -eq $_.pid -or
                        $null -eq $_.tid
                    }).Count -ne 0) {
                $failure = 'diagnostic records lack required structured identity'
            }
        } catch {
            $failure = 'diagnostic output is not valid JSON lines'
        }
    }
    return [ordered]@{
        Name = 'Structured diagnostic transport'
        ExitCode = if ($null -eq $failure) { 0 } else { 1 }
        TimedOut = $timedOut
        StandardOutput = $serverOutput.Result.Trim()
        StandardError = (@($serverError.Result.Trim(), $failure) |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [Environment]::NewLine
    }
}

$dumpbinPath = Find-Dumpbin
$dumpbinVersion = (Get-Item -LiteralPath $dumpbinPath).VersionInfo.FileVersion
$manifestToolPath = Find-ManifestTool
$manifestToolVersion = (Get-Item -LiteralPath $manifestToolPath).VersionInfo.FileVersion
$configurations = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
$expectedExports = @(
    'vt7pty_agent_process', 'vt7pty_conerr_name', 'vt7pty_config_free',
    'vt7pty_config_new', 'vt7pty_config_set_agent_timeout',
    'vt7pty_config_set_initial_size', 'vt7pty_config_set_mouse_mode',
    'vt7pty_conin_name', 'vt7pty_conout_name', 'vt7pty_error_code',
    'vt7pty_error_free', 'vt7pty_error_msg', 'vt7pty_free',
    'vt7pty_get_console_process_list', 'vt7pty_open', 'vt7pty_set_size',
    'vt7pty_spawn', 'vt7pty_spawn_config_free', 'vt7pty_spawn_config_new'
)
$expectedImports = [ordered]@{
    'VT7Pty.dll' = @('ADVAPI32.dll', 'KERNEL32.dll')
    'VT7Pty-Agent.exe' = @('ADVAPI32.dll', 'KERNEL32.dll', 'SHELL32.dll', 'USER32.dll')
    'VT7Pty-DebugServer.exe' = @('ADVAPI32.dll', 'KERNEL32.dll')
    'BackendSmokeTest.exe' = @('KERNEL32.dll', 'VT7Pty.dll')
    'ProtocolTest.exe' = @('KERNEL32.dll')
    'ProtocolTestAgent.exe' = @('KERNEL32.dll')
    'ModernCppTest.exe' = @('KERNEL32.dll')
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
$supportedOsIds = @(
    '{35138b9a-5d96-4fbd-8e2d-a2440225f93a}', # Windows 7
    '{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}', # Windows 8
    '{1f676c76-80e1-4239-95bb-83d0f6d0da78}', # Windows 8.1
    '{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}'  # Windows 10 and 11
)
$postWindows7Imports = @(
    'ClosePseudoConsole', 'CreatePseudoConsole', 'GetDpiForSystem',
    'GetDpiForWindow', 'GetProcessInformation', 'GetSystemCpuSetInformation',
    'GetSystemTimeAdjustmentPrecise', 'GetSystemTimePreciseAsFileTime',
    'GetTempPath2W', 'GetThreadDescription', 'GetThreadInformation',
    'IsWow64Process2', 'ResizePseudoConsole', 'SetProcessInformation',
    'SetThreadDescription', 'SetThreadInformation', 'WaitOnAddress',
    'WakeByAddressAll', 'WakeByAddressSingle'
)

foreach ($configurationName in $configurations) {
    $binaryDirectory = Join-Path $artifactRoot "bin\x64\$configurationName"
    $resultDirectory = Join-Path $artifactRoot "verification\x64\$configurationName"
    if (Test-Path -LiteralPath $resultDirectory) {
        Remove-Item -LiteralPath $resultDirectory -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($resultDirectory) | Out-Null

    $legacyArtifacts = @(
        'winpty.dll', 'winpty.lib', 'winpty.pdb',
        'winpty-agent.exe', 'winpty-agent.pdb',
        'winpty-debugserver.exe', 'winpty-debugserver.pdb',
        'trivial_test.exe', 'trivial_test.pdb',
        'StringBuilderTest.exe', 'StringBuilderTest.pdb'
    )
    foreach ($legacyArtifact in $legacyArtifacts) {
        $legacyArtifactPath = Join-Path $binaryDirectory $legacyArtifact
        if (Test-Path -LiteralPath $legacyArtifactPath -PathType Leaf) {
            throw "Legacy artifact remains in the $configurationName output: $legacyArtifactPath"
        }
    }

    $requiredArtifacts = @(
        'VT7Pty.dll', 'VT7Pty.lib', 'VT7Pty.pdb',
        'VT7Pty-Agent.exe', 'VT7Pty-Agent.pdb',
        'VT7Pty-DebugServer.exe', 'VT7Pty-DebugServer.pdb',
        'ModernCppTest.exe', 'ModernCppTest.pdb',
        'BackendSmokeTest.exe', 'BackendSmokeTest.pdb',
        'ProtocolTest.exe', 'ProtocolTest.pdb',
        'ProtocolTestAgent.exe', 'ProtocolTestAgent.pdb',
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
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'ModernCppTest.exe') -WorkingDirectory $binaryDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'ProtocolTest.exe') -WorkingDirectory $binaryDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'VT7Pty-DebugServer.exe') -WorkingDirectory $binaryDirectory -Arguments @('--self-test')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'VT7Pty-DebugServer.exe') -WorkingDirectory $binaryDirectory -Arguments @('--version')
        Invoke-DiagnosticTransportTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'BackendSmokeTest.exe') -WorkingDirectory $binaryDirectory
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'BackendSmokeTest.exe') -WorkingDirectory $binaryDirectory -Arguments @('APPLICATIONS')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'VT7Pty-Agent.exe') -WorkingDirectory $binaryDirectory -Arguments @('--version')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'fixture-show-argv.exe') -WorkingDirectory $binaryDirectory -Arguments @('alpha', 'two words')
        Invoke-NativeTest -Executable (Join-Path $binaryDirectory 'fixture-output-lines.exe') -WorkingDirectory $binaryDirectory -Arguments @('3', '5')
        Invoke-IsolatedAgentFailureTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory -Case 'missing'
        Invoke-IsolatedAgentFailureTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory -Case 'malformed' -ProtocolMode 'malformed'
        Invoke-IsolatedAgentFailureTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory -Case 'wrong-identity' -ProtocolMode 'wrong-identity'
        Invoke-IsolatedAgentFailureTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory -Case 'older-protocol' -ProtocolMode 'older'
        Invoke-IsolatedAgentFailureTest -BinaryDirectory $binaryDirectory -ResultDirectory $resultDirectory -Case 'newer-protocol' -ProtocolMode 'newer'
    )
    foreach ($test in $tests) {
        Assert-NativeTestPassed -Result $test
    }
    $modernCppTest = $tests | Where-Object { $_.Name -eq 'ModernCppTest.exe' }
    if ($modernCppTest.StandardOutput -notmatch
            'VT7Pty modern C\+\+ utility tests passed\.') {
        throw "$configurationName ModernCppTest did not report a clean completion."
    }
    $protocolTest = $tests | Where-Object { $_.Name -eq 'ProtocolTest.exe' }
    if ($protocolTest.StandardOutput -notmatch 'VT7Pty protocol tests passed') {
        throw "$configurationName protocol test did not report a clean completion."
    }
    $diagnosticSelfTest = @($tests | Where-Object {
        $_.Name -eq 'VT7Pty-DebugServer.exe' -and
        $_.StandardOutput -match 'diagnostic security self-test passed'
    })
    if ($diagnosticSelfTest.Count -ne 1) {
        throw "$configurationName diagnostic pipe security self-test did not pass."
    }
    $diagnosticVersion = @($tests | Where-Object {
        $_.Name -eq 'VT7Pty-DebugServer.exe' -and
        $_.StandardOutput -match [regex]::Escape("commit $sourceCommit")
    })
    if ($diagnosticVersion.Count -ne 1) {
        throw "$configurationName diagnostic server did not report source commit $sourceCommit."
    }
    $applicationTest = @($tests | Where-Object {
        $_.Name -eq 'BackendSmokeTest.exe' -and
        $_.StandardOutput -match 'Command Prompt and Windows PowerShell sessions passed\.'
    })
    if ($applicationTest.Count -ne 1) {
        throw "$configurationName application smoke test did not report a clean completion."
    }
    $agentVersion = $tests | Where-Object { $_.Name -eq 'VT7Pty-Agent.exe' }
    if ($agentVersion.StandardOutput -notmatch [regex]::Escape("VT7Pty version $sourceVersion")) {
        throw "$configurationName agent did not report version $sourceVersion."
    }
    if ($agentVersion.StandardOutput -notmatch [regex]::Escape("commit $sourceCommit")) {
        throw "$configurationName agent did not report source commit $sourceCommit."
    }
    if ($agentVersion.StandardOutput -notmatch [regex]::Escape("API version $apiVersion") -or
            $agentVersion.StandardOutput -notmatch [regex]::Escape("protocol version $protocolVersion")) {
        throw "$configurationName agent did not report API $apiVersion and protocol $protocolVersion."
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
        if ($inspection -notmatch '(?im)^\s*0 \[\s*0\] RVA \[size\] of Delay Import Directory') {
            throw "$binaryName has a non-empty delay-import directory."
        }
        $prohibitedImports = @(
            foreach ($apiName in $postWindows7Imports) {
                if ($inspection -match "(?im)^\s+[0-9a-f]+\s+$([regex]::Escape($apiName))\s*$") {
                    $apiName
                }
            }
        )
        if ($prohibitedImports.Count -ne 0) {
            throw "$binaryName directly imports post-Windows 7 APIs: $($prohibitedImports -join ', ')."
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
        $manifestPath = Join-Path $resultDirectory ($binaryName + '.manifest.xml')
        $manifestResource = if ($binaryName.EndsWith('.dll', [StringComparison]::OrdinalIgnoreCase)) { 2 } else { 1 }
        & $manifestToolPath -nologo "-inputresource:$binaryPath;#$manifestResource" "-out:$manifestPath"
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
            throw "Could not extract the embedded manifest from $binaryName."
        }
        $manifestText = [IO.File]::ReadAllText($manifestPath)
        foreach ($supportedOsId in $supportedOsIds) {
            if ($manifestText -notmatch [regex]::Escape($supportedOsId)) {
                throw "$binaryName does not declare supported OS ID $supportedOsId."
            }
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
            DelayImports = @()
            ProhibitedPostWindows7Imports = $prohibitedImports
            SupportedOsIds = $supportedOsIds
            FileDescription = $versionInfo.FileDescription
            FileVersion = $versionInfo.FileVersion
            ProductName = $versionInfo.ProductName
            ProductVersion = $versionInfo.ProductVersion
            OriginalFilename = $versionInfo.OriginalFilename
        }
    }

    $exportInspection = (& $dumpbinPath /exports (Join-Path $binaryDirectory 'VT7Pty.dll') 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "$configurationName VT7Pty.dll export inspection failed."
    }
    [IO.File]::WriteAllText((Join-Path $resultDirectory 'VT7Pty.dll.exports.txt'), $exportInspection, $utf8NoBom)
    $actualExports = @(
        [regex]::Matches(
            $exportInspection,
            '(?im)^\s+\d+\s+[0-9a-f]+\s+[0-9a-f]+\s+([a-z0-9_]+)(?:\s|$)') |
            ForEach-Object { $_.Groups[1].Value }
    )
    $actualExportKey = (@($actualExports | Sort-Object) -join '|')
    $expectedExportKey = (@($expectedExports | Sort-Object) -join '|')
    if ($actualExportKey -ne $expectedExportKey) {
        throw "$configurationName VT7Pty.dll does not expose the complete inherited export surface."
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
        ApiVersion = $apiVersion
        ProtocolVersion = $protocolVersion
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
            DumpbinPath = $dumpbinPath
            DumpbinFileVersion = $dumpbinVersion
            ManifestToolPath = $manifestToolPath
            ManifestToolFileVersion = $manifestToolVersion
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
