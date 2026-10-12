[CmdletBinding()]
param(
    [ValidateLength(1,49)]
    [ValidatePattern('^[a-z0-9]+(?:[.-][a-z0-9]+)*$')]
    [string]$Suffix='step-1.1-rc2',
    [string]$RuntimeArchive='',
    [switch]$NoBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$root=$PSScriptRoot
$sourceCommit=(& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify harness source.' }
$changes=@(& git -C $root status --porcelain=v1)
if ($LASTEXITCODE -ne 0 -or $changes.Count) { throw 'Fidelity candidate packaging requires a clean source tree.' }
if (-not $RuntimeArchive) { $RuntimeArchive=Join-Path $root 'artifacts\packages\VT7Pty-0.5.0-win7-x64-release-runtime.zip' }
$archiveHash='59554a9cf8246395e60f30875e047d1bfa20d4f3f462c113ef705a86708f5304'
if (-not (Test-Path -LiteralPath $RuntimeArchive -PathType Leaf) -or
    (Get-FileHash -LiteralPath $RuntimeArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archiveHash) {
    throw 'Supply the unchanged accepted 0.5.0 runtime ZIP; its SHA-256 does not match.'
}
if (-not $NoBuild) { & (Join-Path $root 'Build-VT7Pty.ps1') -Configuration Release }
& (Join-Path $root 'Verify-VT7Pty.ps1') -Configuration Release -NoBuild
$bin=Join-Path $root 'artifacts\bin\x64\Release'
$verification=Get-Content -LiteralPath (Join-Path $root 'artifacts\verification\x64\Release\verification.json') -Raw | ConvertFrom-Json
if (-not $verification.SourceTreeClean -or $verification.SourceCommit -ne $sourceCommit) { throw 'Verification identity does not match the clean harness source.' }
& (Join-Path $bin 'VtStateTest.exe')
if ($LASTEXITCODE -ne 0) { throw 'Independent VT oracle tests failed.' }
$parent=Join-Path $root 'artifacts\fidelity\packages'
[IO.Directory]::CreateDirectory($parent) | Out-Null
$stem='VT7Pty-fidelity-baseline-0.5.0-'+$Suffix
$stage=Join-Path $parent $stem
$zip=Join-Path $parent ($stem+'.zip')
$runtime=Join-Path $parent ($stem+'-runtime-extracted')
foreach($path in @($stage,$zip,$runtime,($zip+'.sha256'))) {
    if (Test-Path -LiteralPath $path) { throw "Candidate already exists; use a fresh suffix: $path" }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($RuntimeArchive),$runtime)
$runtimeManifest=Get-Content -LiteralPath (Join-Path $runtime 'manifest.json') -Raw | ConvertFrom-Json
if ($runtimeManifest.Version -ne '0.5.0' -or $runtimeManifest.SourceCommit -ne 'cbd17727b3c8af21e5afd4499b8dca7d2140ed9d') { throw 'Runtime manifest identity mismatch.' }
$runtimeFiles=[ordered]@{}
[IO.Directory]::CreateDirectory((Join-Path $stage 'bin')) | Out-Null
foreach($name in @('VT7Pty.dll','VT7Pty-Agent.exe')) {
    $file=Join-Path $runtime ('bin\'+$name)
    $entry=@($runtimeManifest.Files | Where-Object { $_.Path -eq ('bin/'+$name) })
    $hash=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($entry.Count -ne 1 -or $entry[0].Sha256 -ne $hash) { throw "Runtime binary integrity mismatch: $name" }
    $runtimeFiles[$name]=$hash
    Copy-Item -LiteralPath $file -Destination (Join-Path $stage ('bin\'+$name))
}
foreach($name in @('VtStateTest','FidelityFixture','FidelityTest')) {
    foreach($extension in @('exe','pdb')) { Copy-Item -LiteralPath (Join-Path $bin ($name+'.'+$extension)) -Destination (Join-Path $stage ('bin\'+$name+'.'+$extension)) }
}
$plan=[ordered]@{
    'README.md'='docs\FIDELITY_BASELINE.md'
    'cases.json'='tools\fidelity\cases.json'
    'tools\fidelity\Invoke-FidelityBaseline.ps1'='tools\fidelity\Invoke-FidelityBaseline.ps1'
    'tools\platform\Windows7Compat.ps1'='tools\platform\Windows7Compat.ps1'
    'RUN-FIDELITY-NONESU.cmd'='RUN-FIDELITY-NONESU.cmd'
    'RUN-FIDELITY-ESU.cmd'='RUN-FIDELITY-ESU.cmd'
    'RUN-FIDELITY-LEGACY.cmd'='RUN-FIDELITY-LEGACY.cmd'
    'LICENSE.txt'='LICENSE'
    'CREDITS.md'='CREDITS.md'
    'UPSTREAM.md'='UPSTREAM.md'
}
foreach($relative in $plan.Keys) {
    $destination=Join-Path $stage $relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $plan[$relative]) -Destination $destination
}
# The portable README has no adjacent repository validation directory.
# Pin its evidence links to the harness source instead of shipping broken links.
$portableReadme=Join-Path $stage 'README.md'
$readmeText=[IO.File]::ReadAllText($portableReadme)
$readmeText=[regex]::Replace($readmeText,'\]\(validation/([^)]+)\)',
    '](https://github.com/TheBinaryShadow/vt7pty/blob/'+$sourceCommit+'/docs/validation/$1)')
[IO.File]::WriteAllText($portableReadme,$readmeText,[Text.UTF8Encoding]::new($false))
# Keep the product's original manifest/resources distinct from the new harness.
Copy-Item -LiteralPath (Join-Path $runtime 'manifest.json') -Destination (Join-Path $stage 'runtime-manifest.json')
$files=@(Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
    [ordered]@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');Bytes=$_.Length;
        Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$manifest=[ordered]@{
    SchemaVersion=1;Kind='VT7PtyFidelityBaseline'
    Runtime=[ordered]@{Version='0.5.0';SourceCommit='cbd17727b3c8af21e5afd4499b8dca7d2140ed9d';ArchiveSha256=$archiveHash;Files=$runtimeFiles}
    Harness=[ordered]@{Version='2';SourceCommit=$sourceCommit;SourceTreeClean=$true;Configuration='Release';Architecture='x64';
        VerificationSha256=(Get-FileHash -LiteralPath (Join-Path $root 'artifacts\verification\x64\Release\verification.json') -Algorithm SHA256).Hash.ToLowerInvariant();
        Toolchain=$verification.InspectionTool}
    Files=$files
}
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'),($manifest | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($zip+'.sha256'),$hash+'  '+[IO.Path]::GetFileName($zip)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host ('Fidelity baseline ZIP: '+$zip)
Write-Host ('SHA-256: '+$hash)
Write-Host ('Development comparison: powershell.exe -NoProfile -File "'+(Join-Path $stage 'tools\fidelity\Invoke-FidelityBaseline.ps1')+'" -Tier DevelopmentHost')
