[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('NonESU', 'ESU')]
    [string]$Tier,

    [string]$OutputDirectory,

    [int]$TestTimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$packageRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $packageRoot 'results'
}
$binDirectory = Join-Path $packageRoot 'bin'
$testDirectory = Join-Path $packageRoot 'tests'
$manifestPath = Join-Path $packageRoot 'manifest.json'
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$runStartedAt = (Get-Date).ToUniversalTime().ToString('o')

function Invoke-AcceptanceTest {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Executable,
        [string[]]$Arguments = @(),
        [string[]]$ExpectedOutput,
        [hashtable]$Environment = @{},
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
    $record = [ordered]@{
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
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TestTimeoutSeconds * 1000)) {
            $record.TimedOut = $true
            & taskkill.exe /PID $process.Id /T /F | Out-Null
            $process.WaitForExit()
        } else {
            $record.ExitCode = $process.ExitCode
        }
        $record.StandardOutput = $stdoutTask.Result.Trim()
        $record.StandardError = $stderrTask.Result.Trim()

        if ($record.TimedOut) {
            throw "Exceeded the $TestTimeoutSeconds-second timeout."
        }
        if ($record.ExitCode -ne 0) {
            throw "Exited with code $($record.ExitCode)."
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
    } catch {
        $record.Failure = $_.Exception.Message
    } finally {
        $stopwatch.Stop()
        $record.DurationMilliseconds = $stopwatch.ElapsedMilliseconds
    }
    return [pscustomobject]$record
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

[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$resultBaseName = "windows7-$($Tier.ToLowerInvariant())-$timestamp"
$jsonPath = Join-Path $OutputDirectory ($resultBaseName + '.json')
$textPath = Join-Path $OutputDirectory ($resultBaseName + '.txt')

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$integrityFailures = @()
foreach ($file in $manifest.Files) {
    $filePath = Join-Path $packageRoot ($file.Path.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        $integrityFailures += "Missing: $($file.Path)"
    } else {
        $actualHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $file.Sha256) {
            $integrityFailures += "Hash mismatch: $($file.Path)"
        }
    }
}

$osVersion = [Environment]::OSVersion.Version
$windowsRegistry = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$inventoryWarnings = @()
try {
    $operatingSystem = Get-CimInstance -ClassName Win32_OperatingSystem
} catch {
    $inventoryWarnings += "Win32_OperatingSystem inventory was unavailable: $($_.Exception.Message)"
    $servicePackMajor = 0
    $servicePackText = $windowsRegistry.PSObject.Properties['CSDVersion']
    if ($null -ne $servicePackText -and $servicePackText.Value -match '(\d+)') {
        $servicePackMajor = [int]$Matches[1]
    }
    $operatingSystem = [pscustomobject]@{
        Caption = $windowsRegistry.ProductName
        BuildNumber = $windowsRegistry.CurrentBuildNumber
        ServicePackMajorVersion = $servicePackMajor
        ServicePackMinorVersion = 0
        OSArchitecture = if ([Environment]::Is64BitOperatingSystem) { '64-bit' } else { '32-bit' }
    }
}
try {
    $computerSystem = Get-CimInstance -ClassName Win32_ComputerSystem
} catch {
    $inventoryWarnings += "Win32_ComputerSystem inventory was unavailable: $($_.Exception.Message)"
    $bios = Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\BIOS'
    $computerSystem = [pscustomobject]@{
        Manufacturer = $bios.SystemManufacturer
        Model = $bios.SystemProductName
        TotalPhysicalMemory = $null
    }
}
try {
    $processors = @(Get-CimInstance -ClassName Win32_Processor | ForEach-Object { $_.Name.Trim() })
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
if (-not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) {
    $preflightFailures += 'The acceptance run requires 64-bit Windows and 64-bit Windows PowerShell.'
}
if ($manifest.Architecture -ne 'x64' -or $manifest.MinimumOperatingSystem -ne 'Windows 7 SP1') {
    $preflightFailures += 'The package manifest does not identify an x64 Windows 7 SP1 candidate.'
}
if ($manifest.SourceTreeClean -ne $true) {
    $preflightFailures += 'The candidate was not produced from a clean source tree.'
}
$preflightFailures += $integrityFailures

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
        Invoke-AcceptanceTest -Name 'Inherited lifecycle session' `
            -Executable (Join-Path $testDirectory 'BackendSmokeTest.exe')
        Invoke-AcceptanceTest -Name 'Command Prompt and Windows PowerShell sessions' `
            -Executable (Join-Path $testDirectory 'BackendSmokeTest.exe') `
            -Arguments @('APPLICATIONS') `
            -ExpectedOutput 'Command Prompt and Windows PowerShell sessions passed.'
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
            [ordered]@{
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
                    [ordered]@{
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
$failedTests = @($tests | Where-Object { $_.Status -ne 'Pass' })
$overallStatus = if ($preflightFailures.Count -eq 0 -and $failedTests.Count -eq 0) { 'Pass' } else { 'Fail' }
$record = [ordered]@{
    SchemaVersion = 1
    Status = $overallStatus
    DeclaredTier = $Tier
    StartedAt = $runStartedAt
    CompletedAt = (Get-Date).ToUniversalTime().ToString('o')
    Package = [ordered]@{
        Product = $manifest.Product
        Version = $manifest.Version
        ApiVersion = $manifest.ApiVersion
        ProtocolVersion = $manifest.ProtocolVersion
        Configuration = $manifest.Configuration
        Architecture = $manifest.Architecture
        MinimumOperatingSystem = $manifest.MinimumOperatingSystem
        SourceCommit = $manifest.SourceCommit
        ManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Machine = [ordered]@{
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
    OperatingSystem = [ordered]@{
        ProductName = $windowsRegistry.ProductName
        Caption = $operatingSystem.Caption
        Version = $osVersion.ToString()
        BuildNumber = $operatingSystem.BuildNumber
        ServicePackMajorVersion = $operatingSystem.ServicePackMajorVersion
        ServicePackMinorVersion = $operatingSystem.ServicePackMinorVersion
        Architecture = $operatingSystem.OSArchitecture
        PowerShellVersion = $PSVersionTable.PSVersion.ToString()
        Hotfixes = $hotfixes
    }
    InventoryWarnings = $inventoryWarnings
    PreflightFailures = $preflightFailures
    Tests = $tests
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$summary = @(
    "VT7Pty Windows 7 acceptance: $overallStatus"
    "Tier: $Tier"
    "Machine: $($env:COMPUTERNAME) ($($computerSystem.Manufacturer) $($computerSystem.Model))"
    "OS: $($operatingSystem.Caption), version $osVersion, SP$($operatingSystem.ServicePackMajorVersion)"
    "Package: $($manifest.Product) $($manifest.Version), commit $($manifest.SourceCommit)"
    "Manifest SHA-256: $((Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant())"
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
