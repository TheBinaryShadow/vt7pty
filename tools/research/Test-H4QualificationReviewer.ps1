[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$QualificationDirectory,
    [Parameter(Mandatory=$true)][string]$SoakDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateRange(1,120)][int]$MinimumSoakMinutes=120
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\artifacts')).TrimEnd('\')
if(-not $OutputDirectory.StartsWith($artifactRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $OutputDirectory)){throw 'Use a new directory under repository artifacts.'}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$reviewer=Join-Path $PSScriptRoot 'Test-H4QualificationEvidence.ps1'
$qualification=Join-Path $OutputDirectory 'qualification'
$soak=Join-Path $OutputDirectory 'soak'
Copy-Item -LiteralPath $QualificationDirectory -Destination $qualification -Recurse
Copy-Item -LiteralPath $SoakDirectory -Destination $soak -Recurse
$null=& $reviewer -Directory $qualification
$null=& $reviewer -Directory $soak -MinimumSoakMinutes $MinimumSoakMinutes
$controls=@(
    @{Name='case-missing';Root=$qualification;File='cases.json';Expected='qualification case inventory';Edit={param($v)$v.Cases=@($v.Cases | Select-Object -Skip 1)}},
    @{Name='inactive-wrong-cause';Root=$qualification;File='inactive-desktop\denial.json';Expected='specific inactive desktop';Edit={param($v)$v.Message='generic failure'}},
    @{Name='resource-leak';Root=$qualification;File='concurrent-resources\resources.json';Expected='caller resource budgets';Edit={param($v)$v.After.Handles=$v.Before.Handles+9}},
    @{Name='job-membership';Root=$qualification;File='job-silent-breakaway\caller.json';Expected='native enclosing-job policy';Edit={param($v)$v.AgentInJob=$true}},
    @{Name='early-c-dispatch';Root=$qualification;File='timeout-before-dispatch\fault.json';Expected='expired early request';Edit={param($v)$v.CMessageCount=1}},
    @{Name='late-timeout-decision';Root=$qualification;File='timeout-during-callback\fault.json';Expected='production unknown';Edit={param($v)$v.UnavailableObservedTick=$v.ReleasedTick+1}},
    @{Name='keyboard-corruption';Root=$qualification;File='timeout-during-callback\fault.json';Expected='all keyboard bytes restored';Edit={param($v)$v.After=('ff'+$v.After.Substring(2))}},
    @{Name='agent-not-dead';Root=$qualification;File='agent-death-during-callback\fault.json';Expected='agent death while callback active';Edit={param($v)$v.AgentExitedBeforeRelease=$false}},
    @{Name='short-duration';Root=$soak;File='probe.json';Expected='full requested opt-in duration';Edit={param($v)$v.ElapsedMilliseconds=1}},
    @{Name='cold-baseline-substitution';Root=$soak;File='resources-baseline.json';Expected='recorded cold, warm';Edit={param($v)$v.Handles++}},
    @{Name='trace-gap';Root=$soak;File='cycle-2\cancel.json';Expected='no lost fixture trace window';Edit={param($v)$v.Trace.OldestSequence=100000}},
    @{Name='neighbor-interrupted';Root=$soak;File='cycle-1\neighbor.json';Expected='long-lived pending neighbor';Edit={param($v)$v.ControlCount=1}}
)
$results=@()
foreach($control in $controls){
    $path=Join-Path $control.Root $control.File;$bytes=[IO.File]::ReadAllBytes($path)
    try{
        $v=[Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
        & $control.Edit $v
        [IO.File]::WriteAllText($path,($v | ConvertTo-Json -Depth 32),[Text.UTF8Encoding]::new($false))
        $reason=''
        try{$null=& $reviewer -Directory $control.Root -MinimumSoakMinutes $MinimumSoakMinutes}catch{$reason=$_.Exception.Message}
        if(-not $reason.Contains($control.Expected)){throw ('Missed '+$control.Name+': '+$reason)}
        $results+=@([ordered]@{Case=$control.Name;Rejected=$true;Reason=$reason})
    }finally{[IO.File]::WriteAllBytes($path,$bytes)}
}
$null=& $reviewer -Directory $qualification
$null=& $reviewer -Directory $soak -MinimumSoakMinutes $MinimumSoakMinutes
$result=[ordered]@{Kind='H4QualificationReviewerControls';Status='Pass';Rejected=$results.Count;Controls=$results;OriginalEvidenceUnmodified=$true;MinimumSoakMinutes=$MinimumSoakMinutes}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'review.json'),($result | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
$result
