[CmdletBinding()]
param(
    [ValidateSet('All','US','HR','Session','Elevated')][string]$Check='All',
    [string]$PackageRoot=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$taskScript=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $PackageRoot){$PackageRoot=Join-Path $taskScript '..\..'}
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
. (Join-Path $PackageRoot 'tools\platform\Windows7Compat.ps1')
$taskManifest=Read-VT7Json (Join-Path $PackageRoot 'manifest.json')
if($taskManifest.Kind -ne 'VT7PtyH4RuntimeCandidate' -or $taskManifest.Qualification.Revision -ne 1){
    throw 'Use the H4 qualification candidate containing this launcher.'
}
foreach($taskFile in $taskManifest.Files){
    $taskPath=[IO.Path]::GetFullPath((Join-Path $PackageRoot $taskFile.Path))
    if(-not $taskPath.StartsWith($PackageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
        (Get-Item -LiteralPath $taskPath).Length -ne $taskFile.Bytes -or
        (Get-VT7Sha256 $taskPath) -cne $taskFile.Sha256){throw ('Package changed: '+$taskFile.Path)}
}
$taskRunId=$env:COMPUTERNAME.ToLowerInvariant()+'-h4-manual-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$taskOutput=Join-Path $PackageRoot ('results\'+$taskRunId)
if(Test-Path -LiteralPath $taskOutput){throw 'Use a new manual result directory.'}
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskSteps=@()
$taskChecks=@(if($Check -eq 'All'){'US';'HR';'Session'}else{$Check})
function Save-H4Manual {
    Write-VT7Json (Join-Path $taskOutput 'report.json') @{
        Kind='VT7PtyH4ManualResults';SchemaVersion=1;RunId=$taskRunId;ComputerName=$env:COMPUTERNAME
        RecordedUtc=[DateTime]::UtcNow.ToString('o');SourceCommit=$taskManifest.Runtime.SourceCommit
        ManifestSha256=(Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json'))
        RequestedChecks=$taskChecks;Steps=$taskSteps;PhysicalInputRequired=$true
        PowerShellVersion=$PSVersionTable.PSVersion.ToString();CLRVersion=$PSVersionTable.CLRVersion.ToString()
        H4Accepted=$false;ManualQualificationAccepted=$false
    }
}
Save-H4Manual
Write-Host 'Run this after the automated H4 collection has finished.'
if($Check -eq 'Elevated'){Write-Host 'This check requires an elevated classic console. It does not elevate itself.'}
else {Write-Host 'I will ask for physical keyboard input and a manual lock/unlock. I do not change Windows settings.'}
foreach($taskCheck in $taskChecks){
    $taskName=$taskCheck.ToLowerInvariant()
    $taskDirectory=Join-Path $taskOutput $taskName
    $taskEntry=@{Check=$taskCheck;Directory=$taskName;Status='NotRun';CollectionStatus='NotRun';ExitCode=$null;Error=''}
    try {
        $taskArguments=if($taskCheck -eq 'Session'){'--session-check'}elseif($taskCheck -eq 'Elevated'){'--access-check'}else{'--keyboard '+$taskCheck}
        $taskStart=New-Object Diagnostics.ProcessStartInfo
        $taskStart.FileName=Join-Path $PackageRoot 'runtime\bin\RuntimeCancellationTest.exe'
        $taskStart.Arguments=$taskArguments+' "'+$taskDirectory+'"'
        $taskStart.WorkingDirectory=Join-Path $PackageRoot 'runtime\bin'
        $taskStart.UseShellExecute=$false;$taskStart.CreateNoWindow=$false
        $taskProcess=New-Object Diagnostics.Process;$taskProcess.StartInfo=$taskStart
        try {
            if(-not $taskProcess.Start()){throw 'Cannot start the manual test.'}
            if(-not $taskProcess.WaitForExit(1500000)){
                $taskProcess.Kill();[void]$taskProcess.WaitForExit(10000)
                throw 'Manual test deadline. Keep the partial results.'
            }
            $taskEntry.ExitCode=$taskProcess.ExitCode
        }finally{$taskProcess.Close()}
        $taskProbePath=Join-Path $taskDirectory 'probe.json'
        if(Test-Path -LiteralPath $taskProbePath){
            $taskProbe=Read-VT7Json $taskProbePath
            if($taskProbe.SourceCommit -cne $taskManifest.Runtime.SourceCommit){throw 'Manual test build does not match the package.'}
            $taskEntry.Status=$taskProbe.Status;$taskEntry.CollectionStatus='Completed'
        }else{
            $taskEntry.Status='Fail';$taskEntry.CollectionStatus='Partial'
            $taskFailure=Join-Path $taskDirectory 'failure.json'
            if(Test-Path -LiteralPath $taskFailure){$taskEntry.Error=(Read-VT7Json $taskFailure).Error}
            else{$taskEntry.Error='The native check did not produce a completion record. Keep its partial files.'}
        }
        if(($taskEntry.Status -eq 'Pass') -ne ($taskEntry.ExitCode -eq 0)){throw 'Manual status and exit code disagree.'}
    }catch{$taskEntry.Status='Fail';$taskEntry.CollectionStatus='Partial';$taskEntry.Error=$_.Exception.Message}
    $taskSteps+=@($taskEntry);Save-H4Manual
    Write-Host ($taskCheck+': '+$taskEntry.Status+' '+$taskEntry.Error)
    if($taskEntry.Status -ne 'Pass'){break}
}
$taskFiles=@([IO.Directory]::GetFiles($taskOutput,'*',[IO.SearchOption]::AllDirectories) | Sort-Object | ForEach-Object {
    @{Path=$_.Substring($taskOutput.Length+1).Replace('\','/');Bytes=(Get-Item -LiteralPath $_).Length;Sha256=(Get-VT7Sha256 $_)}
})
Write-VT7Json (Join-Path $taskOutput 'evidence-files.json') @{SchemaVersion=1;Files=$taskFiles}
Write-Host ('Keep the complete manual results: '+$taskOutput)
if($Check -eq 'All'){Write-Host 'Then right-click RUN-H4-ACCESS.cmd and choose Run as administrator for the separate elevated-caller denial check.'}
if($taskSteps.Count -ne $taskChecks.Count -or @($taskSteps | Where-Object {$_.Status -ne 'Pass'}).Count){exit 1}
exit 0
