[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$NativeDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$ExpectedRuntimeSource='54b0d23146dfde7528c9e4db1aa92d12e662f883',
    [string]$CaseDefinitionsPath=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\artifacts')).TrimEnd('\')
if(-not $OutputDirectory.StartsWith($artifactRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $OutputDirectory)){throw 'Use a fresh control directory under repository artifacts.'}
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$copy=Join-Path $OutputDirectory 'evidence'
Copy-Item -LiteralPath $NativeDirectory -Destination $copy -Recurse
$reviewer=Join-Path $PSScriptRoot 'Test-InputEvidence.ps1'
$reviewArgs=@{ExpectedRuntimeSource=$ExpectedRuntimeSource}
if($CaseDefinitionsPath){$reviewArgs.CaseDefinitionsPath=$CaseDefinitionsPath}
$baseline=& $reviewer -Directory $copy @reviewArgs
$controls=@(
    @{Name='wrong-runtime';File='probe.json';Expected='native source';Edit={param($v)$v.RuntimeSourceCommit=('0'*40)}},
    @{Name='missing-case';File='cases.json';Expected='case inventory';Edit={param($v)$v.Cases=@($v.Cases | Select-Object -Skip 1)}},
    @{Name='changed-golden';File='raw-text-controls\recipe.json';Expected='golden recipe';Edit={param($v)$v.ExpectedText='wrong'}},
    @{Name='lost-text';File='raw-text-controls\input-observed.json';Expected='recomputed outcome';Edit={param($v)($v.Records | Where-Object {$_.Down -and $_.Character -eq 97} | Select-Object -First 1).Character=120}},
    @{Name='invalid-repeat';File='raw-key-repeat\input-observed.json';Expected='complete key record';Edit={param($v)$v.Records[0].Repeat=0}},
    @{Name='missing-key-up';File='raw-modifiers\input-observed.json';Expected='recomputed outcome';Edit={param($v)$v.Records=@($v.Records | Where-Object {-not ($_.Type -eq 1 -and -not $_.Down -and $_.VirtualKey -eq 17)})}},
    @{Name='wrong-mouse-wheel';File='mouse-sgr\input-observed.json';Expected='recomputed outcome';Edit={param($v)($v.Records | Where-Object {$_.Type -eq 2 -and $_.Flags -eq 4} | Select-Object -First 1).Buttons=0}},
    @{Name='borrowed-break-group';File='native-break-processed\input-observed.json';Expected='recomputed outcome';Edit={param($v)$v.Reference.ProcessGroupId++}},
    @{Name='reference-child-not-exited';File='native-break-unprocessed\input-observed.json';Expected='recomputed outcome';Edit={param($v)$v.Reference.ChildExited=0}},
    @{Name='false-break-support';File='probe.json';Expected='transport limits';Edit={param($v)$v.CtrlBreakTransport='Supported'}},
    @{Name='false-acceptance';File='probe.json';Expected='transport limits';Edit={param($v)$v.Step12Accepted=$true}},
    @{Name='unclean-host';File='raw-navigation\lifetime.json';Expected='owned cleanup';Edit={param($v)$v.HostExited=$false}},
    @{Name='fixture-read-canceled';File='cooked-unicode\capture-stopped.json';Expected='fixture read cancellation';Edit={param($v)$v.StopCanceledRead=1}},
    @{Name='different-input-writes';File='utf8-bytewise\writes.json';Expected='recorded input writes';Edit={param($v)$v.ChunkHex[0]='78'}},
    @{Name='failed-case-laundered';File='cases.json';Expected='recomputed outcome';Edit={param($v)($v.Cases | Where-Object {$_.Case -eq 'utf8-timeout-fragment'}).Passed=-not ($v.Cases | Where-Object {$_.Case -eq 'utf8-timeout-fragment'}).Passed}}
)
if($baseline.Total -eq 43){
    $controls+=@(
        @{Name='missing-live-reply';File='utf8-live-replies\terminal-replies.json';Expected='terminal reply writes';Edit={param($v)$v.ReplyHex=@()}},
        @{Name='changed-reply-write';File='utf8-fragmented-replies\terminal-replies.json';Expected='terminal reply writes';Edit={param($v)$v.WriteHex[0]='78'}},
        @{Name='changed-idle';File='escape-alone-live\writes.json';Expected='idle interval';Edit={param($v)$v.IdleMilliseconds=0}},
        @{Name='changed-reply-chunk';File='escape-fragmented-replies\recipe.json';Expected='golden recipe';Edit={param($v)$v.ReplyChunkBytes=0}},
        @{Name='lost-recovery-byte';File='utf8-malformed-recovery\input-observed.json';Expected='recomputed outcome';Edit={param($v)($v.Records | Where-Object {$_.Down -and $_.Character -eq 122} | Select-Object -First 1).Character=120}}
    )
}
$results=@()
foreach($control in $controls){
    $path=Join-Path $copy $control.File;$original=[IO.File]::ReadAllBytes($path)
    try{
        $value=[Text.Encoding]::UTF8.GetString($original) | ConvertFrom-Json
        & $control.Edit $value
        [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
        $reason=''
        try{$null=& $reviewer -Directory $copy @reviewArgs}catch{$reason=$_.Exception.Message}
        if(-not $reason.Contains($control.Expected)){throw ('Missed '+$control.Name+': '+$reason)}
        $results+=@(@{Case=$control.Name;Rejected=$true;Reason=$reason})
    }finally{[IO.File]::WriteAllBytes($path,$original)}
}
$path=Join-Path $copy 'runtime-trace.jsonl';$original=[IO.File]::ReadAllBytes($path)
try{
    $text=[Text.Encoding]::UTF8.GetString($original)
    $text=$text.Replace($ExpectedRuntimeSource,('0'*40))
    [IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($false))
    $reason=''
    try{$null=& $reviewer -Directory $copy @reviewArgs}catch{$reason=$_.Exception.Message}
    if(-not $reason.Contains('trace runtime/harness/API/protocol')){throw ('Missed trace identity: '+$reason)}
    $results+=@(@{Case='wrong-trace-runtime';Rejected=$true;Reason=$reason})
}finally{[IO.File]::WriteAllBytes($path,$original)}
$again=& $reviewer -Directory $copy @reviewArgs
if($again.Passed -ne $baseline.Passed -or ($again.FailedCases -join '|') -cne ($baseline.FailedCases -join '|')){throw 'Control restoration changed the evidence.'}
$result=[ordered]@{Kind='InputReviewerControls';Status='Pass';Rejected=$results.Count;BaselineOutcome=$baseline.Outcome;BaselinePassed=$baseline.Passed;Total=$baseline.Total;Controls=$results;Step12Accepted=$false}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'review.json'),($result | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
$result
