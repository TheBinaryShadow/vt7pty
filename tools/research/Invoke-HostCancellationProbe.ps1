[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')]
    [string]$Tier,
    [string]$PackageRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$taskScript=Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $PackageRoot) { $PackageRoot=Join-Path $taskScript '..\..' }
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
$taskManifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
if ($taskManifest.SchemaVersion -ne 1 -or $taskManifest.Kind -ne 'VT7PtyHostCancellationProbe' -or
    $taskManifest.Harness.Version -ne '1' -or $taskManifest.Harness.SourceTreeClean -ne $true -or
    $taskManifest.Harness.Configuration -ne 'Release' -or $taskManifest.Harness.Architecture -ne 'x64' -or
    $taskManifest.Runtime.SourceCommit -ne 'cbd17727b3c8af21e5afd4499b8dca7d2140ed9d') { throw 'Wrong research candidate identity.' }
foreach($taskFile in $taskManifest.Files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $PackageRoot $taskFile.Path))
    if (-not $taskPath.StartsWith($PackageRoot+'\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path leaves package.' }
    if (-not (Test-Path -LiteralPath $taskPath -PathType Leaf) -or
        (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -ne $taskFile.Sha256) { throw ('File verification failed: '+$taskFile.Path) }
}
$taskExpected=@{
    'VT7Pty.dll'='f0509b31d81508c047fb1f6a20836ce6a4ab97f0d13790da42b387548f955167'
    'VT7Pty-Agent.exe'='b6b7e083e9dab1bbf29603a3721e6d261c94e8ad3a230b8526e7b6123f1df1b2'
}
foreach($taskEntry in $taskExpected.GetEnumerator()) {
    if ((Get-VT7Sha256 (Join-Path $PackageRoot ('bin\'+$taskEntry.Key))) -ne $taskEntry.Value) { throw 'Runtime differs from the accepted release.' }
}
$taskRunId=$Tier.ToLowerInvariant()+'-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $PackageRoot ('results\'+$taskRunId)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskOs=Get-WmiObject Win32_OperatingSystem
$taskComputer=Get-WmiObject Win32_ComputerSystem
$taskUpdates=@(Get-WmiObject Win32_QuickFixEngineering | ForEach-Object { @{Id=$_.HotFixID;InstalledOn=[string]$_.InstalledOn} })
$taskPreflight=@()
if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { $taskPreflight+='Use x64 Windows PowerShell.' }
if ($Tier -ne 'DevelopmentHost' -and (-not $taskOs.Version.StartsWith('6.1.') -or $taskOs.ServicePackMajorVersion -ne 1 -or $taskOs.ProductType -ne 1)) { $taskPreflight+='This tier requires Windows 7 SP1.' }
if ($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2) { $taskPreflight+='Legacy requires installed PowerShell 2.0.' }
if ($Tier -eq 'Legacy' -and @($taskUpdates | Where-Object { $_.Id -eq 'KB3191566' }).Count) { $taskPreflight+='Legacy excludes KB3191566.' }
$taskRecords=@()
foreach($taskIndex in 1..3) {
    $taskRecord=@{Repetition=$taskIndex;Status='NotRun';ExitCode=$null;TimedOut=$false;Error='';DurationMilliseconds=0;Observation=$null;StandardOutput='';StandardError='';Cases=$null;Neighbor=$null}
    $taskCase=Join-Path $taskOutput ('run-'+$taskIndex)
    [IO.Directory]::CreateDirectory($taskCase) | Out-Null
    if (-not $taskPreflight.Count) {
        $taskProcess=New-Object Diagnostics.Process
        $taskWatch=[Diagnostics.Stopwatch]::StartNew()
        $taskStarted=$false
        try {
            $taskStart=New-Object Diagnostics.ProcessStartInfo
            $taskStart.FileName=Join-Path $PackageRoot 'bin\HostCancellationProbe.exe'
            $taskStart.WorkingDirectory=Join-Path $PackageRoot 'bin'
            $taskStart.UseShellExecute=$false; $taskStart.CreateNoWindow=$true
            $taskStart.RedirectStandardOutput=$true; $taskStart.RedirectStandardError=$true
            $taskStart.Arguments='"'+$taskCase+'"'
            $taskProcess.StartInfo=$taskStart
            $taskStarted=$taskProcess.Start()
            if (-not $taskStarted) { throw 'Cannot start research executable.' }
            if (-not $taskProcess.WaitForExit(210000)) {
                $taskRecord.TimedOut=$true
                $taskProcess.Kill() # Only this created controller; never terminate a console host.
                if (-not $taskProcess.WaitForExit(10000)) { throw 'Research executable survived timeout cleanup.' }
            } else { $taskRecord.ExitCode=$taskProcess.ExitCode }
            $taskRecord.StandardOutput=$taskProcess.StandardOutput.ReadToEnd().Trim()
            $taskRecord.StandardError=$taskProcess.StandardError.ReadToEnd().Trim()
            if ($taskRecord.TimedOut) { throw 'Research executable deadline.' }
            $taskRecord.Observation=Read-VT7Json (Join-Path $taskCase 'probe.json')
            $taskNative=$taskRecord.Observation
            $taskRecord.Status='Fail'
            if ($taskNative.SchemaVersion -ne 1 -or $taskNative.Probe -ne 'HostCancellation' -or $taskNative.Version -ne 1) { throw 'Native identity mismatch.' }
            if ($taskNative.ProductionCancellationImplemented -ne $false -or $taskNative.MilestoneAccepted -ne $false) { throw 'Unexpected production or acceptance claim.' }
            $taskCases=Read-VT7Json (Join-Path $taskCase 'suite\cases.json')
            $taskNeighbor=Read-VT7Json (Join-Path $taskCase 'neighbor\neighbor.json')
            $taskExpectedCases=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial')
            $taskActualCases=@($taskCases.Cases | ForEach-Object { $_.Case })
            if (($taskActualCases -join '|') -ne ($taskExpectedCases -join '|')) { throw 'Wrong or incomplete native case inventory.' }
            $taskBadCases=@($taskCases.Cases | Where-Object { $_.Passed -ne $true -or $_.OwnedProcessesCleaned -ne $true -or $_.HostExited -ne $true -or $_.HookRemoved -ne $true -or $_.Error -ne '' })
            $taskExpectedOutput='Host cancellation oracle: 33 checks passed'+[Environment]::NewLine+'Host cancellation probe: Pass'
            if ($taskRecord.ExitCode -eq 0 -and $taskNative.Status -eq 'Pass' -and
                $taskNative.SuiteExit -eq 0 -and $taskNative.NeighborExit -eq 0 -and $taskNative.NeighborPolls -eq 17 -and
                $taskNative.DesktopIsolationVerified -eq $true -and $taskNative.OwnedLifetimesCleaned -eq $true -and
                $taskNative.RootDesktop -eq 'Default' -and $taskNative.SuiteDesktop -match '^VT7Pty-H3-[0-9a-f]{32}$' -and
                $taskNative.NeighborDesktop -match '^VT7Pty-H3-[0-9a-f]{32}$' -and $taskNative.SuiteDesktop -ne $taskNative.NeighborDesktop -and
                $taskCases.Passed -eq $true -and $taskBadCases.Count -eq 0 -and
                $taskNeighbor.Passed -eq $true -and $taskNeighbor.PendingObservations -eq 18 -and $taskNeighbor.HostExited -eq $true -and
                $taskRecord.StandardOutput -eq $taskExpectedOutput -and $taskRecord.StandardError -eq '') { $taskRecord.Status='Pass' }
            else { $taskRecord.Error='Inspect native records for cancellation, shell, isolation or cleanup failures.' }
            $taskRecord.Cases=$taskCases
            $taskRecord.Neighbor=$taskNeighbor
        } catch {
            $taskRecord.Status='Fail'; $taskRecord.Error=$_.Exception.Message
            if ($taskStarted -and -not $taskProcess.HasExited) {
                $taskProcess.Kill() # Only this created controller; never terminate a console host.
                [void]$taskProcess.WaitForExit(10000)
            }
        } finally { $taskWatch.Stop(); $taskRecord.DurationMilliseconds=$taskWatch.ElapsedMilliseconds; $taskProcess.Close() }
    } else { $taskRecord.Error=$taskPreflight -join ' ' }
    $taskRecords+=@($taskRecord)
}
$taskPassed=@($taskRecords | Where-Object { $_.Status -eq 'Pass' }).Count
$taskReport=@{
    SchemaVersion=1;Kind='VT7PtyHostCancellationResults';RunId=$taskRunId;Tier=$Tier
    RecordedUtc=[DateTime]::UtcNow.ToString('o');ComputerName=$env:COMPUTERNAME
    Machine=@{Manufacturer=$taskComputer.Manufacturer;Model=$taskComputer.Model}
    OS=@{Caption=$taskOs.Caption;Version=$taskOs.Version;ServicePackMajorVersion=$taskOs.ServicePackMajorVersion;ProductType=$taskOs.ProductType}
    PowerShellVersion=$PSVersionTable.PSVersion.ToString();CLRVersion=$PSVersionTable.CLRVersion.ToString();Updates=$taskUpdates
    ManifestSha256=(Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json'));Runtime=$taskManifest.Runtime;Harness=$taskManifest.Harness
    PreflightErrors=$taskPreflight;Results=$taskRecords;Pass=$taskPassed;Fail=@($taskRecords | Where-Object { $_.Status -eq 'Fail' }).Count
    NotRun=@($taskRecords | Where-Object { $_.Status -eq 'NotRun' }).Count;MilestoneAccepted=$false;ProductionCancellationImplemented=$false;PrivateDesktopResearch=$true
}
Write-VT7Json (Join-Path $taskOutput 'report.json') $taskReport
[IO.File]::WriteAllText((Join-Path $taskOutput 'summary.txt'),('Host cancellation: '+$taskPassed+'/3 passed. Private-desktop research only; Windows 7 review and remaining H3 coverage are required.'+[Environment]::NewLine),$script:vt7Utf8)
Write-Host ('Results: '+$taskOutput)
Write-Host ('Cancellation research repetitions passed: '+$taskPassed+'/3')
if ($taskPassed -ne 3 -or $taskPreflight.Count) { exit 1 }
exit 0
