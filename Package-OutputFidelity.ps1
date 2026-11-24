[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9]+(?:[.-][a-z0-9]+)*$')][string]$Suffix = 'rc3',
    [string]$ReleasedRuntimeArchive = '',
    [string]$AcceptedInputArchive = '',
    [ValidateSet('Output','Screen')][string]$Suite = 'Output',
    [switch]$WithCandidate
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = $PSScriptRoot
. (Join-Path $root 'tools\platform\Windows7Compat.ps1')
. (Join-Path $root 'tools\output\OutputEvidence.ps1')
$screen = $Suite -eq 'Screen'
if ($WithCandidate -and -not $screen) { throw 'The current build can be added to the Screen suite.' }
if ($screen) {
    . (Join-Path $root 'tools\output\ScreenEvidence.ps1')
    if (-not $PSBoundParameters.ContainsKey('Suffix')) {
        $Suffix = if ($WithCandidate) { 'rc3' } else { 'rc1' }
    }
}
$source = (& git -C $root rev-parse --verify HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or @(& git -C $root status --porcelain=v1).Count) {
    throw 'Fidelity packaging requires clean committed source.'
}
$version = [IO.File]::ReadAllText((Join-Path $root 'VERSION.txt')).Trim()
if ($version -cne '0.5.1-dev') { throw 'Wrong fidelity harness version.' }
$packages = Join-Path $root 'artifacts\fidelity\packages'
if (-not $ReleasedRuntimeArchive) {
    $ReleasedRuntimeArchive = Join-Path $root 'artifacts\packages\VT7Pty-0.5.0-win7-x64-release-runtime.zip'
}
if (-not $AcceptedInputArchive) {
    $AcceptedInputArchive = Join-Path $packages 'VT7Pty-input-0.5.1-dev-rc2.zip'
}
$baselineHash = '59554a9cf8246395e60f30875e047d1bfa20d4f3f462c113ef705a86708f5304'
$acceptedHash = '93d1a1b290d796124b143d490181a861d6224bc61f36ca1cbe46113585a57fb5'
if ((Get-VT7Sha256 $ReleasedRuntimeArchive) -cne $baselineHash -or
    (Get-VT7Sha256 $AcceptedInputArchive) -cne $acceptedHash) {
    throw 'Provide the exact accepted 0.5.0 release and input rc2 ZIPs.'
}

$stage = Join-Path $packages ('VT7Pty-' + $Suite.ToLowerInvariant() + '-' + $version + '-' + $Suffix)
$zip = $stage + '.zip'
foreach ($path in @($stage,$zip,($zip + '.sha256'))) {
    if (Test-Path -LiteralPath $path) { throw ('Candidate already exists: ' + $path) }
}

