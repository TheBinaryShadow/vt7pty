[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9]+(?:[.-][a-z0-9]+)*$')][string]$Suffix='rc1',
    [string]$Rc4Archive=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$root=$PSScriptRoot
$source=(& git -C $root rev-parse --verify HEAD).Trim()
if($LASTEXITCODE -ne 0 -or @(& git -C $root status --porcelain=v1).Count){throw 'Manual packaging requires clean committed source.'}
$version=[IO.File]::ReadAllText((Join-Path $root 'VERSION.txt')).Trim()
if($version -cne '0.5.1-dev'){throw 'Wrong H4 development version.'}
$runtimeSource='54b0d23146dfde7528c9e4db1aa92d12e662f883'
$rc4Hash='910a4920b06872c4756e814d5e5851fe2c3ed9b4c802854b59e2aaa0966d16ce'
$rc4ManifestHash='43a767650d720b1f9dc1bf5d8a17420d0ead7991461df65d088dcab206956a98'
if(-not $Rc4Archive){$Rc4Archive=Join-Path $root 'artifacts\fidelity\packages\VT7Pty-h4-runtime-0.5.1-dev-rc4.zip'}
function Hash-H4([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
if((Hash-H4 $Rc4Archive) -cne $rc4Hash){throw 'Use the exact qualified rc4 archive.'}
$parent=Join-Path $root 'artifacts\fidelity\packages'
$stage=Join-Path $parent ('VT7Pty-h4-manual-'+$version+'-'+$Suffix)
$zip=$stage+'.zip'
foreach($path in @($stage,$zip,($zip+'.sha256'))){if(Test-Path -LiteralPath $path){throw 'Candidate exists; choose a fresh suffix.'}}
[void][IO.Directory]::CreateDirectory($stage)
$bin=Join-Path $stage 'runtime\bin'
[void][IO.Directory]::CreateDirectory($bin)
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Rc4Archive))
try{
    $entry=$archive.GetEntry('manifest.json')
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $stage 'rc4-manifest.json'))
    if((Hash-H4 (Join-Path $stage 'rc4-manifest.json')) -cne $rc4ManifestHash){throw 'Wrong rc4 manifest.'}
    $original=[IO.File]::ReadAllText((Join-Path $stage 'rc4-manifest.json')) | ConvertFrom-Json
    if($original.Runtime.SourceCommit -cne $runtimeSource -or -not $original.Runtime.SourceTreeClean){throw 'Wrong qualified runtime source.'}
    $runtimeFiles=@()
    foreach($name in @('VT7Pty.dll','VT7Pty-Agent.exe','VT7Pty-Host.dll','VT7Pty-DebugServer.exe',
        'VT7Pty.pdb','VT7Pty-Agent.pdb','VT7Pty-Host.pdb','VT7Pty-DebugServer.pdb')){
        $relative='runtime/bin/'+$name
        $record=@($original.Files | Where-Object {$_.Path -ceq $relative})
        if($record.Count -ne 1){throw ('Missing qualified artifact '+$name)}
        $destination=Join-Path $bin $name
        [IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($relative),$destination)
        if((Get-Item -LiteralPath $destination).Length -ne $record[0].Bytes -or (Hash-H4 $destination) -cne $record[0].Sha256){throw ('Changed rc4 artifact '+$name)}
        $runtimeFiles+=@($record[0])
    }
}finally{$archive.Dispose()}
# Only the test executable is newly built. Product files above come from rc4.
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=(& $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.Component.MSBuild -property installationPath).Trim()
if(-not $vs){throw 'Visual Studio 2022 is required on the build machine.'}
$buildLog=Join-Path $stage 'harness-build.txt'
& (Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe') (Join-Path $root 'projects\RuntimeCancellationTest.vcxproj') /nologo /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal *> $buildLog
if($LASTEXITCODE -ne 0){throw ('Harness build failed: '+$buildLog)}
$identity=[IO.File]::ReadAllLines((Join-Path $root 'build\obj\RuntimeCancellationTest\x64\Release\generated\SourceIdentity.txt'))
if($identity[0] -cne $version -or $identity[1] -cne $source){throw 'Wrong generated harness source identity.'}
foreach($name in @('RuntimeCancellationTest.exe','RuntimeCancellationTest.pdb')){
    Copy-Item -LiteralPath (Join-Path $root ('artifacts\bin\x64\Release\'+$name)) -Destination (Join-Path $bin $name)
}
$oracle=@(& (Join-Path $bin 'RuntimeCancellationTest.exe') --manual-self-test)
if($LASTEXITCODE -ne 0 -or ($oracle -join '') -cne 'Manual runner oracle: 11 checks passed.'){throw 'Manual runner oracle failed.'}
[IO.File]::WriteAllLines((Join-Path $stage 'harness-self-test.txt'),[string[]]$oracle,[Text.UTF8Encoding]::new($false))
$plan=[ordered]@{
    'README.md'='docs\H4_MANUAL_CHECKS.md'
    'RUN-H4-MANUAL.cmd'='RUN-H4-MANUAL.cmd'
    'tools\research\Invoke-H4Manual.ps1'='tools\research\Invoke-H4Manual.ps1'
    'tools\platform\Windows7Compat.ps1'='tools\platform\Windows7Compat.ps1'
    'LICENSE.txt'='LICENSE';'CREDITS.md'='CREDITS.md';'UPSTREAM.md'='UPSTREAM.md'
}
foreach($name in $plan.Keys){
    $path=Join-Path $stage $name
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    Copy-Item -LiteralPath (Join-Path $root $plan[$name]) -Destination $path
}
# Pin repository links in the portable README to this harness commit.
$readme=Join-Path $stage 'README.md'
$text=[IO.File]::ReadAllText($readme)
$text=[regex]::Replace($text,'\]\(([^)]+)\)',{
    param($m)
    $link=$m.Groups[1].Value
    if($link -match '^[a-zA-Z]+:' -or $link.StartsWith('#')){return $m.Value}
    $path=[IO.Path]::GetFullPath((Join-Path (Join-Path $root 'docs') $link.Split('#')[0]))
    if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Readme link leaves repository.'}
    $fragment=if($link.Contains('#')){'#'+$link.Split('#',2)[1]}else{''}
    return '](https://github.com/TheBinaryShadow/vt7pty/blob/'+$source+'/'+$path.Substring($root.Length+1).Replace('\','/')+$fragment+')'
})
[IO.File]::WriteAllText($readme,$text,[Text.UTF8Encoding]::new($false))
$files=@(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');Bytes=$_.Length;Sha256=(Hash-H4 $_.FullName)}
})
$manifest=[ordered]@{
    Kind='VT7PtyH4ManualCandidate';SchemaVersion=1;H4Accepted=$false
    Runtime=[ordered]@{SourceCommit=$runtimeSource;SourceTreeClean=$true;PackageVersion=$version;ApiVersion='1.1';ProtocolVersion=2
        Architecture='x64';Configuration='Release';UnchangedFromRc4=$true;ArchiveSha256=$rc4Hash;ManifestSha256=$rc4ManifestHash;Files=$runtimeFiles}
    Harness=[ordered]@{SourceCommit=$source;SourceTreeClean=$true;ManualRevision=2;Architecture='x64';Configuration='Release'}
    Manual=[ordered]@{Required=@('US','HR','Session');Launcher='RUN-H4-MANUAL.cmd';PhysicalInputRequired=$true;SyntheticInputAccepted=$false}
    AutomatedRepeatRequired=$false;ElevatedCallerRepeatRequired=$false
    Files=$files
}
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'),($manifest | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
if($source -cne (& git -C $root rev-parse HEAD).Trim() -or @(& git -C $root status --porcelain=v1).Count){throw 'Source changed while packaging.'}
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$hash=Hash-H4 $zip
[IO.File]::WriteAllText(($zip+'.sha256'),$hash+'  '+[IO.Path]::GetFileName($zip)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host ('H4 manual-only candidate: '+$zip)
Write-Host ('SHA-256: '+$hash)
