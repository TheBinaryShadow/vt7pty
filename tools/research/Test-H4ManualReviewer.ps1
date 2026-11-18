[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SmokeDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$SmokeDirectory=[IO.Path]::GetFullPath($SmokeDirectory)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$artifactRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\artifacts')).TrimEnd('\')
if(-not $OutputDirectory.StartsWith($artifactRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $OutputDirectory)){throw 'Use a fresh directory under repository artifacts.'}
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$reviewer=Join-Path $PSScriptRoot 'Test-H4QualificationEvidence.ps1'
$reviews=@();$results=@()
foreach($profile in @('US','HR','Session')){
    $native=Join-Path $SmokeDirectory ($profile+'\native')
    $reviews+=@(& $reviewer -Directory $native -AllowSyntheticSmoke)
    $reason=''
    try{$null=& $reviewer -Directory $native}catch{$reason=$_.Exception.Message}
    if(-not $reason.Contains('synthetic smoke is not manual acceptance')){throw 'Synthetic input accepted as physical.'}
    $results+=@(@{Case=('synthetic-'+$profile);Rejected=$true;Reason=$reason})
}
$controls=@(
    @{Name='wrong-runtime';Profile='US';File='probe.json';Expected='qualified runtime identity';Edit={param($v)$v.RuntimeSourceCommit=('0'*40)}},
    @{Name='incomplete-keys';Profile='US';File='cancel-keys.json';Expected='complete input';Edit={param($v)$v.Complete=$false}},
    @{Name='mixed-layout';Profile='HR';File='text-keys.json';Expected='confirmed layout';Edit={param($v)$v.AllTextUsedRequiredLayout=$false}},
    @{Name='unconfirmed-layout';Profile='US';File='layout-ready.json';Expected='confirmed layout';Edit={param($v)$v.Confirmed=$false}},
    @{Name='wrong-backend-line';Profile='HR';File='text-read.json';Expected='exact subsequent read';Edit={param($v)$v.Text='wrong'}},
    @{Name='wrong-key-layout-id';Profile='US';File='next-keys.json';Expected='complete input';Edit={param($v)($v.Keys | Where-Object {$_.Down -and $_.Character -ne 0} | Select-Object -First 1).LayoutId='0000041a'}},
    @{Name='wrong-session-transition';Profile='Session';File='probe.json';Expected='explicit synthetic smoke';Edit={param($v)$v.CallerThreadDesktopChangedBySmoke=$false}},
    @{Name='missing-unlock';Profile='Session';File='transitions.json';Expected='observed lock/unlock';Edit={param($v)$v.States=@($v.States | Select-Object -First 2)}}
)
foreach($c in $controls){
    $copy=Join-Path $OutputDirectory $c.Name
    Copy-Item -LiteralPath (Join-Path $SmokeDirectory ($c.Profile+'\native')) -Destination $copy -Recurse
    $path=Join-Path $copy $c.File
    $value=[IO.File]::ReadAllText($path) | ConvertFrom-Json
    & $c.Edit $value
    [IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
    $reason=''
    try{$null=& $reviewer -Directory $copy -ExpectedRuntimeSource '54b0d23146dfde7528c9e4db1aa92d12e662f883' -AllowSyntheticSmoke}catch{$reason=$_.Exception.Message}
    if(-not $reason.Contains($c.Expected)){throw ('Missed '+$c.Name+': '+$reason)}
    $results+=@(@{Case=$c.Name;Rejected=$true;Reason=$reason})
}
# Corrupt a runtime trace separately so the declared source cannot hide a mixed binary.
$copy=Join-Path $OutputDirectory 'wrong-runtime-trace'
Copy-Item -LiteralPath (Join-Path $SmokeDirectory 'US\native') -Destination $copy -Recurse
$path=Join-Path $copy 'runtime-trace.jsonl'
$text=[IO.File]::ReadAllText($path).Replace('54b0d23146dfde7528c9e4db1aa92d12e662f883',('0'*40))
[IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($false))
$reason=''
try{$null=& $reviewer -Directory $copy -AllowSyntheticSmoke}catch{$reason=$_.Exception.Message}
if(-not $reason.Contains('trace source/API/protocol')){throw ('Missed wrong trace source: '+$reason)}
$results+=@(@{Case='wrong-runtime-trace';Rejected=$true;Reason=$reason})
$result=[ordered]@{Kind='H4ManualReviewerControls';Status='Pass';SyntheticAcceptance=$false;Reviews=$reviews;Rejected=$results.Count;Controls=$results}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'review.json'),($result | ConvertTo-Json -Depth 16),[Text.UTF8Encoding]::new($false))
$result
