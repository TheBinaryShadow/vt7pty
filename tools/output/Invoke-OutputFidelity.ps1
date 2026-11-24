[CmdletBinding()]
param(
    [ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')][string]$Tier = 'DevelopmentHost',
    [ValidateSet('Output','Screen','Lifecycle')][string]$Suite = 'Output'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [IO.Path]::GetFullPath((Join-Path $scriptDirectory '..\..')).TrimEnd('\')
$screen = $Suite -eq 'Screen'
$lifecycle = $Suite -eq 'Lifecycle'
try {
    . (Join-Path $root 'tools\platform\Windows7Compat.ps1')
    . (Join-Path $root 'tools\platform\NativeCapture.ps1')
    . (Join-Path $scriptDirectory 'OutputEvidence.ps1')
    if ($screen) {
        . (Join-Path $scriptDirectory 'ScreenEvidence.ps1')
        $manifest = Read-ScreenPackage $root
    } elseif ($lifecycle) {
        . (Join-Path $scriptDirectory 'LifecycleEvidence.ps1')
        $manifest = Read-LifecyclePackage $root
    } else {
        $manifest = Read-OutputPackage $root
    }
} catch {
    Write-Host ($Suite + ' package startup failed: ' + $_.Exception.Message)
    exit 2
}
$roles = if ($screen) { @(Get-ScreenRuntimeRoles $manifest) } else { @('baseline','accepted') }

$runId = $Tier.ToLowerInvariant() + '-' + $Suite.ToLowerInvariant() + '-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') +
    '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$resultRoot = Join-Path $root ('results\' + $runId)
[void][IO.Directory]::CreateDirectory($resultRoot)
$os = Get-WmiObject Win32_OperatingSystem
$updates = @(Get-WmiObject Win32_QuickFixEngineering | ForEach-Object {
    @{Id = $_.HotFixID; InstalledOn = [string]$_.InstalledOn}
})
$preflight = @()
if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { $preflight += 'Use x64 Windows PowerShell.' }
if ($Tier -ne 'DevelopmentHost' -and ($os.Version -cnotlike '6.1.*' -or
    $os.ServicePackMajorVersion -ne 1 -or $os.ProductType -ne 1)) { $preflight += 'Use Windows 7 SP1.' }
if ($Tier -ceq 'Legacy' -and ($PSVersionTable.PSVersion.Major -ne 2 -or
    @($updates | Where-Object { $_.Id -eq 'KB3191566' }).Count)) {
    $preflight += 'The legacy tier uses installed PowerShell 2.0 without KB3191566.'
}

$environment = @{VT7PTY_SHOW_CONSOLE=''; VT7PTY_DEBUG=''}
$report = @{
    SchemaVersion = 1; Kind = 'VT7Pty' + $Suite + 'Results'; RunId = $runId; Tier = $Tier
    ComputerName = $env:COMPUTERNAME; StartedUtc = [DateTime]::UtcNow.ToString('o')
    OS = @{Caption = [string]$os.Caption; Version = [string]$os.Version; BuildNumber = [string]$os.BuildNumber
        ServicePackMajorVersion = [int]$os.ServicePackMajorVersion; ProductType = [int]$os.ProductType}
    Updates = $updates; PowerShellVersion = $PSVersionTable.PSVersion.ToString()
    CLRVersion = [Environment]::Version.ToString()
    ManifestSha256 = Get-VT7Sha256 (Join-Path $root 'manifest.json')
    Harness = $manifest.Harness; Runtimes = $manifest.Runtimes; SessionFlags = $manifest.SessionFlags
    PreflightErrors = $preflight; Oracle = $null; Cases = @(); CharacterizationComplete = $false
    MilestoneAccepted = $false
}
$report[$(if ($screen) { 'Step14Accepted' } elseif ($lifecycle) { 'Step15Accepted' } else { 'Step13Accepted' })] = $false
if ($lifecycle) { $report.Regressions = @() }
$ready = $preflight.Count -eq 0
if ($ready) {
    try {
        $oracle = Invoke-InputProcess (Join-Path $root 'bin\VtStateTest.exe') '' (Join-Path $root 'bin') 30000 $environment
        $report.Oracle = $oracle
        $ready = -not $oracle.TimedOut -and $oracle.ExitCode -eq 0 -and
            $oracle.StandardOutput -ceq 'VT7Pty VT state oracle tests passed.'
    } catch {
        $report.Oracle = @{Error = $_.Exception.Message}
        $ready = $false
    }
}
if ($ready) {
    foreach ($role in $roles) {
        $bin = Join-Path $root ($role + '\bin')
        $exe = Join-Path $bin 'FidelityTest.exe'
        foreach ($spec in $manifest.Cases) {
            $relative = $role + '/' + $spec.Name
            $directory = Join-Path $resultRoot $relative
            [void][IO.Directory]::CreateDirectory($directory)
            $case = @{Runtime = $role; Name = $spec.Name; Area = $spec.Area; Directory = $relative
                Status = 'Incomplete'; Outcome = ''; Controller = $null; Error = ''}
            Write-Host ($role + ' ' + $spec.Name)
            try {
                $controller = Invoke-InputProcess $exe ($spec.Name + ' "' + $directory + '"') $bin 75000 $environment
                $case.Controller = $controller
                if ($controller.TimedOut) { throw 'Native fidelity case exceeded its deadline.' }
                if ($screen) {
                    $observed = Read-ScreenCase $directory $spec $controller.ExitCode $manifest
                } elseif ($lifecycle) {
                    $observed = Read-LifecycleCase $directory $spec $controller.ExitCode $manifest $role
                } else {
                    $observed = Read-OutputCase $directory $spec $controller.ExitCode $manifest.Harness.SnapshotBytes
                }
                $case.Status = 'Observed'
                $case.Outcome = if ($lifecycle) { Get-LifecycleOutcome $observed }
                    elseif ($observed.Status -ceq 'Fail') { 'Difference' }
                    elseif (-not $screen -and $observed.CapabilityUnavailable) { 'CapabilityUnavailable' } else { 'Match' }
            } catch {
                $case.Error = $_.Exception.Message
            }
            $report.Cases += $case
        }
        if ($lifecycle) {
            foreach ($name in $manifest.Regressions) {
                Write-Host ($role + ' regression ' + $name)
                $regression = @{ Runtime = $role; Name = $name; Controller = $null; Error = '' }
                try {
                    $limit = if ($name -ceq 'REPEAT') { 300000 } else { 60000 }
                    $regression.Controller = Invoke-InputProcess (Join-Path $bin 'SessionContractTest.exe') $name $bin $limit $environment
                    if (-not (Test-LifecycleRegression $regression)) { throw 'Session contract regression failed.' }
                } catch {
                    $regression.Error = $_.Exception.Message
                }
                $report.Regressions += $regression
            }
        }
    }
}
$report.CharacterizationComplete = $ready -and $report.Cases.Count -eq $roles.Count * $manifest.Cases.Count -and
    @($report.Cases | Where-Object { $_.Status -ne 'Observed' }).Count -eq 0
if ($lifecycle) {
    $report.CharacterizationComplete = $report.CharacterizationComplete -and
        $report.Regressions.Count -eq $roles.Count * $manifest.Regressions.Count -and
        @($report.Regressions | Where-Object { $_.Error }).Count -eq 0
}
$report.CompletedUtc = [DateTime]::UtcNow.ToString('o')
Write-VT7Json (Join-Path $resultRoot 'report.json') $report
$files = @(Get-ChildItem -LiteralPath $resultRoot -Recurse | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName |
    ForEach-Object {
        @{Path = $_.FullName.Substring($resultRoot.Length + 1).Replace('\','/'); Bytes = $_.Length
            Sha256 = Get-VT7Sha256 $_.FullName}
    })
Write-VT7Json (Join-Path $resultRoot 'evidence-files.json') @{SchemaVersion = 1; Files = $files}
Write-Host ('Results at ' + $resultRoot)
if (-not $report.CharacterizationComplete) {
    Write-Host 'Collection incomplete. Keep the entire result directory.'
    exit 2
}
Write-Host 'Collection complete. Keep all matches, differences and capability observations for review.'
exit 0
