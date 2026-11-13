[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')][string]$Tier,
    [string]$PackageRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$taskScript=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $PackageRoot) { $PackageRoot=Join-Path $taskScript '..\..' }
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
$taskManifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
if($taskManifest.SchemaVersion -ne 1 -or $taskManifest.Kind -ne 'VT7PtyHostCoordinationDiagnostics' -or
    $taskManifest.Experiment.Version -ne '4' -or -not $taskManifest.Experiment.SourceTreeClean -or
    $taskManifest.Experiment.Architecture -ne 'x64' -or $taskManifest.Experiment.Configuration -ne 'Release' -or
    $taskManifest.Control.ArchiveSha256 -ne '19d2755ad052744953efaed46c91a3fda18694aa6337086808682e22aaf71c1c' -or
    $taskManifest.Control.ManifestSha256 -ne '67b715e9ac7913d6b520ac5bfe99d67b942fb1e8b161d764bd4319c6f85145e7' -or
    $taskManifest.Reference.ManifestSha256 -ne '1ee53b275e70c1361eb0a6fc99c62efa8de6810cbbbb752173024df1393e82d0' -or
    $taskManifest.Reference.ArchiveSha256 -ne 'ed03734b8d6198710f0bfc8ef475b05f36f59949999a08748b9dc34e3aed2371') { throw 'Wrong diagnostic package identity.' }
