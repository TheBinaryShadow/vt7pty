[CmdletBinding()]
param(
    [ValidateSet('DevelopmentHost','NonESU','ESU','Legacy')][string]$Tier='DevelopmentHost',
    [string]$PowerShell7Path='C:\Program Files\PowerShell\7\pwsh.exe',
    [string]$PackageRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
if(-not $PackageRoot){$PackageRoot=Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..'}
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
. (Join-Path $PackageRoot 'tools\platform\NativeCapture.ps1')
$taskManifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
if($taskManifest.Kind -cne 'VT7PtyInputCandidate' -or $taskManifest.SchemaVersion -ne 2 -or
    $taskManifest.Runtime.UnchangedFromH4 -or -not $taskManifest.Runtime.SourceTreeClean -or
    $taskManifest.Runtime.ApiVersion -cne '1.1' -or $taskManifest.Runtime.ProtocolVersion -ne 2 -or
    -not $taskManifest.Reference.Unchanged -or $taskManifest.Reference.ArchiveSha256 -cne '84f1bd227af856ec624c88d77c504417dea312bf0e4b3e1df03f28b73c4ab8ac'){
    throw 'Use the complete issued input candidate.'
}
foreach($taskFile in $taskManifest.Files){
    $taskPath=[IO.Path]::GetFullPath((Join-Path $PackageRoot $taskFile.Path))
    if(-not $taskPath.StartsWith($PackageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
        (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -cne $taskFile.Sha256){throw ('Changed candidate file: '+$taskFile.Path)}
}
$taskRunId=$Tier.ToLowerInvariant()+'-input-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $PackageRoot ('results\'+$taskRunId)
[void][IO.Directory]::CreateDirectory($taskOutput)
$taskOs=Get-WmiObject Win32_OperatingSystem
$taskComputer=Get-WmiObject Win32_ComputerSystem
$taskUpdates=@(Get-WmiObject Win32_QuickFixEngineering | Sort-Object HotFixID | ForEach-Object {@{Id=$_.HotFixID}})
$taskPreflight=@()
if($env:PROCESSOR_ARCHITECTURE -ne 'AMD64'){$taskPreflight+='Use x64 Windows PowerShell.'}
if($Tier -ne 'DevelopmentHost' -and (-not $taskOs.Version.StartsWith('6.1.') -or $taskOs.ServicePackMajorVersion -ne 1 -or $taskOs.ProductType -ne 1)){$taskPreflight+='This tier requires Windows 7 SP1.'}
if($Tier -eq 'Legacy' -and $PSVersionTable.PSVersion.Major -ne 2){$taskPreflight+='Legacy requires installed PowerShell 2.0.'}
if($Tier -eq 'Legacy' -and @($taskUpdates | Where-Object {$_.Id -eq 'KB3191566'}).Count){$taskPreflight+='Legacy excludes KB3191566.'}
$taskPs7=@{Required=($Tier -ne 'Legacy');Applicable=($Tier -ne 'Legacy');Path='';Version='';Sha256='';ModuleManifestSha256='';Reason=''}
if($taskPs7.Required){
    try{
        $PowerShell7Path=[IO.Path]::GetFullPath($PowerShell7Path)
        $taskVersion=(& $PowerShell7Path -NoLogo -NoProfile -NonInteractive -Command '$PSVersionTable.PSVersion.ToString()' 2>&1 | Out-String).Trim()
        if($LASTEXITCODE -ne 0 -or $taskVersion -cne '7.2.24'){throw 'PowerShell 7.2.24 is required for the frozen H4 guards.'}
        $taskPs7.Path=$PowerShell7Path;$taskPs7.Version=$taskVersion;$taskPs7.Sha256=Get-VT7Sha256 $PowerShell7Path
        $taskPs7.ModuleManifestSha256=Get-VT7Sha256 (Join-Path (Split-Path -Parent $PowerShell7Path) 'Modules\PSReadLine\PSReadLine.psd1')
    }catch{$taskPreflight+=$_.Exception.Message}
}else{$taskPs7.Reason='NotApplicable on the legacy tier; installed Windows PowerShell 2.0 remains required.'}
$taskTrials=@()
$taskQualification=@{Status='NotRun';ExitCode=$null;Directory='qualification';Error=''}
$taskRegression=@{Status='NotRun';Cases=@();Error=''}
function Save-InputReport {
    Write-VT7Json (Join-Path $taskOutput 'report.json') @{
        Kind='VT7PtyInputResults';SchemaVersion=2;RunId=$taskRunId;ComputerName=$env:COMPUTERNAME;Tier=$Tier
        RecordedUtc=[DateTime]::UtcNow.ToString('o');OS=@{Caption=$taskOs.Caption;Version=$taskOs.Version;ServicePackMajorVersion=$taskOs.ServicePackMajorVersion;ProductType=$taskOs.ProductType}
        Machine=@{Manufacturer=$taskComputer.Manufacturer;Model=$taskComputer.Model};Updates=$taskUpdates
        PowerShellVersion=$PSVersionTable.PSVersion.ToString();CLRVersion=$PSVersionTable.CLRVersion.ToString();PowerShell7=$taskPs7
        ManifestSha256=Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json');Harness=$taskManifest.Harness;Runtime=$taskManifest.Runtime
        PreflightErrors=$taskPreflight;ExpectedTrials=7;Trials=$taskTrials
        Reference=$taskManifest.Reference;Qualification=$taskQualification;Regression=$taskRegression
        CtrlBreakTransport='Unsupported';NativeBreakIsReferenceOnly=$true
        KnownDevelopmentFailures=$taskManifest.KnownDevelopmentFailures
        PendingCoverage=$taskManifest.PendingCoverage;Step12Accepted=$false;MilestoneAccepted=$false
    }
}
Save-InputReport
Write-Host 'I test the rebuilt decoder runtime, with API 1.1 and protocol 2.'
Write-Host 'I first retain one unchanged rc1 input control, then run three pairs of H4 guards and 43 input cases.'
Write-Host 'Keep Windows unlocked. Physical keyboard, lock/unlock and soak repeats are not requested.'
Write-Host 'The rc1 reference retains its three original failures. Candidate failures remain actual failures.'
foreach($taskOrder in 1..7){
    $taskRole=if($taskOrder -eq 1){'Reference'}elseif($taskOrder%2 -eq 0){'Guard'}else{'Input'}
    $taskName=$taskOrder.ToString('00')+'-'+$taskRole.ToLowerInvariant()
    $taskTrial=Join-Path $taskOutput $taskName
    [void][IO.Directory]::CreateDirectory($taskTrial)
    $taskRecord=@{Order=$taskOrder;Role=$taskRole;Directory=$taskName;Outcome='NotRun';CollectionStatus='NotRun';ExitCode=$null;TimedOut=$false;Error=''}
    try{
        if($taskPreflight.Count){throw 'Prerequisite checks failed; keep this report.'}
        $taskBin=Join-Path $PackageRoot $(if($taskRole -eq 'Reference'){'reference\bin'}else{'runtime\bin'})
        $taskNative=Join-Path $taskTrial 'native'
        $taskArgs=$(if($taskRole -eq 'Guard'){'--run'}else{'--input-run'})+' "'+$taskNative+'"'
        $taskRuntimeSource=if($taskRole -eq 'Reference'){$taskManifest.Reference.RuntimeSourceCommit}else{$taskManifest.Runtime.SourceCommit}
        $taskEnv=@{VT7PTY_DEBUG='trace';VT7PTY_H4_RUNTIME_SOURCE=$taskRuntimeSource;VT7PTY_H3_PWSH=$taskPs7.Path;VT7PTY_H4_LEGACY=$(if($taskPs7.Required){''}else{'1'})}
        Write-Host ('Starting '+$taskName)
        $taskNativeRun=Invoke-InputProcess (Join-Path $taskBin 'RuntimeCancellationTest.exe') $taskArgs $taskBin 900000 $taskEnv
        foreach($taskField in $taskNativeRun.Keys){$taskRecord[$taskField]=$taskNativeRun[$taskField]}
        [IO.File]::WriteAllText((Join-Path $taskTrial 'stdout.txt'),$taskRecord.StandardOutput,$script:vt7Utf8)
        [IO.File]::WriteAllText((Join-Path $taskTrial 'stderr.txt'),$taskRecord.StandardError,$script:vt7Utf8)
        if($taskRecord.TimedOut){throw 'Controller deadline; keep all partial files.'}
        $taskProbe=Read-VT7Json (Join-Path $taskNative 'probe.json')
        $taskCases=(Read-VT7Json (Join-Path $taskNative 'cases.json')).Cases
        $taskExpected=if($taskRole -eq 'Guard'){$taskManifest.Guard.CasesByTier[$Tier]}elseif($taskRole -eq 'Reference'){$taskManifest.Reference.Cases}else{$taskManifest.Input.Cases}
        $taskKind=if($taskRole -eq 'Guard'){'H4RuntimeProbe'}else{'InputFidelityProbe'}
        $taskSource=if($taskRole -eq 'Reference'){$taskManifest.Reference.HarnessSourceCommit}else{$taskManifest.Harness.SourceCommit}
        if($taskProbe.Kind -cne $taskKind -or $taskProbe.SourceCommit -cne $taskSource -or
            $taskProbe.Total -ne $taskExpected -or @($taskCases).Count -ne $taskExpected){throw 'Wrong native source or case inventory.'}
        if($taskRole -ne 'Guard' -and ($taskProbe.RuntimeSourceCommit -cne $taskRuntimeSource -or -not $taskProbe.FullInventory)){throw 'Wrong input runtime or partial selection.'}
        if(($taskProbe.Status -ceq 'Pass') -ne ($taskRecord.ExitCode -eq 0)){throw 'Native outcome and exit code disagree.'}
        $taskRecord.Outcome=$taskProbe.Status;$taskRecord.Passed=$taskProbe.Passed;$taskRecord.Total=$taskProbe.Total
        $taskRecord.CollectionStatus='Completed'
    }catch{$taskRecord.Outcome='Incomplete';$taskRecord.CollectionStatus='Partial';$taskRecord.Error=$_.Exception.Message}
    $taskTrials+=@($taskRecord);Save-InputReport
    Write-Host ($taskName+': '+$taskRecord.Outcome+' '+$taskRecord.Error)
}
# These phases run on the candidate runtime, after the collectors have closed.
if(-not $taskPreflight.Count){
    try{
        Write-Host 'Starting the seven focused H4 lifetime and fault guards.'
        $taskBin=Join-Path $PackageRoot 'runtime\bin'
        $taskNative=Join-Path $taskOutput 'qualification'
        $taskRun=Invoke-InputProcess (Join-Path $taskBin 'RuntimeCancellationTest.exe') ('--qualify "'+$taskNative+'"') $taskBin 180000 @{
            VT7PTY_DEBUG='trace';VT7PTY_H4_RUNTIME_SOURCE=$taskManifest.Runtime.SourceCommit
        }
        $taskQualification.ExitCode=$taskRun.ExitCode
        [IO.File]::WriteAllText((Join-Path $taskOutput 'qualification-stdout.txt'),$taskRun.StandardOutput,$script:vt7Utf8)
        [IO.File]::WriteAllText((Join-Path $taskOutput 'qualification-stderr.txt'),$taskRun.StandardError,$script:vt7Utf8)
        $taskProbe=Read-VT7Json (Join-Path $taskNative 'probe.json')
        if($taskRun.TimedOut -or $taskRun.ExitCode -ne 0 -or $taskProbe.Status -cne 'Pass' -or
            $taskProbe.SourceCommit -cne $taskManifest.Runtime.SourceCommit -or $taskProbe.Total -ne 7){throw 'Focused qualification failed.'}
        $taskQualification.Status='Pass'
    }catch{$taskQualification.Status='Fail';$taskQualification.Error=$_.Exception.Message}
    Save-InputReport
    try{
        Write-Host 'Starting 14 focused unit, protocol and flagless session regressions.'
        $taskRecipes=(Read-VT7Json (Join-Path $PackageRoot 'tools\input\regression-cases.json')).Cases
        $taskBin=Join-Path $PackageRoot 'runtime\bin'
        foreach($taskRecipe in $taskRecipes){
            $taskCase=@{Name=$taskRecipe.Name;Executable=$taskRecipe.Executable;Arguments=$taskRecipe.Arguments;Status='Fail';Error=''}
            try{
                $taskRun=Invoke-InputProcess (Join-Path $taskBin $taskRecipe.Executable) $taskRecipe.Arguments $taskBin $taskRecipe.TimeoutMilliseconds @{VT7PTY_DEBUG=''}
                foreach($taskField in $taskRun.Keys){$taskCase[$taskField]=$taskRun[$taskField]}
                if($taskRun.TimedOut -or $taskRun.ExitCode -ne 0 -or
                    -not $taskRun.StandardOutput.Contains($taskRecipe.ExpectedOutput)){throw 'Regression failed or did not produce its expected completion.'}
                $taskCase.Status='Pass'
            }catch{$taskCase.Error=$_.Exception.Message}
            $taskRegression.Cases+=@($taskCase);Save-InputReport
            Write-Host ($taskCase.Name+': '+$taskCase.Status)
        }
        $taskRegression.Status=if(@($taskRegression.Cases | Where-Object {$_.Status -ne 'Pass'}).Count){'Fail'}else{'Pass'}
    }catch{$taskRegression.Status='Fail';$taskRegression.Error=$_.Exception.Message}
}
Save-InputReport
$taskFiles=@([IO.Directory]::GetFiles($taskOutput,'*',[IO.SearchOption]::AllDirectories) | Sort-Object | ForEach-Object {
    @{Path=$_.Substring($taskOutput.Length+1).Replace('\','/');Bytes=(Get-Item -LiteralPath $_).Length;Sha256=(Get-VT7Sha256 $_)}
})
Write-VT7Json (Join-Path $taskOutput 'evidence-files.json') @{SchemaVersion=1;Files=$taskFiles}
Write-Host ('Keep the complete results: '+$taskOutput)
if(@($taskTrials | Where-Object {$_.CollectionStatus -ne 'Completed'}).Count){exit 2}
$taskReference=@($taskTrials | Where-Object {$_.Role -eq 'Reference'})[0]
$taskReferenceCases=(Read-VT7Json (Join-Path $taskOutput '01-reference\native\cases.json')).Cases
$taskReferenceFailures=@($taskReferenceCases | Where-Object {-not $_.Passed} | ForEach-Object {$_.Case})
if($taskReference.Outcome -cne 'Fail' -or $taskReference.ExitCode -ne 1 -or
    ($taskReferenceFailures -join '|') -cne ($taskManifest.Reference.KnownFailures -join '|')){exit 1}
if(@($taskTrials | Where-Object {$_.Role -ne 'Reference' -and $_.Outcome -ne 'Pass'}).Count -or
    $taskQualification.Status -ne 'Pass' -or $taskRegression.Status -ne 'Pass'){exit 1}
exit 0
