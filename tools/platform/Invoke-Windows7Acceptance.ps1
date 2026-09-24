param(
    [ValidateSet('NonESU', 'ESU', 'Legacy')]
    [string]$Tier,

    [string]$OutputDirectory,

    [int]$TestTimeoutSeconds = 90,

    [ValidateRange(0, 120)]
    [int]$SoakMinutes = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
trap { Write-Host "Acceptance runner failed: $($_.Exception.Message)"; exit 1 }
if (-not $Tier) { Write-Host 'A Windows 7 acceptance tier is required.'; exit 2 }

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDirectory 'Windows7Compat.ps1')
$packageRoot = Split-Path -Parent (Split-Path -Parent $scriptDirectory)
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $packageRoot 'results'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$binDirectory = Join-Path $packageRoot 'bin'
$testDirectory = Join-Path $packageRoot 'tests'
$manifestPath = Join-Path $packageRoot 'manifest.json'
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$runStartedAt = (Get-Date).ToUniversalTime().ToString('o')

function Stop-VT7ProcessTree {
    param([Diagnostics.Process]$Process)
    & taskkill.exe /PID $Process.Id /T /F | Out-Null
    $terminationCode = $LASTEXITCODE
    if ($terminationCode -ne 0) {
        if (-not $Process.HasExited) {
            try { $Process.Kill() } catch { }
        }
        [void]$Process.WaitForExit(5000)
        throw "Process-tree cleanup failed: taskkill exit code $terminationCode."
    }
    if (-not $Process.WaitForExit(5000)) {
        throw 'Process-tree cleanup failed: the process did not exit.'
    }
}

function Invoke-AcceptanceTest {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Executable,
        [string[]]$Arguments = @(),
        [string[]]$ExpectedOutput,
        [hashtable]$Environment = @{},
        [int]$TimeoutSeconds = $TestTimeoutSeconds,
        [int]$ExpectedExitCode = 0,
        [switch]$ExpectTimeout,
        [ValidateSet('StandardOutput', 'StandardError')]
        [string]$ExpectedStream = 'StandardOutput'
    )

    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $displayExecutable = if ($Executable.StartsWith(
            $packageRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        $Executable.Substring($packageRoot.Length + 1).Replace('\', '/')
    } else {
        $Executable
    }
    $record = @{
        Name = $Name
        Executable = $displayExecutable
        Arguments = $Arguments
        Status = 'Fail'
        DurationMilliseconds = 0
        ExitCode = $null
        TimedOut = $false
        StandardOutput = ''
        StandardError = ''
        Failure = $null
    }

    $process = $null
    $stdoutSubscription = $null
    $stderrSubscription = $null
    $stdoutSource = 'vt7pty.stdout.' + [guid]::NewGuid().ToString('N')
    $stderrSource = 'vt7pty.stderr.' + [guid]::NewGuid().ToString('N')
    try {
        if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
            throw "Executable is missing: $Executable"
        }
        $startInfo = New-Object Diagnostics.ProcessStartInfo
        $startInfo.FileName = $Executable
        $startInfo.WorkingDirectory = $testDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.Arguments = ($Arguments | ForEach-Object {
            '"' + $_.Replace('"', '\"') + '"'
        }) -join ' '
        foreach ($variableName in $Environment.Keys) {
            $startInfo.EnvironmentVariables[$variableName] =
                [string]$Environment[$variableName]
        }

        $process = New-Object Diagnostics.Process
        $process.StartInfo = $startInfo
        if (-not $process.Start()) {
            throw "Could not start $Executable"
        }
        $stdout = @{ Text = (New-Object Text.StringBuilder); Done = $false }
        $stderr = @{ Text = (New-Object Text.StringBuilder); Done = $false }
        $stdoutSubscription = Register-ObjectEvent -InputObject $process `
            -EventName OutputDataReceived -SourceIdentifier $stdoutSource `
            -MessageData $stdout -Action {
                if ($null -eq $EventArgs.Data) {
                    $Event.MessageData.Done = $true
                } else {
                    [void]$Event.MessageData.Text.AppendLine($EventArgs.Data)
                }
            }
        $stderrSubscription = Register-ObjectEvent -InputObject $process `
            -EventName ErrorDataReceived -SourceIdentifier $stderrSource `
            -MessageData $stderr -Action {
                if ($null -eq $EventArgs.Data) {
                    $Event.MessageData.Done = $true
                } else {
                    [void]$Event.MessageData.Text.AppendLine($EventArgs.Data)
                }
            }
        $process.BeginOutputReadLine()
        $process.BeginErrorReadLine()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $record.TimedOut = $true
            Stop-VT7ProcessTree $process
        } else {
            $record.ExitCode = $process.ExitCode
        }
        $process.WaitForExit()
        $drain = [Diagnostics.Stopwatch]::StartNew()
        while ((-not $stdout.Done -or -not $stderr.Done) -and
                $drain.ElapsedMilliseconds -lt 10000) {
            Start-Sleep -Milliseconds 10
        }
        if (-not $stdout.Done -or -not $stderr.Done) {
            throw 'Process output capture did not complete.'
        }
        $record.StandardOutput = $stdout.Text.ToString().Trim()
        $record.StandardError = $stderr.Text.ToString().Trim()

        if ($ExpectTimeout) {
            if (-not $record.TimedOut) {
                throw "Expected timeout after $TimeoutSeconds seconds was not detected."
            }
            $record.Status = 'Pass'
        } else {
            if ($record.TimedOut) {
                throw "Exceeded the $TimeoutSeconds-second timeout."
            }
            if ($record.ExitCode -ne $ExpectedExitCode) {
                throw "Exited with code $($record.ExitCode); expected $ExpectedExitCode."
            }
            if ($ExpectedOutput) {
                $actualOutput = $record[$ExpectedStream]
                foreach ($expectedText in $ExpectedOutput) {
                    if ($actualOutput -notmatch [regex]::Escape($expectedText)) {
                        throw "$ExpectedStream did not contain '$expectedText'."
                    }
                }
            }
            $record.Status = 'Pass'
        }
    } catch {
        $record.Failure = $_.Exception.Message
    } finally {
        if ($null -ne $stdoutSubscription) {
            Unregister-Event -SourceIdentifier $stdoutSource
            Remove-Job -Job $stdoutSubscription -Force
        }
        if ($null -ne $stderrSubscription) {
            Unregister-Event -SourceIdentifier $stderrSource
            Remove-Job -Job $stderrSubscription -Force
        }
        if ($null -ne $process) { $process.Close() }
        $stopwatch.Stop()
        $record.DurationMilliseconds = $stopwatch.ElapsedMilliseconds
    }
    return $record
}