foreach($taskFile in $taskManifest.Files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $PackageRoot $taskFile.Path))
    if(-not $taskPath.StartsWith($PackageRoot+'\', [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $taskPath -PathType Leaf) -or (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -ne $taskFile.Sha256) { throw ('File verification failed: '+$taskFile.Path) }
}
if((Get-VT7Sha256 (Join-Path $PackageRoot 'control\manifest.json')) -ne $taskManifest.Control.ManifestSha256) { throw 'Original rc1 manifest changed.' }
$taskControl=Read-VT7Json (Join-Path $PackageRoot 'control\manifest.json')
foreach($taskFile in $taskControl.Files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path (Join-Path $PackageRoot 'control') $taskFile.Path))
    if(-not $taskPath.StartsWith($PackageRoot+'\control\', [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $taskPath -PathType Leaf) -or (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -ne $taskFile.Sha256) { throw ('Original rc1 control changed: '+$taskFile.Path) }
}
$taskReferenceRoot=Join-Path $PackageRoot 'reference'
if((Get-VT7Sha256 (Join-Path $taskReferenceRoot 'manifest.json')) -ne $taskManifest.Reference.ManifestSha256) { throw 'Original trace manifest changed.' }
$taskReference=Read-VT7Json (Join-Path $taskReferenceRoot 'manifest.json')
foreach($taskFile in $taskReference.Files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskReferenceRoot $taskFile.Path))
    if(-not $taskPath.StartsWith($taskReferenceRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $taskPath -PathType Leaf) -or (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -ne $taskFile.Sha256) { throw ('Original trace reference changed: '+$taskFile.Path) }
}
$taskRuntime=@{'VT7Pty.dll'='f0509b31d81508c047fb1f6a20836ce6a4ab97f0d13790da42b387548f955167';'VT7Pty-Agent.exe'='b6b7e083e9dab1bbf29603a3721e6d261c94e8ad3a230b8526e7b6123f1df1b2'}
foreach($taskRole in @('control')) {
    foreach($taskEntry in $taskRuntime.GetEnumerator()) {
        if((Get-VT7Sha256 (Join-Path $PackageRoot ($taskRole+'\bin\'+$taskEntry.Key))) -ne $taskEntry.Value) { throw 'Runtime differs from the accepted 0.5.0 pair.' }
    }
}
$taskOldBin=Join-Path $taskReferenceRoot 'experiment\bin'
if((Get-VT7Sha256 (Join-Path $taskOldBin 'VT7Pty.dll')) -ne $taskRuntime['VT7Pty.dll'] -or
    (Get-VT7Sha256 (Join-Path $taskOldBin 'VT7Pty-Agent.exe')) -ne $taskReference.Experiment.AgentSha256 -or
    [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $taskOldBin 'VT7Pty-Agent.exe')).OriginalFilename -ne 'VT7Pty-FreezeAgent.exe') { throw 'The version-3 reference runtime/research agent changed.' }
if((Get-VT7Sha256 (Join-Path $PackageRoot 'experiment\bin\VT7Pty.dll')) -ne $taskRuntime['VT7Pty.dll']) { throw 'Experiment runtime DLL changed.' }
$taskAgent=Join-Path $PackageRoot 'experiment\bin\VT7Pty-Agent.exe'
if((Get-VT7Sha256 $taskAgent) -ne $taskManifest.Experiment.AgentSha256 -or [Diagnostics.FileVersionInfo]::GetVersionInfo($taskAgent).OriginalFilename -ne 'VT7Pty-CoordinatedAgent.exe') { throw 'Wrong research agent.' }
$taskRunId=$Tier.ToLowerInvariant()+'-coordination-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $PackageRoot ('results\'+$taskRunId)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskOs=Get-WmiObject Win32_OperatingSystem
$taskComputer=Get-WmiObject Win32_ComputerSystem
$taskUpdates=@(Get-WmiObject Win32_QuickFixEngineering | ForEach-Object {@{Id=$_.HotFixID;InstalledOn=[string]$_.InstalledOn}})
$taskPreflight=@()
if($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') { $taskPreflight+='Use x64 Windows PowerShell.' }
if($Tier -ne 'DevelopmentHost' -and (-not $taskOs.Version.StartsWith('6.1.') -or $taskOs.ServicePackMajorVersion -ne 1 -or $taskOs.ProductType -ne 1)) { $taskPreflight+='This tier requires Windows 7 SP1.' }
if($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2) { $taskPreflight+='Legacy requires installed PowerShell 2.0.' }
if($Tier -eq 'Legacy' -and @($taskUpdates | Where-Object {$_.Id -eq 'KB3191566'}).Count) { $taskPreflight+='Legacy excludes KB3191566.' }
$taskExpected=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial')
$taskRecords=@()
function Save-DiagnosticReport {
    $taskReport=@{
        SchemaVersion=1;Kind='VT7PtyHostCoordinationResults';RunId=$taskRunId;Tier=$Tier
        RecordedUtc=[DateTime]::UtcNow.ToString('o');ComputerName=$env:COMPUTERNAME
        OS=@{Caption=$taskOs.Caption;Version=$taskOs.Version;ServicePackMajorVersion=$taskOs.ServicePackMajorVersion;ProductType=$taskOs.ProductType}
        Machine=@{Manufacturer=$taskComputer.Manufacturer;Model=$taskComputer.Model}
        PowerShellVersion=$PSVersionTable.PSVersion.ToString();CLRVersion=$PSVersionTable.CLRVersion.ToString();Updates=$taskUpdates
        ManifestSha256=(Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json'));Control=$taskManifest.Control;Reference=$taskManifest.Reference;Experiment=$taskManifest.Experiment;Runtime=$taskManifest.Runtime
        PreflightErrors=$taskPreflight;Trials=$taskRecords;ExpectedTrials=9;MilestoneAccepted=$false;ProductionCancellationImplemented=$false;DiagnosticOnly=$true
        Completed=@($taskRecords | Where-Object {$_.CollectionStatus -eq 'Completed'}).Count
        ControlPass=@($taskRecords | Where-Object {$_.Role -eq 'Control' -and $_.Outcome -eq 'Pass'}).Count
        ControlFail=@($taskRecords | Where-Object {$_.Role -eq 'Control' -and $_.Outcome -eq 'Fail'}).Count
        ReferencePass=@($taskRecords | Where-Object {$_.Role -eq 'Reference' -and $_.Outcome -eq 'Pass'}).Count
        ReferenceFail=@($taskRecords | Where-Object {$_.Role -eq 'Reference' -and $_.Outcome -eq 'Fail'}).Count
        ExperimentPass=@($taskRecords | Where-Object {$_.Role -eq 'Experiment' -and $_.Outcome -eq 'Pass'}).Count
        ExperimentFail=@($taskRecords | Where-Object {$_.Role -eq 'Experiment' -and $_.Outcome -eq 'Fail'}).Count
        FindingsAreAcceptance=$false
    }
    Write-VT7Json (Join-Path $taskOutput 'report.json') $taskReport
}
Save-DiagnosticReport
Write-Host 'Selection challenges can retain failed cancellation outcomes. Keep every trial.'
foreach($taskOrder in 1..9) {
    $taskRole=@('Control','Reference','Experiment')[($taskOrder-1)%3]
    $taskIsTrace=$taskRole -ne 'Control'
    $taskIsExperiment=$taskRole -eq 'Experiment'
    $taskPair=[int][Math]::Ceiling($taskOrder/3.0)
    $taskRotation=$taskPair-1
    $taskTrialName=$taskOrder.ToString('00')+'-'+$taskRole.ToLowerInvariant()
    $taskTrial=Join-Path $taskOutput $taskTrialName
    $taskNative=Join-Path $taskTrial 'native'
    [IO.Directory]::CreateDirectory($taskNative) | Out-Null
    $taskRecord=@{Order=$taskOrder;Group=$taskPair;Role=$taskRole;Rotation=$taskRotation;Directory=$taskTrialName;Outcome='NotRun';CollectionStatus='NotRun';Error='';TimedOut=$false;ExitCode=$null;DurationMilliseconds=0;Observation=$null;Cases=$null;Neighbor=$null;StandardOutput='';StandardError='';ControllerStartQpc='';ControllerEndQpc='';Frequency=[Diagnostics.Stopwatch]::Frequency.ToString()}
    if(-not $taskPreflight.Count) {
        Write-Host ('Trial '+$taskOrder+'/9: '+$taskRole+' group '+$taskPair)
        $taskProcess=New-Object Diagnostics.Process
        $taskWatch=[Diagnostics.Stopwatch]::StartNew()
        $taskStarted=$false
        try {
            $taskStart=New-Object Diagnostics.ProcessStartInfo
            $taskSubdir=if($taskIsExperiment) {'experiment'} elseif($taskIsTrace) {'reference\experiment'} else {'control'}
            $taskName=if($taskIsExperiment) {'HostCancellationCoordinatedProbe.exe'} elseif($taskIsTrace) {'HostCancellationFreezeProbe.exe'} else {'HostCancellationProbe.exe'}
            $taskStart.FileName=Join-Path $PackageRoot ($taskSubdir+'\bin\'+$taskName)
            $taskStart.WorkingDirectory=Join-Path $PackageRoot ($taskSubdir+'\bin')
            $taskStart.Arguments='"'+$taskNative+'"'; $taskStart.UseShellExecute=$false; $taskStart.CreateNoWindow=$true
            $taskStart.RedirectStandardOutput=$true; $taskStart.RedirectStandardError=$true
            if($taskIsTrace) { $taskStart.EnvironmentVariables['VT7PTY_H3_ORDER']=[string]$taskRotation }
            if($taskIsExperiment) { $taskStart.EnvironmentVariables['VT7PTY_H3_NEIGHBOR_SOAK_MS']='0' }
            $taskProcess.StartInfo=$taskStart
            $taskRecord.ControllerStartQpc=[Diagnostics.Stopwatch]::GetTimestamp().ToString()
            $taskStarted=$taskProcess.Start()
            if(-not $taskStarted) { throw 'Cannot start the owned diagnostic controller.' }
            $taskTimeoutMs=if($taskIsExperiment) {480000} elseif($taskIsTrace) {480000} else {210000}
            if(-not $taskProcess.WaitForExit($taskTimeoutMs)) {
                $taskRecord.TimedOut=$true
                $taskProcess.Kill() # Only this created root; its existing parent watches clean its own workers.
                if(-not $taskProcess.WaitForExit(10000)) { throw 'Owned controller survived timeout cleanup.' }
            } else { $taskRecord.ExitCode=$taskProcess.ExitCode }
            $taskRecord.ControllerEndQpc=[Diagnostics.Stopwatch]::GetTimestamp().ToString()
            $taskRecord.StandardOutput=$taskProcess.StandardOutput.ReadToEnd().Trim()
            $taskRecord.StandardError=$taskProcess.StandardError.ReadToEnd().Trim()
            if($taskRecord.TimedOut) { throw 'Native controller deadline; keep all partial records.' }
            $taskRoot=Read-VT7Json (Join-Path $taskNative 'probe.json')
            $taskCases=Read-VT7Json (Join-Path $taskNative 'suite\cases.json')
            $taskNeighbor=Read-VT7Json (Join-Path $taskNative 'neighbor\neighbor.json')
            $taskRecord.Observation=$taskRoot; $taskRecord.Cases=$taskCases; $taskRecord.Neighbor=$taskNeighbor
            $taskVersion=if($taskIsExperiment) {4} elseif($taskIsTrace) {3} else {1}
            if($taskRoot.SchemaVersion -ne 1 -or $taskRoot.Probe -ne 'HostCancellation' -or $taskRoot.Version -ne $taskVersion -or
                $taskRoot.MilestoneAccepted -ne $false -or $taskRoot.ProductionCancellationImplemented -ne $false) { throw 'Native identity or acceptance claim mismatch.' }
            if($taskIsTrace -and ($taskRoot.DiagnosticOnly -ne $true -or $taskRoot.H3Accepted -ne $false -or $taskRoot.OriginalDeadlineFailuresRetained -ne $true)) { throw 'Trace boundary flags are missing.' }
            $taskNames=$taskExpected
            if($taskIsTrace) { $taskNames=@($taskExpected)+@('host-key-record') }
            if($taskIsTrace) {
                foreach($taskRead in @('completed','pending')) {
                    foreach($taskPosition in 0..3) {
                        $taskCondition=@('quiet','selected','coordinated','delay-control')[($taskPosition+$taskRotation)%4]
                        $taskNames+=@('focus-'+$taskRead+'-'+$taskCondition)
                    }
                }
                $taskNames+=@('focus-selection-bracket')
                if($taskIsExperiment) { $taskNames+=@('coordination-expiry') }
            }
            if((@($taskCases.Cases | ForEach-Object {$_.Case}) -join '|') -ne ($taskNames -join '|')) { throw 'Wrong native case inventory.' }
            if($taskIsTrace) {
                if($taskRoot.ResearchAgent -ne $true -or $taskRoot.AgentGateProtocol -ne 1 -or $taskRoot.Rotation -ne $taskRotation) { throw 'Research agent boundary/rotation mismatch.' }
            }
            if($taskIsExperiment -and (-not $taskRoot.BaselineCoordinated -or -not $taskRoot.NeighborPeriodicDrain -or $taskRoot.NeighborSoakMs -ne 0)) { throw 'Coordinated prototype boundary mismatch.' }
            $taskBad=@($taskCases.Cases | Where-Object {-not $_.Passed -or -not $_.OwnedProcessesCleaned -or -not $_.HostExited -or -not $_.HookRemoved -or $_.Error -ne ''})
            $taskRecord.Outcome='Fail'
            if($taskRecord.ExitCode -eq 0 -and $taskRoot.Status -eq 'Pass' -and $taskRoot.SuiteExit -eq 0 -and
                $taskRoot.NeighborExit -eq 0 -and $taskRoot.OwnedLifetimesCleaned -and $taskRoot.DesktopIsolationVerified -and
                $taskCases.Passed -and $taskBad.Count -eq 0 -and $taskNeighbor.Passed -and $taskRecord.StandardError -eq '') { $taskRecord.Outcome='Pass' }
            $taskRecord.CollectionStatus='Completed' # Outcomes remain Pass/Fail independently.
        } catch {
            $taskRecord.Outcome='Fail'; $taskRecord.CollectionStatus='Partial'; $taskRecord.Error=$_.Exception.Message
            if($taskStarted -and -not $taskProcess.HasExited) { $taskProcess.Kill(); [void]$taskProcess.WaitForExit(10000) }
        } finally {
            $taskWatch.Stop(); $taskRecord.DurationMilliseconds=$taskWatch.ElapsedMilliseconds; $taskProcess.Close()
        }
    } else { $taskRecord.Error=$taskPreflight -join ' ' }
    [IO.File]::WriteAllText((Join-Path $taskTrial 'stdout.txt'),$taskRecord.StandardOutput,$script:vt7Utf8)
    [IO.File]::WriteAllText((Join-Path $taskTrial 'stderr.txt'),$taskRecord.StandardError,$script:vt7Utf8)
    $taskFiles=@([IO.Directory]::GetFiles($taskTrial,'*',[IO.SearchOption]::AllDirectories) | Sort-Object | ForEach-Object {
        @{Path=$_.Substring($taskTrial.Length+1).Replace('\','/');Bytes=(Get-Item -LiteralPath $_).Length;Sha256=(Get-VT7Sha256 $_)}
    })
    Write-VT7Json (Join-Path $taskTrial 'evidence-files.json') @{SchemaVersion=1;Files=$taskFiles}
    $taskRecords+=@($taskRecord)
    Save-DiagnosticReport
    Write-Host ($taskRole+' outcome: '+$taskRecord.Outcome+'; collection: '+$taskRecord.CollectionStatus)
}
$taskFailed=@($taskRecords | Where-Object {$_.Outcome -ne 'Pass' -or $_.CollectionStatus -ne 'Completed'}).Count
[IO.File]::WriteAllText((Join-Path $taskOutput 'summary.txt'),('Diagnostic collection: '+@($taskRecords | Where-Object {$_.CollectionStatus -eq 'Completed'}).Count+'/9 trials completed; '+$taskFailed+' trials have failures or incomplete collection. Original outcomes are preserved; this is not H3 acceptance.'+[Environment]::NewLine),$script:vt7Utf8)
Write-Host ('Results: '+$taskOutput)
Write-Host 'Keep the entire results directory, including selected-state cancellation failures. Collection status and original outcomes are separate.'
if($taskFailed -or $taskPreflight.Count) { exit 1 }
exit 0
