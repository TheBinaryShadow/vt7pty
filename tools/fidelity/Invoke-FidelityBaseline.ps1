[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')]
    [string]$Tier,
    [string]$PowerShell7Path = '',
    [string]$PackageRoot = ''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$scriptDirectory=Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $PackageRoot) { $PackageRoot=[IO.Path]::GetFullPath((Join-Path $scriptDirectory '..\..')) }
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
if (-not $PowerShell7Path) { $PowerShell7Path=Join-Path $env:ProgramFiles 'PowerShell\7\pwsh.exe' }
$manifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
$caseManifest=Read-VT7Json (Join-Path $PackageRoot 'cases.json')
if ($manifest.SchemaVersion -ne 1 -or $manifest.Kind -ne 'VT7PtyFidelityBaseline' -or
    $manifest.Runtime.Version -ne '0.5.0' -or
    $manifest.Runtime.SourceCommit -ne 'cbd17727b3c8af21e5afd4499b8dca7d2140ed9d') {
    throw 'This is not the identified VT7Pty 0.5.0 fidelity baseline bundle.'
}
foreach($file in $manifest.Files) {
    $path=[IO.Path]::GetFullPath((Join-Path $PackageRoot $file.Path))
    if (-not $path.StartsWith($PackageRoot+'\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path leaves package root.' }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $file.Bytes -or
        (Get-VT7Sha256 $path) -ne $file.Sha256) { throw "Package file verification failed: $($file.Path)" }
}
$bin=Join-Path $PackageRoot 'bin'
foreach($runtimeFile in @('VT7Pty.dll','VT7Pty-Agent.exe')) {
    if ((Get-VT7Sha256 (Join-Path $bin $runtimeFile)) -ne $manifest.Runtime.Files[$runtimeFile]) {
        throw "Unchanged 0.5.0 runtime check failed: $runtimeFile"
    }
}
$runId=$Tier.ToLowerInvariant()+'-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$resultRoot=Join-Path $PackageRoot ('results\'+$runId)
[IO.Directory]::CreateDirectory($resultRoot) | Out-Null
$os=Get-WmiObject Win32_OperatingSystem
$computer=Get-WmiObject Win32_ComputerSystem
$updates=@(Get-WmiObject Win32_QuickFixEngineering | ForEach-Object { @{Id=$_.HotFixID;InstalledOn=[string]$_.InstalledOn} })
$preflight=@()
if ($Tier -ne 'DevelopmentHost' -and ($manifest.Harness.SourceTreeClean -ne $true -or
    $manifest.Harness.Configuration -ne 'Release' -or $manifest.Harness.Architecture -ne 'x64')) { $preflight+='Target candidates require a clean x64 Release harness.' }
$taskkill=Join-Path ([Environment]::SystemDirectory) 'taskkill.exe'
if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { $preflight+='Use x64 Windows and x64 Windows PowerShell.' }
if ($Tier -ne 'DevelopmentHost' -and
    (-not $os.Version.StartsWith('6.1.') -or $os.ServicePackMajorVersion -ne 1 -or $os.ProductType -ne 1)) { $preflight+='This tier requires Windows 7 SP1.' }
if ($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2) { $preflight+='Legacy tier requires the installed PowerShell 2.0 engine.' }
if ($Tier -eq 'Legacy' -and @($updates | Where-Object { $_.Id -eq 'KB3191566' }).Count -ne 0) { $preflight+='Legacy tier excludes KB3191566.' }
if (($Tier -eq 'NonESU' -or $Tier -eq 'ESU') -and $PSVersionTable.PSVersion.ToString() -notlike '5.1*') { $preflight+='NonESU/ESU shell corpus requires Windows PowerShell 5.1.' }

