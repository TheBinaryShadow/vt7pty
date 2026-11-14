[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$NativeDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$NativeDirectory=[IO.Path]::GetFullPath($NativeDirectory).TrimEnd('\')
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\artifacts')).TrimEnd('\')
if(-not $OutputDirectory.StartsWith($artifactRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $OutputDirectory)){throw 'Use a new reviewer-test directory under repository artifacts.'}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$copy=Join-Path $OutputDirectory 'mutated-copy'
Copy-Item -LiteralPath $NativeDirectory -Destination $copy -Recurse
$reviewer=Join-Path $PSScriptRoot 'Test-H4RuntimeEvidence.ps1'
$null=& $reviewer -Directory $copy
$cases=@(
    @{Name='missing-case';File='cases.json';Expected='exact case inventory';Edit={param($v)$v.Cases=@($v.Cases | Select-Object -Skip 1)}},
    @{Name='retained-old-line';File='cooked-partial\cancel-1.json';Expected='canceled-read evidence';Edit={param($v)$v.Text='old'}},
    @{Name='duplicate-control';File='cooked-partial\cancel-1.json';Expected='one handler exit per event';Edit={param($v)$v.ControlCount=2}},
    @{Name='wrong-next-line';File='cooked-partial\next-read.json';Expected='exact subsequent read';Edit={param($v)$v.Text='BAD'}},
    @{Name='raw-duplicate-control';File='raw-input\raw-read.json';Expected='raw modifier/C/z records';Edit={param($v)$v.ControlCount=1}},
    @{Name='retired-allocation';File='close-rpc\close.json';Expected='two-phase close contract';Edit={param($v)$v.AllocationRetained=$false}},
    @{Name='wrong-child-status';File='child-cmd-cooked\child.json';Expected='active child and parent';Edit={param($v)$v.ChildExitCode=38}},
    @{Name='handle-leak';File='recreate-resources\resources.json';Expected='caller handle budget';Edit={param($v)$v.HandlesAfter=$v.HandlesBefore+3}}
)
$results=@()
foreach($case in $cases) {
    $path=Join-Path $copy $case.File;$bytes=[IO.File]::ReadAllBytes($path)
    try {
        $value=[Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
        & $case.Edit $value
        [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 32),[Text.UTF8Encoding]::new($false))
        $rejected=$false;$reason=''
        try{$null=& $reviewer -Directory $copy}catch{$reason=$_.Exception.Message;$rejected=$reason.Contains($case.Expected)}
        if(-not $rejected){throw ('Reviewer missed '+$case.Name+'; observed '+$reason)}
        $results+=@([ordered]@{Case=$case.Name;Rejected=$true;Reason=$reason})
    } finally {[IO.File]::WriteAllBytes($path,$bytes)}
}
$path=Join-Path $copy 'runtime-trace.jsonl';$bytes=[IO.File]::ReadAllBytes($path)
try {
    $text=[Text.Encoding]::UTF8.GetString($bytes)
    $changed=[regex]::Replace($text,'(host-cancel after sequence=\d+ keys=)[0-9a-f]{2}', '1ff',1)
    if($changed -ceq $text){throw 'Missing after-keyboard record.'}
    [IO.File]::WriteAllText($path,$changed,[Text.UTF8Encoding]::new($false))
    $rejected=$false;$reason=''
    try{$null=& $reviewer -Directory $copy}catch{$reason=$_.Exception.Message;$rejected=$reason.Contains('all 256 keyboard bytes restored')}
    if(-not $rejected){throw ('Reviewer missed keyboard corruption: '+$reason)}
    $results+=@([ordered]@{Case='keyboard-corruption';Rejected=$true;Reason=$reason})
} finally {[IO.File]::WriteAllBytes($path,$bytes)}
$null=& $reviewer -Directory $copy
$result=[ordered]@{Kind='H4ReviewerFaultControls';Status='Pass';Rejected=$results.Count;Controls=$results;OriginalEvidenceUnmodified=$true}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'review.json'),($result | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
$result
