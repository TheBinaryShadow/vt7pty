[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9]+(?:[.-][a-z0-9]+)*$')][string]$Suffix='rc1',
    [string]$ReferenceArchive='',
    [string]$RegressionArchive='',
    [switch]$NoBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$root=$PSScriptRoot
$source=(& git -C $root rev-parse --verify HEAD).Trim()
$changes=@(& git -C $root status --porcelain=v1)
if($LASTEXITCODE -ne 0 -or $changes.Count){throw 'H4 packaging requires clean committed source.'}
$version=[IO.File]::ReadAllText((Join-Path $root 'VERSION.txt')).Trim()
if($version -ne '0.5.1-dev'){throw 'This candidate requires the H4 development identity.'}
if(-not $ReferenceArchive){$ReferenceArchive=Join-Path $root 'artifacts\fidelity\packages\VT7Pty-host-guards-0.5.0-rc1.zip'}
$referenceHash='620cd3e489fdf53f08184207f947414bd50f89a1ff617507c76c7ac20a099ca7'
$referenceManifestHash='d7f4c97e6ca6952d837b2cca566dd3a5479a44829f2a3ef19f63a11df5b05892'
if((Get-FileHash -LiteralPath $ReferenceArchive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $referenceHash){throw 'Use the exact accepted H3 guard rc1 ZIP.'}
if(-not $NoBuild) {
    & (Join-Path $root 'Package-VT7Pty.ps1') -PackageSuffix ('h4-'+$Suffix)
}
if(-not $RegressionArchive){$RegressionArchive=Join-Path $root ('artifacts\packages\VT7Pty-'+$version+'-win7-x64-release-h4-'+$Suffix+'-tests.zip')}
$verifyPath=Join-Path $root 'artifacts\verification\x64\Release\verification.json'
$verification=Get-Content -LiteralPath $verifyPath -Raw | ConvertFrom-Json
if(-not $verification.SourceTreeClean -or $verification.SourceCommit -cne $source -or
    $verification.SourceVersion -cne $version -or $verification.ApiVersion -cne '1.1' -or $verification.ProtocolVersion -ne 2 -or
    @($verification.Tests | Where-Object {$_.ExitCode -ne 0 -or $_.TimedOut}).Count){throw 'Release verification does not match clean H4 source.'}
$parent=Join-Path $root 'artifacts\fidelity\packages'
[IO.Directory]::CreateDirectory($parent) | Out-Null
$stem='VT7Pty-h4-runtime-'+$version+'-'+$Suffix
$stage=Join-Path $parent $stem
$zip=$stage+'.zip'
foreach($path in @($stage,$zip,($zip+'.sha256'))){if(Test-Path -LiteralPath $path){throw 'Candidate exists; choose a fresh suffix.'}}
[IO.Directory]::CreateDirectory($stage) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($ReferenceArchive),(Join-Path $stage 'reference'))
$referenceRoot=Join-Path $stage 'reference'
if((Get-FileHash -LiteralPath (Join-Path $referenceRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -cne $referenceManifestHash){throw 'Frozen H3 manifest changed.'}
$reference=Get-Content -LiteralPath (Join-Path $referenceRoot 'manifest.json') -Raw | ConvertFrom-Json
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($RegressionArchive),(Join-Path $stage 'regression'))
$regressionRoot=Join-Path $stage 'regression'
$regression=Get-Content -LiteralPath (Join-Path $regressionRoot 'manifest.json') -Raw | ConvertFrom-Json
if($regression.SourceCommit -cne $source -or $regression.Version -cne $version -or $regression.ApiVersion -cne '1.1' -or
    $regression.ProtocolVersion -ne 2 -or -not $regression.SourceTreeClean){throw 'Wrong matched regression archive.'}
foreach($pair in @(@($referenceRoot,$reference),@($regressionRoot,$regression))) {
    foreach($file in $pair[1].Files) {
        $path=[IO.Path]::GetFullPath((Join-Path $pair[0] $file.Path))
        if(-not $path.StartsWith($pair[0]+'\',[StringComparison]::OrdinalIgnoreCase) -or
            (Get-Item -LiteralPath $path).Length -ne $file.Bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $file.Sha256){throw ('Extracted dependency changed: '+$file.Path)}
    }
}
$bin=Join-Path $stage 'runtime\bin'
[IO.Directory]::CreateDirectory($bin) | Out-Null
foreach($name in @('VT7Pty.dll','VT7Pty-Agent.exe','VT7Pty-Host.dll','VT7Pty-DebugServer.exe','RuntimeCancellationTest.exe',
    'H4TestGate.dll','H4TestGate.pdb',
    'VT7Pty.pdb','VT7Pty-Agent.pdb','VT7Pty-Host.pdb','VT7Pty-DebugServer.pdb','RuntimeCancellationTest.pdb')) {
    $record=@($verification.Artifacts | Where-Object {$_.Name -ceq $name})
    $built=Join-Path $root ('artifacts\bin\x64\Release\'+$name)
    if($record.Count -ne 1 -or (Get-Item -LiteralPath $built).Length -ne $record[0].Bytes -or
        (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash.ToLowerInvariant() -cne $record[0].Sha256){throw ('Runtime artifact changed after verification: '+$name)}

    Copy-Item -LiteralPath $built -Destination (Join-Path $bin $name)
}
foreach($name in @('VT7Pty.dll','VT7Pty-Agent.exe','VT7Pty-Host.dll')) {
    if((Get-FileHash -LiteralPath (Join-Path $bin $name) -Algorithm SHA256).Hash -cne
        (Get-FileHash -LiteralPath (Join-Path $regressionRoot ('bin\'+$name)) -Algorithm SHA256).Hash){throw 'Runtime and regression artifacts differ.'}
}
$plan=[ordered]@{
    'README.md'='docs\H4_RUNTIME.md'
    'tools\research\Invoke-H4Runtime.ps1'='tools\research\Invoke-H4Runtime.ps1'
    'tools\research\H4Cases.ps1'='tools\research\H4Cases.ps1'
    'tools\research\Invoke-H4Manual.ps1'='tools\research\Invoke-H4Manual.ps1'
    'RUN-H4-MANUAL.cmd'='RUN-H4-MANUAL.cmd'
    'RUN-H4-ACCESS.cmd'='RUN-H4-ACCESS.cmd'
    'MANUAL-CHECKS.md'='docs\H4_MANUAL_CHECKS.md'
    'tools\platform\Windows7Compat.ps1'='tools\platform\Windows7Compat.ps1'
    'RUN-H4-NONESU.cmd'='RUN-H4-NONESU.cmd'
    'RUN-H4-ESU.cmd'='RUN-H4-ESU.cmd'
    'RUN-H4-LEGACY.cmd'='RUN-H4-LEGACY.cmd'
    'LICENSE.txt'='LICENSE';'CREDITS.md'='CREDITS.md';'UPSTREAM.md'='UPSTREAM.md'
}
foreach($name in $plan.Keys) {
    $target=Join-Path $stage $name
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $plan[$name]) -Destination $target
}
$readmePath=Join-Path $stage 'README.md'
$text=[IO.File]::ReadAllText($readmePath)
$text=[regex]::Replace($text,'\]\(([^)]+)\)',{
    param($match)
    $link=$match.Groups[1].Value
    if($link -match '^[a-zA-Z]+:' -or $link.StartsWith('#')){return $match.Value}
    $absolute=[IO.Path]::GetFullPath((Join-Path (Join-Path $root 'docs') $link.Split('#')[0]))
    if(-not $absolute.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Readme link leaves repository.'}
    $fragment=if($link.Contains('#')){'#'+$link.Split('#',2)[1]}else{''}
    return '](https://github.com/TheBinaryShadow/vt7pty/blob/'+$source+'/'+$absolute.Substring($root.Length+1).Replace('\','/')+$fragment+')'
})
[IO.File]::WriteAllText($readmePath,$text,[Text.UTF8Encoding]::new($false))
$files=@(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');Bytes=$_.Length;Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$manifest=[ordered]@{
    SchemaVersion=1;Kind='VT7PtyH4RuntimeCandidate';H4Accepted=$false
    Runtime=[ordered]@{PackageVersion=$version;ApiVersion='1.1';ProtocolVersion=2;SourceCommit=$source;SourceTreeClean=$true;Architecture='x64';Configuration='Release'
        HostCancellationOptIn=$true;OrdinaryApplicationDesktop=$true;CloseApi='TwoPhase';CasesByTier=[ordered]@{DevelopmentHost=41;NonESU=41;ESU=41;Legacy=33}
        VerificationSha256=(Get-FileHash -LiteralPath $verifyPath -Algorithm SHA256).Hash.ToLowerInvariant()}
    Reference=[ordered]@{ArchiveSha256=$referenceHash;ManifestSha256=$referenceManifestHash;GuardSourceCommit=$reference.Experiment.SourceCommit;ControlSourceCommit=$reference.Control.SourceCommit;Unchanged=$true}
    Regression=[ordered]@{ArchiveSha256=(Get-FileHash -LiteralPath $RegressionArchive -Algorithm SHA256).Hash.ToLowerInvariant();ManifestSha256=(Get-FileHash -LiteralPath (Join-Path $regressionRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant();SourceCommit=$source;SoakMinutes=120;UsesOptInHostCancellation=$false}
    Qualification=[ordered]@{Revision=1;Cases=7;ExternalTestGate='H4TestGate.dll';ProductFaultSwitches=$false}
    OptInSoak=[ordered]@{Minutes=120;TraceByteLimit=268435456;LongLivedActiveAndNeighbor=$true}
    Manual=[ordered]@{Required=@('US','HR','LockUnlock','ElevatedCaller');Launcher='RUN-H4-MANUAL.cmd';SyntheticPhysicalInput=$false}
    RemainingQualification=@('Windows7Rc3Results','USAndCroatianKeyboardWorkflows','LockUnlock','ElevatedCaller','BroaderApplicationAndDescendantPolicy')
    Files=$files
}
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'),($manifest | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($zip+'.sha256'),$hash+'  '+[IO.Path]::GetFileName($zip)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host ('H4 test candidate: '+$zip)
Write-Host ('SHA-256: '+$hash)
