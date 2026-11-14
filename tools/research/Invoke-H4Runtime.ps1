[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')][string]$Tier,
    [string]$PackageRoot='',
    [string]$PowerShell7Path='C:\Program Files\PowerShell\7\pwsh.exe'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$taskScript=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $PackageRoot){$PackageRoot=Join-Path $taskScript '..\..'}
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
. (Join-Path $PackageRoot 'tools\research\H4Cases.ps1')
$taskManifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
if($taskManifest.Kind -ne 'VT7PtyH4RuntimeCandidate' -or $taskManifest.SchemaVersion -ne 1 -or
    -not $taskManifest.Runtime.SourceTreeClean -or $taskManifest.Runtime.ApiVersion -ne '1.1' -or
    $taskManifest.Runtime.ProtocolVersion -ne 2 -or $taskManifest.Reference.ArchiveSha256 -ne
    '620cd3e489fdf53f08184207f947414bd50f89a1ff617507c76c7ac20a099ca7') {throw 'Wrong H4 package identity.'}
foreach($taskFile in $taskManifest.Files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $PackageRoot $taskFile.Path))
    if(-not $taskPath.StartsWith($PackageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
        (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or (Get-VT7Sha256 $taskPath) -ne $taskFile.Sha256) {
        throw ('Package verification failed: '+$taskFile.Path)
    }
}
$taskRunId=$Tier.ToLowerInvariant()+'-h4-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $PackageRoot ('results\'+$taskRunId)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskOs=Get-WmiObject Win32_OperatingSystem
$taskComputer=Get-WmiObject Win32_ComputerSystem
$taskUpdates=@(Get-WmiObject Win32_QuickFixEngineering | ForEach-Object {@{Id=$_.HotFixID;InstalledOn=[string]$_.InstalledOn}})
$taskShells=@()
foreach($taskName in @('cmd.exe','WindowsPowerShell\v1.0\powershell.exe')) {
    $taskPath=Join-Path ([Environment]::SystemDirectory) $taskName
    $taskShells+=@(@{Path=$taskPath;Sha256=(Get-VT7Sha256 $taskPath);FileVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($taskPath).FileVersion})
}
$taskPreflight=@()
if($env:PROCESSOR_ARCHITECTURE -ne 'AMD64'){$taskPreflight+='Use x64 Windows PowerShell.'}
if($Tier -ne 'DevelopmentHost' -and (-not $taskOs.Version.StartsWith('6.1.') -or $taskOs.ServicePackMajorVersion -ne 1 -or $taskOs.ProductType -ne 1)){$taskPreflight+='This tier requires Windows 7 SP1.'}
if($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2){$taskPreflight+='Legacy requires installed PowerShell 2.0.'}
if($Tier -eq 'Legacy' -and @($taskUpdates | Where-Object {$_.Id -eq 'KB3191566'}).Count){$taskPreflight+='Legacy excludes KB3191566.'}
$taskPs7=@{Required=($Tier -ne 'Legacy');Applicable=($Tier -ne 'Legacy');Path='';EngineVersion='';ExecutableSha256='';ModuleManifestSha256='';Reason='';Error=''}
if($taskPs7.Required) {
    try {
        $PowerShell7Path=[IO.Path]::GetFullPath($PowerShell7Path)
        $taskPs7.Path=$PowerShell7Path
        $taskPs7.ExecutableSha256=Get-VT7Sha256 $PowerShell7Path
        $taskVersion=(& $PowerShell7Path -NoLogo -NoProfile -NonInteractive -Command '$PSVersionTable.PSVersion.ToString()' 2>&1 | Out-String).Trim()
        if($LASTEXITCODE -ne 0 -or $taskVersion -ne '7.2.24'){throw 'PowerShell 7.2.24 x64 is required.'}
        $taskPs7.EngineVersion=$taskVersion
        $taskModule=Join-Path (Split-Path -Parent $PowerShell7Path) 'Modules\PSReadLine\PSReadLine.psd1'
        $taskPs7.ModuleManifestSha256=Get-VT7Sha256 $taskModule
    } catch {$taskPs7.Error=$_.Exception.Message;$taskPreflight+=$_.Exception.Message}
} else {$taskPs7.Reason='NotApplicable on LEOPARD; installed Windows PowerShell 2.0 remains required.'}
$taskTrials=@()
$taskRegression=@{Status='NotRun';ExitCode=$null;Directory='regression';SoakMinutes=120;Reason=''}
function Save-H4Report {
    Write-VT7Json (Join-Path $taskOutput 'report.json') @{
        SchemaVersion=1;Kind='VT7PtyH4RuntimeResults';RunId=$taskRunId;Tier=$Tier;ComputerName=$env:COMPUTERNAME
        RecordedUtc=[DateTime]::UtcNow.ToString('o');OS=@{Caption=$taskOs.Caption;Version=$taskOs.Version;ServicePackMajorVersion=$taskOs.ServicePackMajorVersion;ProductType=$taskOs.ProductType}
        Machine=@{Manufacturer=$taskComputer.Manufacturer;Model=$taskComputer.Model};Updates=$taskUpdates;SystemShells=$taskShells
        PowerShellVersion=$PSVersionTable.PSVersion.ToString();CLRVersion=$PSVersionTable.CLRVersion.ToString();PowerShell7=$taskPs7
        ManifestSha256=(Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json'));Runtime=$taskManifest.Runtime;Reference=$taskManifest.Reference
        PreflightErrors=$taskPreflight;Trials=$taskTrials;ExpectedTrials=9;ExpectedRuntimeCases=@(Get-H4CaseNames $taskPs7.Required).Count
        Regression=$taskRegression;ControlFailuresRetained=$true;H4Accepted=$false;MilestoneAccepted=$false
        ManualKeyboardQualification='Pending';OptInSustainedWorkloadQualification='Pending'
    }
}
# CLR 2-compatible readers drain both pipes while the controller is running.
# This prevents a completed long regression from blocking on a full stdout pipe.
if(-not ('VT7PtyH4Capture' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Threading;
public sealed class VT7PtyH4Capture {
    private readonly StringBuilder buffer = new StringBuilder();
    private readonly object gate = new object();
    private readonly Thread worker;
    public bool Truncated;
    public string Error = "";
    public VT7PtyH4Capture(StreamReader reader) {
        worker = new Thread(new ThreadStart(delegate {
            try {
                char[] chunk = new char[4096];
                int count;
                while ((count = reader.Read(chunk, 0, chunk.Length)) != 0) {
                    lock(gate) {
                        int available = 4194304 - buffer.Length;
                        if (count > available) Truncated = true;
                        if (available > 0) buffer.Append(chunk, 0, Math.Min(count, available));
                    }
                }
            } catch(Exception error) { Error = error.Message; }
        }));
        worker.IsBackground = true;
        worker.Start();
    }
    public bool Finish(int milliseconds) { return worker.Join(milliseconds); }
    public string Text { get { lock(gate) { return buffer.ToString(); } } }
}
"@
}

function Invoke-H4Process([string]$Executable,[string]$Arguments,[string]$WorkingDirectory,[int]$TimeoutMs,[hashtable]$Environment) {
    $taskStart=New-Object Diagnostics.ProcessStartInfo
    $taskStart.FileName=$Executable;$taskStart.Arguments=$Arguments;$taskStart.WorkingDirectory=$WorkingDirectory
    $taskStart.UseShellExecute=$false;$taskStart.CreateNoWindow=$true;$taskStart.RedirectStandardOutput=$true;$taskStart.RedirectStandardError=$true
    foreach($taskKey in $Environment.Keys){
        if([string]$Environment[$taskKey] -eq ''){$taskStart.EnvironmentVariables.Remove($taskKey)}
        else {$taskStart.EnvironmentVariables[$taskKey]=[string]$Environment[$taskKey]}
    }
    $taskProcess=New-Object Diagnostics.Process;$taskProcess.StartInfo=$taskStart
    $taskClock=[Diagnostics.Stopwatch]::StartNew()
    try {
        if(-not $taskProcess.Start()){throw 'Cannot start the created test controller.'}
        $taskOut=New-Object VT7PtyH4Capture -ArgumentList $taskProcess.StandardOutput
        $taskErr=New-Object VT7PtyH4Capture -ArgumentList $taskProcess.StandardError
        $taskTimedOut=-not $taskProcess.WaitForExit($TimeoutMs)
        if($taskTimedOut) {
            # Only this created controller. Session-bound monitors retire its
            # owned consoles; the runner never opens or terminates a conhost.
            $taskProcess.Kill();[void]$taskProcess.WaitForExit(10000)
        }
        if(-not $taskOut.Finish(10000) -or -not $taskErr.Finish(10000)){throw 'Controller output drain deadline.'}
        if($taskOut.Truncated -or $taskErr.Truncated -or $taskOut.Error -or $taskErr.Error){throw 'Controller output was truncated or could not be collected.'}
        $taskStdout=$taskOut.Text.Trim()
        $taskStderr=$taskErr.Text.Trim()
        return @{ExitCode=$taskProcess.ExitCode;TimedOut=$taskTimedOut;StandardOutput=$taskStdout;StandardError=$taskStderr;DurationMilliseconds=$taskClock.ElapsedMilliseconds}
    } finally {$taskClock.Stop();$taskProcess.Close()}
}
Save-H4Report
Write-Host 'H4 uses an opt-in runtime. Original rc1 failures are retained separately.'
Write-Host 'The Windows 7 regression phase includes the full 120-minute baseline soak.'
foreach($taskOrder in 1..9) {
    $taskRole=@('Control','Reference','Runtime')[($taskOrder-1)%3]
    $taskGroup=[int][Math]::Ceiling($taskOrder/3.0)
    $taskName=$taskOrder.ToString('00')+'-'+$taskRole.ToLowerInvariant()
    $taskTrial=Join-Path $taskOutput $taskName
    [IO.Directory]::CreateDirectory($taskTrial) | Out-Null
    $taskNative=Join-Path $taskTrial 'native'
    $taskRecord=@{Order=$taskOrder;Group=$taskGroup;Role=$taskRole;Directory=$taskName;Outcome='NotRun';NativeOutcome='NotRun';CollectionStatus='NotRun';Error='';TimedOut=$false;ExitCode=$null;DurationMilliseconds=0;StandardOutput='';StandardError=''}
    if(-not $taskPreflight.Count) {
        try {
            Write-Host ('Trial '+$taskOrder+'/9: '+$taskRole)
            $taskEnv=@{'VT7PTY_H3_ORDER'=[string]($taskGroup-1);'VT7PTY_H3_GUARD_TIER'=$(if($Tier -eq 'DevelopmentHost'){'Development'}else{$Tier});'VT7PTY_H3_NEIGHBOR_SOAK_MS'='0'}
            if($taskPs7.Required){$taskEnv['VT7PTY_H3_PWSH']=$PowerShell7Path;$taskEnv['VT7PTY_H4_LEGACY']=''}
            else {$taskEnv['VT7PTY_H4_LEGACY']='1'}
            if($taskRole -eq 'Runtime'){$taskBin=Join-Path $PackageRoot 'runtime\bin';$taskExe='RuntimeCancellationTest.exe';$taskArgs='--run "'+$taskNative+'"'}
            elseif($taskRole -eq 'Reference'){$taskBin=Join-Path $PackageRoot 'reference\experiment\bin';$taskExe='HostCancellationGuardProbe.exe';$taskArgs='"'+$taskNative+'"'}
            else {$taskBin=Join-Path $PackageRoot 'reference\control\bin';$taskExe='HostCancellationProbe.exe';$taskArgs='"'+$taskNative+'"'}
            if($taskRole -ne 'Runtime'){[IO.Directory]::CreateDirectory($taskNative) | Out-Null}
            $taskNativeRun=Invoke-H4Process (Join-Path $taskBin $taskExe) $taskArgs $taskBin 900000 $taskEnv
            foreach($taskField in $taskNativeRun.Keys){$taskRecord[$taskField]=$taskNativeRun[$taskField]}
            if($taskRecord.TimedOut){throw 'Controller deadline; keep all partial evidence.'}
            $taskProbe=Read-VT7Json (Join-Path $taskNative 'probe.json')
            $taskRecord.NativeOutcome=$taskProbe.Status
            $taskRecord.Outcome=$taskProbe.Status
            if($taskRole -eq 'Runtime') {
                $taskCases=(Read-VT7Json (Join-Path $taskNative 'cases.json')).Cases
                $taskExpected=@(Get-H4CaseNames $taskPs7.Required)
                if($taskProbe.Kind -ne 'H4RuntimeProbe' -or $taskProbe.SourceCommit -ne $taskManifest.Runtime.SourceCommit -or
                    (@($taskCases | ForEach-Object {$_.Case}) -join '|') -ne ($taskExpected -join '|') -or $taskProbe.Total -ne $taskExpected.Count){throw 'Wrong native runtime identity or case inventory.'}
            } else {
                $taskVersion=if($taskRole -eq 'Control'){1}else{5}
                if($taskProbe.Probe -ne 'HostCancellation' -or $taskProbe.Version -ne $taskVersion){throw 'Wrong frozen reference identity.'}
            }
            if(($taskRecord.Outcome -eq 'Pass') -ne ($taskRecord.ExitCode -eq 0)){throw 'Native status and exit status disagree.'}
            $taskRecord.CollectionStatus='Completed'
        } catch {$taskRecord.Outcome='Fail';$taskRecord.CollectionStatus='Partial';$taskRecord.Error=$_.Exception.Message}
    } else {$taskRecord.Error=$taskPreflight -join ' '}
    [IO.File]::WriteAllText((Join-Path $taskTrial 'stdout.txt'),$taskRecord.StandardOutput,$script:vt7Utf8)
    [IO.File]::WriteAllText((Join-Path $taskTrial 'stderr.txt'),$taskRecord.StandardError,$script:vt7Utf8)
    $taskFiles=@([IO.Directory]::GetFiles($taskTrial,'*',[IO.SearchOption]::AllDirectories) | Sort-Object | ForEach-Object {
        @{Path=$_.Substring($taskTrial.Length+1).Replace('\','/');Bytes=(Get-Item -LiteralPath $_).Length;Sha256=(Get-VT7Sha256 $_)}
    })
    Write-VT7Json (Join-Path $taskTrial 'evidence-files.json') @{SchemaVersion=1;Files=$taskFiles}
    $taskTrials+=@($taskRecord);Save-H4Report
    Write-Host ($taskRole+': '+$taskRecord.Outcome+'; collection '+$taskRecord.CollectionStatus)
}
if($Tier -eq 'DevelopmentHost') {
    $taskRegression.Status='NotApplicable';$taskRegression.Reason='Windows 7 acceptance runs on the three target tiers. Exact source Debug/Release verification is recorded in the package.'
} elseif(-not $taskPreflight.Count) {
    try {
        Write-Host 'Starting the full regression suite and 120-minute baseline soak.'
        $taskScript=Join-Path $PackageRoot 'regression\tools\platform\Invoke-Windows7Acceptance.ps1'
        $taskShell=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'
        $taskArgs='-NoLogo -NoProfile -ExecutionPolicy Bypass -File "'+$taskScript+'" -Tier '+$Tier+' -OutputDirectory "'+(Join-Path $taskOutput 'regression')+'"'
        $taskRun=Invoke-H4Process $taskShell $taskArgs (Join-Path $PackageRoot 'regression') 8100000 @{}
        $taskRegression.ExitCode=$taskRun.ExitCode
        $taskRegression.Status=if($taskRun.ExitCode -eq 0 -and -not $taskRun.TimedOut){'Pass'}else{'Fail'}
        $taskRegression.Reason=$taskRun.StandardError
        [IO.File]::WriteAllText((Join-Path $taskOutput 'regression-stdout.txt'),$taskRun.StandardOutput,$script:vt7Utf8)
        [IO.File]::WriteAllText((Join-Path $taskOutput 'regression-stderr.txt'),$taskRun.StandardError,$script:vt7Utf8)
    } catch {$taskRegression.Status='Fail';$taskRegression.Reason=$_.Exception.Message}
}
Save-H4Report
$taskFailures=@($taskTrials | Where-Object {$_.CollectionStatus -ne 'Completed' -or ($_.Role -ne 'Control' -and $_.Outcome -ne 'Pass')}).Count
$taskSummary='H4 collection: '+@($taskTrials | Where-Object {$_.CollectionStatus -eq 'Completed'}).Count+'/9 trials completed. Runtime/reference failures or incomplete trials: '+$taskFailures+'. Regression: '+$taskRegression.Status+'. Original control failures remain failures. H4 acceptance is pending independent review, keyboard and sustained opt-in workload qualification.'
[IO.File]::WriteAllText((Join-Path $taskOutput 'summary.txt'),$taskSummary+[Environment]::NewLine,$script:vt7Utf8)
$taskAllFiles=@([IO.Directory]::GetFiles($taskOutput,'*',[IO.SearchOption]::AllDirectories) | Sort-Object | ForEach-Object {
    @{Path=$_.Substring($taskOutput.Length+1).Replace('\','/');Bytes=(Get-Item -LiteralPath $_).Length;Sha256=(Get-VT7Sha256 $_)}
})
Write-VT7Json (Join-Path $taskOutput 'evidence-files.json') @{SchemaVersion=1;Files=$taskAllFiles}

Write-Host $taskSummary
Write-Host ('Results: '+$taskOutput)
if($taskFailures -or $taskPreflight.Count -or $taskRegression.Status -eq 'Fail'){exit 1}
exit 0
