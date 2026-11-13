[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9]+(?:[.-][a-z0-9]+)*$')][string]$Suffix='rc1',
    [string]$ControlArchive='',
    [string]$ReferenceArchive='',
    [switch]$NoBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$root=$PSScriptRoot
$source=(& git -C $root rev-parse HEAD).Trim()
$changes=@(& git -C $root status --porcelain=v1)
if($LASTEXITCODE -ne 0 -or $changes.Count) { throw 'Diagnostic packaging requires a clean source tree.' }
if(-not $ControlArchive) { $ControlArchive=Join-Path $root 'artifacts\fidelity\packages\VT7Pty-host-cancellation-0.5.0-rc1.zip' }
if(-not $ReferenceArchive) { $ReferenceArchive=Join-Path $root 'artifacts\fidelity\packages\VT7Pty-host-freeze-diagnostics-0.5.0-rc1.zip' }
$referenceHash='ed03734b8d6198710f0bfc8ef475b05f36f59949999a08748b9dc34e3aed2371'
$referenceManifestHash='1ee53b275e70c1361eb0a6fc99c62efa8de6810cbbbb752173024df1393e82d0'
if((Get-FileHash -LiteralPath $ReferenceArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $referenceHash) { throw 'Supply the exact issued diagnostic rc1 ZIP for the unchanged trace reference.' }
$controlHash='19d2755ad052744953efaed46c91a3fda18694aa6337086808682e22aaf71c1c'
$controlManifestHash='67b715e9ac7913d6b520ac5bfe99d67b942fb1e8b161d764bd4319c6f85145e7'
if(-not (Test-Path -LiteralPath $ControlArchive -PathType Leaf) -or
    (Get-FileHash -LiteralPath $ControlArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $controlHash) { throw 'Supply the exact issued H3 rc1 ZIP for unchanged controls.' }
if(-not $NoBuild) {
    & (Join-Path $root 'Build-VT7Pty.ps1') -Configuration Release
    & (Join-Path $root 'Analyze-VT7Pty.ps1')
}
& (Join-Path $root 'Verify-VT7Pty.ps1') -Configuration Release -NoBuild
$verifyPath=Join-Path $root 'artifacts\verification\x64\Release\verification.json'
$verification=Get-Content -LiteralPath $verifyPath -Raw | ConvertFrom-Json
if(-not $verification.SourceTreeClean -or $verification.SourceCommit -ne $source) { throw 'Verification does not match the clean diagnostic source.' }
$parent=Join-Path $root 'artifacts\fidelity\packages'
[IO.Directory]::CreateDirectory($parent) | Out-Null
$stem='VT7Pty-host-coordination-0.5.0-'+$Suffix
$stage=Join-Path $parent $stem
$zip=$stage+'.zip'
foreach($path in @($stage,$zip,($zip+'.sha256'))) { if(Test-Path -LiteralPath $path) { throw 'Candidate exists; choose a fresh suffix.' } }
[IO.Directory]::CreateDirectory($stage) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($ControlArchive),(Join-Path $stage 'control'))
$controlRoot=Join-Path $stage 'control'
if((Get-FileHash -LiteralPath (Join-Path $controlRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $controlManifestHash) { throw 'Original control manifest mismatch.' }
$control=Get-Content -LiteralPath (Join-Path $controlRoot 'manifest.json') -Raw | ConvertFrom-Json
if($control.Harness.SourceCommit -ne '464b3546e2402cbd419b328e055291f85a4dcb14' -or $control.Harness.Version -ne '1') { throw 'Wrong control source.' }
foreach($entry in $control.Files) {
    $path=[IO.Path]::GetFullPath((Join-Path $controlRoot $entry.Path))
    if(-not $path.StartsWith($controlRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or
        (Get-Item -LiteralPath $path).Length -ne $entry.Bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Sha256) { throw ('Control extraction changed: '+$entry.Path) }
}
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($ReferenceArchive),(Join-Path $stage 'reference'))
$referenceRoot=Join-Path $stage 'reference'
if((Get-FileHash -LiteralPath (Join-Path $referenceRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $referenceManifestHash) { throw 'Original trace manifest changed.' }
$reference=Get-Content -LiteralPath (Join-Path $referenceRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $reference.Files) {
    $path=[IO.Path]::GetFullPath((Join-Path $referenceRoot $entry.Path))
    if(-not $path.StartsWith($referenceRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $path).Length -ne $entry.Bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Sha256) { throw ('Original trace extraction changed: '+$entry.Path) }
}
$traceBin=Join-Path $stage 'experiment\bin'
[IO.Directory]::CreateDirectory($traceBin) | Out-Null
foreach($name in @('VT7Pty.dll')) { Copy-Item -LiteralPath (Join-Path $controlRoot ('bin\'+$name)) -Destination (Join-Path $traceBin $name) }
foreach($name in @('HostCancellationCoordinatedProbe.exe','HostCancellationCoordinatedProbe.pdb','HostCancellationCoordinatedHook.dll','HostCancellationCoordinatedHook.pdb')) {
    Copy-Item -LiteralPath (Join-Path $root ('artifacts\bin\x64\Release\'+$name)) -Destination (Join-Path $traceBin $name)
}
Copy-Item -LiteralPath (Join-Path $root 'artifacts\bin\x64\Release\VT7Pty-CoordinatedAgent.exe') -Destination (Join-Path $traceBin 'VT7Pty-Agent.exe')
Copy-Item -LiteralPath (Join-Path $root 'artifacts\bin\x64\Release\VT7Pty-CoordinatedAgent.pdb') -Destination (Join-Path $traceBin 'VT7Pty-CoordinatedAgent.pdb')
$plan=[ordered]@{
    'README.md'='docs\HOST_COORDINATION_PROBE.md'
    'HOST_CANCELLATION_DESIGN.md'='docs\HOST_CANCELLATION_DESIGN.md'
    'tools\research\Invoke-HostCoordination.ps1'='tools\research\Invoke-HostCoordination.ps1'
    'tools\platform\Windows7Compat.ps1'='tools\platform\Windows7Compat.ps1'
    'RUN-HOST-COORDINATION-NONESU.cmd'='RUN-HOST-COORDINATION-NONESU.cmd'
    'RUN-HOST-COORDINATION-ESU.cmd'='RUN-HOST-COORDINATION-ESU.cmd'
    'RUN-HOST-COORDINATION-LEGACY.cmd'='RUN-HOST-COORDINATION-LEGACY.cmd'
    'LICENSE.txt'='LICENSE';'CREDITS.md'='CREDITS.md';'UPSTREAM.md'='UPSTREAM.md'
}
foreach($relative in $plan.Keys) {
    $target=Join-Path $stage $relative
    [IO.Directory]::CreateDirectory((Split-Path $target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $plan[$relative]) -Destination $target
}
foreach($relative in @('README.md','HOST_CANCELLATION_DESIGN.md')) {
    $path=Join-Path $stage $relative
    $text=[IO.File]::ReadAllText($path)
    $text=[regex]::Replace($text,'\]\(([^)]+)\)',{
        param($match)
        $link=$match.Groups[1].Value
        if($link -match '^[a-zA-Z]+:' -or $link.StartsWith('#')) { return $match.Value }
        if($link -eq 'HOST_COORDINATION_PROBE.md') { return '](README.md)' }
        if($link -eq 'HOST_CANCELLATION_DESIGN.md') { return '](HOST_CANCELLATION_DESIGN.md)' }
        $absolute=[IO.Path]::GetFullPath((Join-Path (Join-Path $root 'docs') $link.Split('#')[0]))
        if(-not $absolute.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Documentation link leaves the repository.' }
        $fragment=if($link.Contains('#')) {'#'+$link.Split('#',2)[1]} else {''}
        return '](https://github.com/TheBinaryShadow/vt7pty/blob/'+$source+'/'+$absolute.Substring($root.Length+1).Replace('\','/')+$fragment+')'
    })
    [IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($false))
}
$files=@(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');Bytes=$_.Length;Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$manifest=[ordered]@{
    SchemaVersion=1;Kind='VT7PtyHostCoordinationDiagnostics';DiagnosticOnly=$true
    Control=[ordered]@{Version='1';SourceCommit=$control.Harness.SourceCommit;ArchiveSha256=$controlHash;ManifestSha256=$controlManifestHash;OriginalFiles=18}
    Reference=[ordered]@{Version='3';SourceCommit=$reference.Experiment.SourceCommit;ArchiveSha256=$referenceHash;ManifestSha256=$referenceManifestHash;OriginalFiles=71}
    Experiment=[ordered]@{Version='4';AgentGateProtocol=1;ResearchAgent=$true;BaselineCoordinated=$true;NeighborDrainIntervalMs=250;CallbackCompletionMarker=$true;SourceCommit=$source;SourceTreeClean=$true;Architecture='x64';Configuration='Release'
        VerificationSha256=(Get-FileHash -LiteralPath $verifyPath -Algorithm SHA256).Hash.ToLowerInvariant();Toolchain=$verification.InspectionTool
        KeyTraceCapacity=16;FixtureTimelineCapacity=64;OutputFrameCapacity=512;WindowRingCapacity=4096;ChunkCapacity=32
        AgentSha256=(Get-FileHash -LiteralPath (Join-Path $traceBin 'VT7Pty-Agent.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
        RuntimeDllSha256=(Get-FileHash -LiteralPath (Join-Path $traceBin 'VT7Pty.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
        AgentOriginalFilename='VT7Pty-CoordinatedAgent.exe';Cases=23}
    Runtime=$control.Runtime;Files=$files
}
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'),($manifest | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($zip+'.sha256'),$hash+'  '+[IO.Path]::GetFileName($zip)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host ('Diagnostic-only ZIP: '+$zip)
Write-Host ('SHA-256: '+$hash)