function Invoke-IsolatedAgentFailureTest {
    param(
        [Parameter(Mandatory = $true)][string]$Case,
        [string]$ProtocolMode
    )

    $caseDirectory = Join-Path $OutputDirectory (
        $resultBaseName + '-agent-' + $Case)
    if (Test-Path -LiteralPath $caseDirectory) {
        Remove-Item -LiteralPath $caseDirectory -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($caseDirectory) | Out-Null
    Copy-Item -LiteralPath (Join-Path $testDirectory 'BackendSmokeTest.exe') `
        -Destination $caseDirectory
    Copy-Item -LiteralPath (Join-Path $binDirectory 'VT7Pty.dll') `
        -Destination $caseDirectory

    if ($ProtocolMode) {
        Copy-Item -LiteralPath (Join-Path $testDirectory 'ProtocolTestAgent.exe') `
            -Destination (Join-Path $caseDirectory 'VT7Pty-Agent.exe')
        return Invoke-AcceptanceTest -Name "Agent rejection: $Case" `
            -Executable (Join-Path $caseDirectory 'BackendSmokeTest.exe') `
            -Arguments @('EXPECT_INCOMPATIBLE_AGENT') `
            -Environment @{ VT7PTY_PROTOCOL_TEST_MODE = $ProtocolMode }
    }

    return Invoke-AcceptanceTest -Name "Agent rejection: $Case" `
        -Executable (Join-Path $caseDirectory 'BackendSmokeTest.exe') `
        -Arguments @('EXPECT_MISSING_AGENT')
}

function Invoke-DiagnosticTransportTest {
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $logPath = Join-Path $OutputDirectory ($resultBaseName + '-diagnostics.jsonl')
    $record = @{
        Name = 'Structured diagnostic transport and bundle'
        Executable = 'VT7Pty-DebugServer.exe + ProtocolTest.exe + New-VT7PtyDiagnosticBundle.ps1'
        Arguments = [string[]]@('--output', $logPath, '--max-bytes', '4096',
            '--max-messages', '3')
        Status = 'Fail'
        DurationMilliseconds = 0
        ExitCode = $null
        TimedOut = $false
        StandardOutput = ''
        StandardError = ''
        Failure = $null
    }
    $server = $null
    $serverOutSubscription = $null
    $serverErrSubscription = $null
    $serverOutSource = 'vt7pty.serverout.' + [guid]::NewGuid().ToString('N')
    $serverErrSource = 'vt7pty.servererr.' + [guid]::NewGuid().ToString('N')
    try {
        $serverInfo = New-Object Diagnostics.ProcessStartInfo
        $serverInfo.FileName = Join-Path $binDirectory 'VT7Pty-DebugServer.exe'
        $serverInfo.WorkingDirectory = $packageRoot
        $serverInfo.UseShellExecute = $false
        $serverInfo.CreateNoWindow = $true
        $serverInfo.RedirectStandardOutput = $true
        $serverInfo.RedirectStandardError = $true
        $serverInfo.Arguments = '--output "' + $logPath +
            '" --max-bytes 4096 --max-messages 3'
        $server = New-Object Diagnostics.Process
        $server.StartInfo = $serverInfo
        if (-not $server.Start()) {
            throw 'Could not start diagnostic server.'
        }
        $serverOutput = @{ Text = (New-Object Text.StringBuilder); Done = $false }
        $serverError = @{ Text = (New-Object Text.StringBuilder); Done = $false }
        $serverOutSubscription = Register-ObjectEvent -InputObject $server `
            -EventName OutputDataReceived -SourceIdentifier $serverOutSource `
            -MessageData $serverOutput -Action {
                if ($null -eq $EventArgs.Data) {
                    $Event.MessageData.Done = $true
                } else {
                    [void]$Event.MessageData.Text.AppendLine($EventArgs.Data)
                }
            }
        $serverErrSubscription = Register-ObjectEvent -InputObject $server `
            -EventName ErrorDataReceived -SourceIdentifier $serverErrSource `
            -MessageData $serverError -Action {
                if ($null -eq $EventArgs.Data) {
                    $Event.MessageData.Done = $true
                } else {
                    [void]$Event.MessageData.Text.AppendLine($EventArgs.Data)
                }
            }
        $server.BeginOutputReadLine()
        $server.BeginErrorReadLine()
        Start-Sleep -Milliseconds 250
        $emitter = Invoke-AcceptanceTest -Name 'Diagnostic emitter' `
            -Executable (Join-Path $testDirectory 'ProtocolTest.exe') `
            -Arguments @('EMIT_DIAGNOSTICS') `
            -Environment @{ VT7PTY_DEBUG = 'trace' }
        if ($emitter.Status -ne 'Pass') {
            throw "Diagnostic emitter failed: $($emitter.Failure)"
        }
        if (-not $server.WaitForExit($TestTimeoutSeconds * 1000)) {
            $record.TimedOut = $true
            Stop-VT7ProcessTree $server
            throw "Diagnostic server exceeded the $TestTimeoutSeconds-second timeout."
        }
        $record.ExitCode = $server.ExitCode
        $server.WaitForExit()
        $drain = [Diagnostics.Stopwatch]::StartNew()
        while ((-not $serverOutput.Done -or -not $serverError.Done) -and
                $drain.ElapsedMilliseconds -lt 10000) {
            Start-Sleep -Milliseconds 10
        }
        if (-not $serverOutput.Done -or -not $serverError.Done) {
            throw 'Diagnostic server output capture did not complete.'
        }
        $record.StandardOutput = $serverOutput.Text.ToString().Trim()
        $record.StandardError = $serverError.Text.ToString().Trim()
        if ($record.ExitCode -ne 0) {
            throw "Diagnostic server exited with code $($record.ExitCode)."
        }
        if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
            throw 'Diagnostic server did not create its output file.'
        }
        if ((Get-Item -LiteralPath $logPath).Length -gt 4096) {
            throw 'Diagnostic server exceeded its configured file bound.'
        }
        $records = @(Get-Content -LiteralPath $logPath | ForEach-Object {
            $script:vt7Json.DeserializeObject($_)
        })
        if ($records.Count -ne 3 -or
                @($records | Where-Object {
                    $_.product -ne 'VT7Pty' -or
                    $_.version -ne $manifest.Version -or
                    $_.commit -ne $manifest.SourceCommit -or
                    $_.api -ne $manifest.ApiVersion -or
                    [int]$_.protocol -ne [int]$manifest.ProtocolVersion -or
                    $null -eq $_.timestamp -or $null -eq $_.severity -or
                    $null -eq $_.subsystem -or $null -eq $_.pid -or
                    $null -eq $_.tid
                }).Count -ne 0) {
            throw 'Diagnostic records lack required structured build identity.'
        }
        $bundleDirectory = Join-Path $OutputDirectory (
            $resultBaseName + '-diagnostic-bundle')
        & (Join-Path $packageRoot `
            'tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1') `
            -LogPath $logPath `
            -PackageDirectory $packageRoot `
            -OutputDirectory $bundleDirectory `
            -NoArchive
        $bundleManifestPath = Join-Path $bundleDirectory `
            'diagnostic-manifest.json'
        if (-not (Test-Path -LiteralPath $bundleManifestPath -PathType Leaf)) {
            throw 'Diagnostic bundle manifest was not created.'
        }
        $bundleManifest = Read-VT7Json $bundleManifestPath
        if ($bundleManifest.Package.SourceCommit -ne $manifest.SourceCommit -or
                [int]$bundleManifest.Diagnostics.RecordCount -ne 3 -or
                [int]$bundleManifest.Diagnostics.InvalidLineCount -ne 0 -or
                $bundleManifest.Diagnostics.ContainsOptInInputRecords -ne $false) {
            throw 'Diagnostic bundle did not preserve the expected clean identity.'
        }
        $record.Status = 'Pass'
    } catch {
        $record.Failure = $_.Exception.Message
    } finally {
        if ($null -ne $serverOutSubscription) {
            Unregister-Event -SourceIdentifier $serverOutSource
            Remove-Job -Job $serverOutSubscription -Force
        }
        if ($null -ne $serverErrSubscription) {
            Unregister-Event -SourceIdentifier $serverErrSource
            Remove-Job -Job $serverErrSubscription -Force
        }
        if ($null -ne $server) { $server.Close() }
        $stopwatch.Stop()
        $record.DurationMilliseconds = $stopwatch.ElapsedMilliseconds
    }
    return $record
}

[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$resultBaseName = "windows7-$($Tier.ToLowerInvariant())-$timestamp"
$jsonPath = Join-Path $OutputDirectory ($resultBaseName + '.json')
$textPath = Join-Path $OutputDirectory ($resultBaseName + '.txt')

$manifest = Read-VT7Json $manifestPath
$integrityFailures = @()
foreach ($file in $manifest.Files) {
    $filePath = Join-Path $packageRoot ($file.Path.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        $integrityFailures += "Missing: $($file.Path)"
    } else {
        $actualHash = Get-VT7Sha256 $filePath
        if ($actualHash -ne $file.Sha256) {
            $integrityFailures += "Hash mismatch: $($file.Path)"
        }
    }
}

$osVersion = [Environment]::OSVersion.Version
$windowsRegistry = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$inventoryWarnings = @()
try {
    $operatingSystem = Get-WmiObject -Class Win32_OperatingSystem
} catch {
    $inventoryWarnings += "Win32_OperatingSystem inventory was unavailable: $($_.Exception.Message)"
    $servicePackMajor = 0
    $servicePackText = $windowsRegistry.PSObject.Properties['CSDVersion']
    if ($null -ne $servicePackText -and $servicePackText.Value -match '(\d+)') {
        $servicePackMajor = [int]$Matches[1]
    }
    $operatingSystem = New-Object PSObject -Property @{
        Caption = $windowsRegistry.ProductName
        BuildNumber = $windowsRegistry.CurrentBuildNumber
        ServicePackMajorVersion = $servicePackMajor
        ServicePackMinorVersion = 0
        OSArchitecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'AMD64') { '64-bit' } else { '32-bit' }
    }
}
try {
    $computerSystem = Get-WmiObject -Class Win32_ComputerSystem
} catch {
    $inventoryWarnings += "Win32_ComputerSystem inventory was unavailable: $($_.Exception.Message)"
    $bios = Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\BIOS'
    $computerSystem = New-Object PSObject -Property @{
        Manufacturer = $bios.SystemManufacturer
        Model = $bios.SystemProductName
        TotalPhysicalMemory = $null
    }
}
try {
    $processors = @(Get-WmiObject -Class Win32_Processor | ForEach-Object { $_.Name.Trim() })
} catch {
    $inventoryWarnings += "Win32_Processor inventory was unavailable: $($_.Exception.Message)"
    $processors = @($env:PROCESSOR_IDENTIFIER)
}
$preflightFailures = @()
if ($osVersion.Major -ne 6 -or $osVersion.Minor -ne 1) {
    $preflightFailures += "Expected Windows 7 version 6.1; found $osVersion."
}
if ([int]$operatingSystem.ServicePackMajorVersion -lt 1) {
    $preflightFailures += 'Windows 7 Service Pack 1 is required.'
}
if ($operatingSystem.OSArchitecture -ne '64-bit' -or [IntPtr]::Size -ne 8) {
    $preflightFailures += 'The acceptance run requires 64-bit Windows and 64-bit Windows PowerShell.'
}
if ($manifest.Architecture -ne 'x64' -or $manifest.MinimumOperatingSystem -ne 'Windows 7 SP1') {
    $preflightFailures += 'The package manifest does not identify an x64 Windows 7 SP1 candidate.'
}
if ($manifest.SourceTreeClean -ne $true) {
    $preflightFailures += 'The candidate was not produced from a clean source tree.'
}
if ($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2) {
    $preflightFailures += 'The legacy tier requires the installed Windows PowerShell 2.0 engine.'
}
$preflightFailures += $integrityFailures
if ($SoakMinutes -ne 120) {
    $preflightFailures += 'Milestone and release acceptance require the 120-minute soak profile.'
}

$previousPath = $env:PATH
$env:PATH = $binDirectory + ';' + $env:PATH
try {
    $tests = @(
        Invoke-AcceptanceTest -Name 'Modern C++ utility unit test' `
            -Executable (Join-Path $testDirectory 'ModernCppTest.exe') `
            -ExpectedOutput 'VT7Pty modern C++ utility tests passed.'
        Invoke-AcceptanceTest -Name 'Client-agent protocol unit test' `
            -Executable (Join-Path $testDirectory 'ProtocolTest.exe') `
            -ExpectedOutput 'VT7Pty protocol tests passed'
        Invoke-AcceptanceTest -Name 'Diagnostic pipe security self-test' `
            -Executable (Join-Path $binDirectory 'VT7Pty-DebugServer.exe') `
            -Arguments @('--self-test') `
            -ExpectedOutput 'diagnostic security self-test passed'
        Invoke-AcceptanceTest -Name 'Diagnostic build identity' `
            -Executable (Join-Path $binDirectory 'VT7Pty-DebugServer.exe') `
            -Arguments @('--version') `
            -ExpectedOutput @(
                "VT7Pty version $($manifest.Version)",
                "commit $($manifest.SourceCommit)",
                "API version $($manifest.ApiVersion)",
                "protocol version $($manifest.ProtocolVersion)")
        Invoke-DiagnosticTransportTest
        Invoke-AcceptanceTest -Name 'Inherited lifecycle session' `
            -Executable (Join-Path $testDirectory 'BackendSmokeTest.exe')
        Invoke-AcceptanceTest -Name 'Command Prompt and Windows PowerShell sessions' `
            -Executable (Join-Path $testDirectory 'BackendSmokeTest.exe') `
            -Arguments @('APPLICATIONS') `
            -ExpectedOutput 'Command Prompt and Windows PowerShell sessions passed.'
        foreach ($mode in @('OUTPUT', 'UNICODE', 'INPUT', 'RESIZE', 'REPEAT', 'SHUTDOWN', 'SPAWN_FAILURE', 'CONCURRENT')) {
            Invoke-AcceptanceTest -Name "Session contract: $mode" `
                -Executable (Join-Path $testDirectory 'SessionContractTest.exe') `
                -Arguments @($mode) `
                -TimeoutSeconds $(if ($mode -eq 'REPEAT') { 300 } else { $TestTimeoutSeconds }) `
                -ExpectedOutput "Session contract $mode passed."
        }
        if ($SoakMinutes -gt 0) {
            Invoke-AcceptanceTest -Name "Session contract: SOAK $SoakMinutes minutes" `
                -Executable (Join-Path $testDirectory 'SessionContractTest.exe') `
                -Arguments @('SOAK', [string]$SoakMinutes) `
                -TimeoutSeconds ($SoakMinutes * 60 + 120) `
                -ExpectedOutput 'Session contract SOAK passed.'
        } else {
            @{
                Name = 'Session contract: SOAK 120 minutes'
                Executable = 'tests/SessionContractTest.exe'
                Arguments = @('SOAK', '120')
                Status = 'NotRun'
                DurationMilliseconds = 0
                ExitCode = $null
                TimedOut = $false
                StandardOutput = ''
                StandardError = ''
                Failure = 'SoakMinutes was set to 0.'
            }
        }
        foreach ($control in @(
                @('NEGATIVE_OUTPUT', 'missing or truncated output marker'),
                @('NEGATIVE_STATUS', 'wrong fixture exit status'),
                @('NEGATIVE_ORDER', 'output markers were reordered'),
                @('NEGATIVE_TRUNCATION', 'missing or truncated output marker'),
                @('NEGATIVE_LEAK', 'deliberate handle leak detected'))) {
            Invoke-AcceptanceTest -Name "Negative control: $($control[0])" `
                -Executable (Join-Path $testDirectory 'SessionContractTest.exe') `
                -Arguments @($control[0]) -ExpectedExitCode 1 `
                -ExpectedStream StandardError -ExpectedOutput $control[1]
        }
        Invoke-AcceptanceTest -Name 'Negative control: timeout' `
            -Executable (Join-Path $testDirectory 'SessionContractTest.exe') `
            -Arguments @('NEGATIVE_TIMEOUT') -TimeoutSeconds 2 -ExpectTimeout
        Invoke-AcceptanceTest -Name 'Agent source identity' `
            -Executable (Join-Path $binDirectory 'VT7Pty-Agent.exe') `
            -Arguments @('--version') `
            -ExpectedOutput @(
                "VT7Pty version $($manifest.Version)",
                "commit $($manifest.SourceCommit)",
                "API version $($manifest.ApiVersion)",
                "protocol version $($manifest.ProtocolVersion)")
        Invoke-AcceptanceTest -Name 'Argument quoting fixture' `
            -Executable (Join-Path $testDirectory 'fixture-show-argv.exe') `
            -Arguments @('alpha', 'two words') `
            -ExpectedOutput '[two words]'
        Invoke-AcceptanceTest -Name 'Bounded output fixture' `
            -Executable (Join-Path $testDirectory 'fixture-output-lines.exe') `
            -Arguments @('3', '5') `
            -ExpectedOutput '3 XXXXX'
        Invoke-IsolatedAgentFailureTest -Case 'missing'
        Invoke-IsolatedAgentFailureTest -Case 'malformed' -ProtocolMode 'malformed'
        Invoke-IsolatedAgentFailureTest -Case 'wrong-identity' -ProtocolMode 'wrong-identity'
        Invoke-IsolatedAgentFailureTest -Case 'older-protocol' -ProtocolMode 'older'
        Invoke-IsolatedAgentFailureTest -Case 'newer-protocol' -ProtocolMode 'newer'
    )
} finally {
    $env:PATH = $previousPath
}

$hotfixes = @()
try {
    $hotfixes = @(
        Get-HotFix | Sort-Object HotFixID | ForEach-Object {
            @{
                HotFixId = $_.HotFixID
                Description = $_.Description
                InstalledOn = if ($null -eq $_.InstalledOn) { $null } else { ([datetime]$_.InstalledOn).ToString('yyyy-MM-dd') }
            }
        }
    )
} catch {
    $inventoryWarnings += "Get-HotFix inventory was unavailable: $($_.Exception.Message)"
    try {
        $cbsRoot = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\Packages'
        $hotfixes = @(
            Get-ChildItem -LiteralPath $cbsRoot | ForEach-Object {
                if ($_.PSChildName -match 'KB\d+') {
                    @{
                        HotFixId = $Matches[0]
                        Description = 'CBS package'
                        InstalledOn = $null
                    }
                }
            } | Sort-Object HotFixId -Unique
        )
    } catch {
        $preflightFailures += "Installed-update inventory failed: $($_.Exception.Message)"
    }
}
if ($Tier -eq 'Legacy' -and
        @($hotfixes | Where-Object { $_.HotFixId -eq 'KB3191566' }).Count -ne 0) {
    $preflightFailures += 'The legacy tier must not have KB3191566 installed.'
}
$failedTests = @($tests | Where-Object { $_.Status -ne 'Pass' })
$overallStatus = if ($preflightFailures.Count -eq 0 -and $failedTests.Count -eq 0) { 'Pass' } else { 'Fail' }
$crashArtifacts = @(
    Get-ChildItem -LiteralPath $OutputDirectory -Recurse |
        Where-Object {
            -not $_.PSIsContainer -and
            @('.dmp', '.wer') -contains $_.Extension -and
            $_.LastWriteTimeUtc -ge [datetime]$runStartedAt
        } | ForEach-Object {
            @{
                Path = $_.FullName.Substring($OutputDirectory.Length + 1).Replace('\', '/')
                Bytes = $_.Length
                Sha256 = [string](Get-VT7Sha256 $_.FullName)
            }
        }
)
$record = @{
    SchemaVersion = 1
    Status = $overallStatus
    DeclaredTier = $Tier
    StartedAt = $runStartedAt
    CompletedAt = (Get-Date).ToUniversalTime().ToString('o')
    Package = @{
        Product = $manifest.Product
        Version = $manifest.Version
        ApiVersion = $manifest.ApiVersion
        ProtocolVersion = $manifest.ProtocolVersion
        Configuration = $manifest.Configuration
        Architecture = $manifest.Architecture
        MinimumOperatingSystem = $manifest.MinimumOperatingSystem
        SourceCommit = $manifest.SourceCommit
        ManifestSha256 = [string](Get-VT7Sha256 $manifestPath)
        Toolchain = $manifest.Toolchain
    }
    Machine = @{
        ComputerName = $env:COMPUTERNAME
        Manufacturer = $computerSystem.Manufacturer
        Model = $computerSystem.Model
        TotalPhysicalMemory = if ($null -eq $computerSystem.TotalPhysicalMemory) {
            $null
        } else {
            [uint64]$computerSystem.TotalPhysicalMemory
        }
        Processors = $processors
    }
    OperatingSystem = @{
        ProductName = $windowsRegistry.ProductName
        Caption = $operatingSystem.Caption
        Version = $osVersion.ToString()
        BuildNumber = $operatingSystem.BuildNumber
        ServicePackMajorVersion = $operatingSystem.ServicePackMajorVersion
        ServicePackMinorVersion = $operatingSystem.ServicePackMinorVersion
        Architecture = $operatingSystem.OSArchitecture
        PowerShellVersion = $PSVersionTable.PSVersion.ToString()
        Hotfixes = [hashtable[]]$hotfixes
    }
    InventoryWarnings = $inventoryWarnings
    PreflightFailures = $preflightFailures
    CrashArtifacts = $crashArtifacts
    Tests = [hashtable[]]$tests
}
Write-VT7Json $jsonPath $record

$summary = @(
    "VT7Pty Windows 7 acceptance: $overallStatus"
    "Tier: $Tier"
    "Machine: $($env:COMPUTERNAME) ($($computerSystem.Manufacturer) $($computerSystem.Model))"
    "OS: $($operatingSystem.Caption), version $osVersion, SP$($operatingSystem.ServicePackMajorVersion)"
    "Package: $($manifest.Product) $($manifest.Version), commit $($manifest.SourceCommit)"
    "Manifest SHA-256: $(Get-VT7Sha256 $manifestPath)"
    ""
    "Preflight failures:"
    $(if ($preflightFailures.Count -eq 0) { '  None' } else { $preflightFailures | ForEach-Object { "  $_" } })
    ""
    "Tests:"
    $($tests | ForEach-Object {
        $detail = if ($_.Failure) { " - $($_.Failure)" } else { '' }
        "  $($_.Status): $($_.Name) ($($_.DurationMilliseconds) ms)$detail"
    })
    ""
    "Machine-readable result: $jsonPath"
)
[IO.File]::WriteAllLines($textPath, [string[]]$summary, $utf8NoBom)
$summary | ForEach-Object { Write-Host $_ }

if ($overallStatus -ne 'Pass') {
    exit 1
}