function Invoke-FidelityCase {
    param($Case)
    $record=@{Name=$Case.Name;Area=$Case.Area;Kind=$Case.Kind;Status='NotRun';Reason='';ExitCode=$null;TimedOut=$false;
        DurationMilliseconds=0;Observation=$null;StandardOutput='';StandardError=''}
    $caseDirectory=Join-Path $resultRoot $Case.Name
    [IO.Directory]::CreateDirectory($caseDirectory) | Out-Null
    if ($preflight.Count) { $record.Reason=($preflight -join ' '); return $record }
    $arguments=@()
    if ($Case.Name -ne 'ORACLE') { $arguments=@($Case.Name,$caseDirectory) }
    if ($Case.Name.StartsWith('PS7')) {
        if ($Tier -eq 'Legacy') {
            $record.Status='NotApplicable'; $record.Reason='Pinned PowerShell 7 is optional on the legacy tier.'; return $record
        }
        if (-not (Test-Path -LiteralPath $PowerShell7Path -PathType Leaf)) {
            $record.Reason='PowerShell 7.2.24 is missing at '+$PowerShell7Path; return $record
        }
        $arguments+=@([IO.Path]::GetFullPath($PowerShell7Path))
    }
    $process=New-Object Diagnostics.Process
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $started=$false
    try {
        $start=New-Object Diagnostics.ProcessStartInfo
        $start.FileName=Join-Path $bin $Case.Executable
        $start.WorkingDirectory=$bin
        $start.UseShellExecute=$false; $start.CreateNoWindow=$true
        $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
        $start.Arguments=($arguments | ForEach-Object { '"' + $_.Replace('"','\"').TrimEnd('\') + '"' }) -join ' '
        $process.StartInfo=$start
        $started=$process.Start()
        if (-not $started) { throw 'Cannot start native harness.' }
        if (-not $process.WaitForExit($Case.TimeoutSeconds*1000)) {
            $record.TimedOut=$true
            & $taskkill /PID $process.Id /T /F | Out-Null
            if (-not $process.WaitForExit(10000)) { throw 'Timed-out harness did not terminate.' }
        } else { $record.ExitCode=$process.ExitCode }
        # Native harness output is bounded to one summary/error line; case data
        # and controlled terminal bytes are written directly into caseDirectory.
        $record.StandardOutput=$process.StandardOutput.ReadToEnd().Trim()
        $record.StandardError=$process.StandardError.ReadToEnd().Trim()
        $watch.Stop(); $record.DurationMilliseconds=$watch.ElapsedMilliseconds
        $record.Status='Fail'
        if ($record.TimedOut) { $record.Reason='Native harness outer deadline.'; return $record }
        if ($Case.Name -eq 'ORACLE') {
            if ($record.ExitCode -eq 0 -and $record.StandardOutput -eq $Case.ExpectedOutput) { $record.Status='Pass' }
            else { $record.Reason='Independent oracle unit tests failed.' }
            return $record
        }
        $casePath=Join-Path $caseDirectory 'case.json'
        if (-not (Test-Path -LiteralPath $casePath -PathType Leaf)) { throw 'Native case record missing.' }
        $observation=Read-VT7Json $casePath
        $record.Observation=$observation
        if ($observation.SchemaVersion -ne 1 -or $observation.Case -ne $Case.Name) { throw 'Native case identity mismatch.' }
        if ($observation.OwnedProcessesCleaned -ne $true) { $record.Reason='Owned process cleanup failed.'; return $record }
        if ($Case.Kind -eq 'NegativeControl') {
            if ($record.ExitCode -eq $Case.ExpectedExitCode -and $observation.Status -eq 'Fail' -and
                $observation.Error -eq $Case.ExpectedError) {
                $record.Status='Pass'; $record.Reason='Injected fault detected; native failure is intentional.'
            } else { $record.Reason='Negative control did not detect its intended fault.' }
        } else {
            $record.Status=$observation.Status
            $record.Reason=$observation.Error
            if ($Case.Name.StartsWith('PS_') -and $observation.ContainsKey('ShellIdentity')) {
                if (($Tier -eq 'Legacy' -and -not $observation.ShellIdentity.StartsWith('2.0|')) -or
                    (($Tier -eq 'NonESU' -or $Tier -eq 'ESU') -and -not $observation.ShellIdentity.StartsWith('5.1.'))) {
                    $record.Status='NotRun'; $record.Reason='The tested shell engine does not match this tier: '+$observation.ShellIdentity
                }
            }
            if ($Case.Name.StartsWith('PS_') -and $observation.Category -eq 'HarnessOrPrerequisiteError' -and
                $observation.Error -like '*Windows PowerShell case requires*') { $record.Status='NotRun' }
            if ($record.ExitCode -ne 0 -and $record.Status -eq 'Pass') { throw 'Native pass has a failing exit code.' }
            if ($Case.Name.StartsWith('PS7') -and $observation.Category -eq 'HarnessOrPrerequisiteError' -and
                $observation.Error -like '*PowerShell 7 case requires*') { $record.Status='NotRun' }
        }
    } catch {
        $record.Status='Fail'; $record.Reason=$_.Exception.Message
        if ($started -and -not $process.HasExited) {
            & $taskkill /PID $process.Id /T /F | Out-Null
            [void]$process.WaitForExit(10000)
        }
    } finally { $watch.Stop(); $record.DurationMilliseconds=$watch.ElapsedMilliseconds; $process.Close() }
    return $record
}
function Get-FidelityApplicationRecords {
    $found=@()
    $moduleRoot=Join-Path (Split-Path -Parent $PowerShell7Path) 'Modules\PSReadLine'
    foreach($path in @((Join-Path $env:windir 'System32\cmd.exe'),(Join-Path $env:windir 'System32\WindowsPowerShell\v1.0\powershell.exe'),$PowerShell7Path,
        (Join-Path $moduleRoot 'PSReadLine.psd1'),(Join-Path $moduleRoot 'Microsoft.PowerShell.PSReadLine2.dll'))) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $found+=@(@{Path=$path;FileVersion=(Get-Item -LiteralPath $path).VersionInfo.FileVersion;Sha256=(Get-VT7Sha256 $path)})
        } else { $found+=@(@{Path=$path;Status='NotRun';Reason='Application/module file missing'}) }
    }
    return $found
}
$applications=@(Get-FidelityApplicationRecords)
$startedUtc=[DateTime]::UtcNow.ToString('o')
$records=@()
foreach($case in $caseManifest.Cases) {
    Write-Host ('Running fidelity '+$case.Name)
    $records+=@(Invoke-FidelityCase $case)
}
$applicationsAfter=@(Get-FidelityApplicationRecords)
$applicationsStable=$true
for($index=0;$index -lt $applications.Count;++$index) {
    $before=$applications[$index]; $after=$applicationsAfter[$index]
    if ($before.ContainsKey('Sha256') -ne $after.ContainsKey('Sha256')) { $applicationsStable=$false }
    elseif ($before.ContainsKey('Sha256') -and $before.Sha256 -ne $after.Sha256) { $applicationsStable=$false }
}
if (-not $applicationsStable) { $preflight+='Application/module files changed during the run.' }
$harnessFailures=@($records | Where-Object { $_.Kind -eq 'Characterization' -and $_.Status -eq 'Fail' -and
    ($null -eq $_.Observation -or $_.Observation.Category -eq 'HarnessOrPrerequisiteError') }).Count
$harnessHealthy=$harnessFailures -eq 0 -and @($records | Where-Object { ($_.Kind -eq 'Oracle' -or $_.Kind -eq 'NegativeControl') -and $_.Status -ne 'Pass' }).Count -eq 0
$missing=@($records | Where-Object { $_.Status -eq 'NotRun' }).Count
$workflowFailures=@($records | Where-Object { $_.Kind -eq 'Characterization' -and $_.Status -eq 'Fail' -and
    $null -ne $_.Observation -and $_.Observation.Category -eq 'BehaviorMismatch' }).Count
$report=@{SchemaVersion=1;Kind='VT7PtyFidelityBaselineResults';Tier=$Tier;RunId=$runId;Runtime=$manifest.Runtime;Harness=$manifest.Harness;
    PackageManifestSha256=(Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json'));Preflight=$preflight;
    Machine=@{Name=$env:COMPUTERNAME;OSCaption=$os.Caption;OSVersion=$os.Version;ServicePack=$os.ServicePackMajorVersion;ProductType=$os.ProductType;
        Manufacturer=$computer.Manufacturer;Model=$computer.Model;PowerShellVersion=$PSVersionTable.PSVersion.ToString();
        CLRVersion=$PSVersionTable.CLRVersion.ToString();Updates=$updates};
    Applications=$applications;ApplicationsAfter=$applicationsAfter;ApplicationsStable=$applicationsStable;
    StartedUtc=$startedUtc;FinishedUtc=[DateTime]::UtcNow.ToString('o');Cases=$records;PendingCoverage=$caseManifest.PendingCoverage;
    HarnessHealthy=$harnessHealthy;CharacterizationComplete=($preflight.Count -eq 0 -and $harnessHealthy -and $missing -eq 0);
    WorkflowFailures=$workflowFailures;HarnessFailures=$harnessFailures;MilestoneAccepted=$false}
Write-VT7Json (Join-Path $resultRoot 'results.json') $report
$summary=New-Object Text.StringBuilder
[void]$summary.AppendLine('VT7Pty 0.5.0 fidelity characterization: '+$Tier)
[void]$summary.AppendLine('Harness healthy: '+$harnessHealthy+'; workflow failures: '+$workflowFailures+'; not run: '+$missing)
foreach($record in $records) { [void]$summary.AppendLine($record.Name+': '+$record.Status+' '+$record.Reason) }
[void]$summary.AppendLine('Pending coverage remains listed in results.json. This run does not accept Milestone 1.')
[IO.File]::WriteAllText((Join-Path $resultRoot 'results.txt'),$summary.ToString(),$script:vt7Utf8)
Write-Host $summary.ToString()
Write-Host ('Results: '+$resultRoot)
if (-not $harnessHealthy -or $missing -ne 0 -or $workflowFailures -ne 0 -or $preflight.Count -ne 0) { exit 1 }
exit 0