& (Join-Path $root 'Build-VT7Pty.ps1') -Configuration Release
& (Join-Path $root 'Verify-VT7Pty.ps1') -Configuration Release -NoBuild
$bin = Join-Path $root 'artifacts\bin\x64\Release'
$verificationPath = Join-Path $root 'artifacts\verification\x64\Release\verification.json'
$verification = Read-VT7Json $verificationPath
if ($verification.SourceCommit -cne $source -or -not $verification.SourceTreeClean) {
    throw 'Wrong local verification identity.'
}
foreach ($name in @('FidelityTest','FidelityFixture','VtStateTest')) {
    $identity = [IO.File]::ReadAllLines((Join-Path $root ('build\obj\' + $name + '\x64\Release\generated\SourceIdentity.txt')))
    if ($identity[0] -cne $version -or $identity[1] -cne $source) { throw ('Wrong harness identity: ' + $name) }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Export-OutputArchiveEntry($Archive, [string]$EntryName, [string]$Destination) {
    $entries = @($Archive.Entries | Where-Object { $_.FullName.Replace('\','/') -ceq $EntryName })
    if ($entries.Count -ne 1) { throw ('Wrong ZIP entry inventory: ' + $EntryName) }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entries[0], $Destination)
}

function Export-OutputRuntime([string]$Role, [string]$ArchivePath, [string]$ArchiveHash, [string]$Commit,
    [string]$Prefix, [string[]]$Names, [string]$ManifestHash = '') {
    $folder = Join-Path $stage $Role
    $runtimeBin = Join-Path $folder 'bin'
    [void][IO.Directory]::CreateDirectory($runtimeBin)
    $archive = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ArchivePath))
    $runtimeFiles = @{}
    try {
        $originalPath = Join-Path $folder 'source-manifest.json'
        Export-OutputArchiveEntry $archive 'manifest.json' $originalPath
        $actualHash = Get-VT7Sha256 $originalPath
        if ($ManifestHash -and $actualHash -cne $ManifestHash) { throw ('Wrong original manifest: ' + $Role) }
        $original = Read-VT7Json $originalPath
        $identity = if ($Role -ceq 'baseline') { $original } else { $original.Runtime }
        if ($identity.SourceCommit -cne $Commit) { throw ('Wrong original runtime source: ' + $Role) }
        foreach ($name in $Names) {
            $entry = $Prefix + $name
            $destination = Join-Path $runtimeBin $name
            Export-OutputArchiveEntry $archive $entry $destination
            $file = @($original.Files | Where-Object { $_.Path -ceq $entry })
            if ($file.Count -ne 1 -or (Get-VT7Sha256 $destination) -cne $file[0].Sha256 -or
                (Get-Item -LiteralPath $destination).Length -ne $file[0].Bytes) { throw ('Wrong runtime artifact: ' + $entry) }
            $runtimeFiles[$name] = $entry
        }
    } finally {
        $archive.Dispose()
    }
    return @{Archive = [IO.Path]::GetFileName($ArchivePath); ArchiveSha256 = $ArchiveHash
        ManifestSha256 = $actualHash; SourceCommit = $Commit; Binaries = $runtimeFiles}
}

$baseline = Export-OutputRuntime 'baseline' $ReleasedRuntimeArchive $baselineHash 'cbd17727b3c8af21e5afd4499b8dca7d2140ed9d' 'bin/' @('VT7Pty.dll','VT7Pty-Agent.exe')
$acceptedNames = @('VT7Pty.dll','VT7Pty-Agent.exe','VT7Pty-Host.dll','VT7Pty-DebugServer.exe')
$acceptedManifest = 'e66b761b06c4c99fb885dcfe1862537cd9fb3dd577365569668639ccd09f7517'
$accepted = Export-OutputRuntime 'accepted' $AcceptedInputArchive $acceptedHash '0ca6e3fa78481cd4df35820b9fe3988f46e0bff4' 'runtime/bin/' $acceptedNames $acceptedManifest
$baseline.Version = '0.5.0'
$accepted.Version = $version
$runtimes = @{baseline = $baseline; accepted = $accepted}
$roles = @('baseline','accepted')
$verificationHash = Get-VT7Sha256 $verificationPath
if ($WithCandidate) {
    $roles += 'candidate'
    $folder = Join-Path $stage 'candidate'
    $runtimeBin = Join-Path $folder 'bin'
    [void][IO.Directory]::CreateDirectory($runtimeBin)
    Copy-Item -LiteralPath $verificationPath -Destination (Join-Path $folder 'source-verification.json')
    $binaries = @{}
    foreach ($name in $acceptedNames) {
        Copy-Item -LiteralPath (Join-Path $bin $name) -Destination (Join-Path $runtimeBin $name)
        $binaries[$name] = $name
    }
    $runtimes.candidate = @{Version = $version; SourceCommit = $source; ManifestSha256 = $verificationHash; Binaries = $binaries}
}
$caseArgument = '--' + $Suite.ToLowerInvariant() + '-cases'
$casesJson = $null
foreach ($role in $roles) {
    $runtimeBin = Join-Path $stage ($role + '\bin')
    foreach ($name in @('FidelityTest.exe','FidelityFixture.exe')) {
        Copy-Item -LiteralPath (Join-Path $bin $name) -Destination (Join-Path $runtimeBin $name)
    }
    $recipe = (& (Join-Path $runtimeBin 'FidelityTest.exe') $caseArgument | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or ($null -ne $casesJson -and $recipe -cne $casesJson)) {
        throw ('Cannot load the same fidelity recipes with runtime ' + $role)
    }
    $casesJson = $recipe
}
$compiled = $casesJson | ConvertFrom-Json
& (Join-Path $bin 'VtStateTest.exe')
if ($LASTEXITCODE -ne 0) { throw 'Independent VT state tests failed.' }

$plan = @{
    'README.md' = 'docs\' + $Suite.ToUpperInvariant() + '_FIDELITY.md'
    'LICENSE.txt' = 'LICENSE'
    'UPSTREAM.md' = 'UPSTREAM.md'
    'CREDITS.md' = 'CREDITS.md'
    'bin\VtStateTest.exe' = 'artifacts\bin\x64\Release\VtStateTest.exe'
    'symbols\FidelityTest.pdb' = 'artifacts\bin\x64\Release\FidelityTest.pdb'
    'symbols\FidelityFixture.pdb' = 'artifacts\bin\x64\Release\FidelityFixture.pdb'
    'tools\platform\Windows7Compat.ps1' = 'tools\platform\Windows7Compat.ps1'
    'tools\platform\NativeCapture.ps1' = 'tools\platform\NativeCapture.ps1'
    'tools\output\OutputEvidence.ps1' = 'tools\output\OutputEvidence.ps1'
    'tools\output\Invoke-OutputFidelity.ps1' = 'tools\output\Invoke-OutputFidelity.ps1'
}
if ($screen) { $plan['tools\output\ScreenEvidence.ps1'] = 'tools\output\ScreenEvidence.ps1' }
foreach ($tier in @('NONESU','ESU','LEGACY')) {
    $launcher = 'RUN-' + $Suite.ToUpperInvariant() + '-' + $tier + '.cmd'
    $plan[$launcher] = $launcher
}
foreach ($relative in $plan.Keys) {
    $destination = Join-Path $stage $relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
    Copy-Item -LiteralPath (Join-Path $root $plan[$relative]) -Destination $destination
}
$files = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{Path = $_.FullName.Substring($stage.Length + 1).Replace('\','/'); Bytes = $_.Length; Sha256 = Get-VT7Sha256 $_.FullName}
})
$manifest = @{
    Runtimes = $runtimes; SessionFlags = 0
    Harness = @{SourceCommit = $source; SourceTreeClean = $true; Configuration = 'Release'; Architecture = 'x64'
        SnapshotBytes = $compiled.SnapshotBytes; VerificationSha256 = $verificationHash}
    Files = $files
}
if ($screen) {
    if ($compiled.SchemaVersion -ne 2) { throw 'Wrong viewport harness schema.' }
    $manifest.SchemaVersion = 3
    $manifest.Kind = 'VT7PtyScreenCharacterization'
    $manifest.Step14Accepted = $false
    $manifest.ResizePairs = $compiled.ResizePairs
    $manifest.ViewportResizePairs = $compiled.ViewportResizePairs
    $manifest.Cases = @($compiled.Cases | ForEach-Object { @{Name = $_.Name; Area = $_.Area; Mode = $_.Mode} })
} else {
    $manifest.SchemaVersion = 2
    $manifest.Kind = 'VT7PtyOutputCharacterization'
    $manifest.Step13Accepted = $false
    $manifest.CheckpointsPerCase = 2
    $manifest.Cases = @($compiled.Cases | ForEach-Object {
        @{Name = $_.Name; Area = $_.Area; RequestedOutputCP = $_.RequestedOutputCP; WidthProbe = $_.WidthProbe}
    })
}
Write-VT7Json (Join-Path $stage 'manifest.json') $manifest
if ($screen) { $null = Read-ScreenPackage $stage } else { $null = Read-OutputPackage $stage }
if ($source -cne (& git -C $root rev-parse HEAD).Trim() -or @(& git -C $root status --porcelain=v1).Count) {
    throw 'Source changed during fidelity candidate packaging.'
}
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
$hash = Get-VT7Sha256 $zip
[IO.File]::WriteAllText($zip + '.sha256', $hash + '  ' + [IO.Path]::GetFileName($zip) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))
Write-Host ($Suite + ' characterization ZIP: ' + $zip)
Write-Host ('SHA-256: ' + $hash)
