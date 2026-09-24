[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReleaseSetPath,
    [Parameter(Mandatory = $true)][string]$NonEsuResult,
    [Parameter(Mandatory = $true)][string]$EsuResult,
    [Parameter(Mandatory = $true)][string]$LegacyResult,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$releaseSetPath = [IO.Path]::GetFullPath($ReleaseSetPath)
$packageDirectory = Split-Path -Parent $releaseSetPath
$releaseSet = Get-Content -LiteralPath $releaseSetPath -Raw | ConvertFrom-Json
$releaseStem = [IO.Path]::GetFileNameWithoutExtension($releaseSetPath) -replace '-release-set$', ''
$checksumPath = Join-Path $packageDirectory "$releaseStem.sha256"
$failures = @()
if ($releaseSet.SchemaVersion -ne 1 -or $releaseSet.Product -ne 'VT7Pty' -or
        $releaseSet.SourceTreeClean -ne $true -or
        $releaseSet.Configuration -ne 'Release' -or
        $releaseSet.Architecture -ne 'x64' -or
        $releaseSet.MinimumOperatingSystem -ne 'Windows 7 SP1') {
    $failures += 'Release-set identity or source-clean state is invalid.'
}
$archiveKinds = @($releaseSet.Archives | ForEach-Object Kind | Sort-Object)
if (($archiveKinds -join ',') -ne 'development,runtime,symbols,tests') {
    $failures += 'The release set must contain one archive of each required kind.'
}
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    $failures += 'The release-set checksum file is missing.'
    $checksumLines = @()
} else {
    $checksumLines = @(Get-Content -LiteralPath $checksumPath)
}
foreach ($archive in $releaseSet.Archives) {
    $path = Join-Path $packageDirectory $archive.Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $failures += "Missing $($archive.Kind) archive: $($archive.Name)"
        continue
    }
    $file = Get-Item -LiteralPath $path
    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($file.Length -ne $archive.Bytes -or $actualHash -ne $archive.Sha256) {
        $failures += "Hash or length mismatch: $($archive.Name)"
    }
    if ("$($archive.Sha256)  $($archive.Name)" -notin $checksumLines) {
        $failures += "Checksum entry is missing: $($archive.Name)"
    }
}
$releaseSetHash = (Get-FileHash -LiteralPath $releaseSetPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ("$releaseSetHash  $([IO.Path]::GetFileName($releaseSetPath))" -notin $checksumLines) {
    $failures += 'The release-set checksum entry is missing.'
}
if ($checksumLines.Count -ne 5) {
    $failures += 'The checksum file must list four archives and the release-set manifest.'
}

$testArchive = $releaseSet.Archives | Where-Object Kind -eq 'tests' | Select-Object -First 1
if ($null -eq $testArchive) { throw 'The release set has no test archive.' }
$testPath = Join-Path $packageDirectory $testArchive.Name
if (Test-Path -LiteralPath $testPath -PathType Leaf) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($testPath)
    try {
        $entry = $zip.GetEntry('manifest.json')
        if ($null -eq $entry) { throw 'The test archive has no manifest.json.' }
        $stream = $entry.Open()
        $memory = New-Object IO.MemoryStream
        try { $stream.CopyTo($memory) }
        finally { $stream.Dispose() }
        $bytes = $memory.ToArray()
        $memory.Dispose()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $manifestHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $testManifest = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
        if ($manifestHash -ne $testArchive.ManifestSha256 -or
                $testManifest.PackageKind -ne 'tests' -or
                $testManifest.SourceCommit -ne $releaseSet.SourceCommit -or
                $testManifest.Version -ne $releaseSet.Version -or
                $testManifest.SourceTreeClean -ne $true) {
            $failures += 'The test-archive manifest does not match the release set.'
        }
    } finally {
        $zip.Dispose()
    }
}

$records = @()
foreach ($target in @(
        @{ Tier = 'NonESU'; Path = $NonEsuResult },
        @{ Tier = 'ESU'; Path = $EsuResult },
        @{ Tier = 'Legacy'; Path = $LegacyResult })) {
    $path = [IO.Path]::GetFullPath($target.Path)
    $result = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    $issues = @()
    if ($result.SchemaVersion -ne 1 -or $result.Status -ne 'Pass' -or
            $result.DeclaredTier -ne $target.Tier) {
        $issues += 'Tier, schema, or overall status mismatch.'
    }
    if ($result.Package.SourceCommit -ne $releaseSet.SourceCommit -or
            $result.Package.Version -ne $releaseSet.Version -or
            $result.Package.ApiVersion -ne $releaseSet.ApiVersion -or
            $result.Package.ProtocolVersion -ne $releaseSet.ProtocolVersion -or
            $result.Package.ManifestSha256 -ne $testArchive.ManifestSha256) {
        $issues += 'Package identity does not match the exact test archive.'
    }
    if ($result.OperatingSystem.BuildNumber -ne '7601' -or
            $result.OperatingSystem.ServicePackMajorVersion -ne 1 -or
            $result.OperatingSystem.Architecture -ne '64-bit') {
        $issues += 'The target is not recorded as Windows 7 SP1 x64.'
    }
    if ($target.Tier -eq 'Legacy' -and
            ($result.OperatingSystem.PowerShellVersion -notmatch '^2\.' -or
                @($result.OperatingSystem.Hotfixes | Where-Object HotFixId -eq 'KB3191566').Count -ne 0)) {
        $issues += 'The legacy target must report PowerShell 2.0 without KB3191566.'
    }
    if (@($result.PreflightFailures).Count -ne 0 -or
            @($result.InventoryWarnings).Count -ne 0 -or
            @($result.CrashArtifacts).Count -ne 0) {
        $issues += 'Preflight, inventory, or crash evidence requires disposition.'
    }
    if (@($result.Tests).Count -ne 30 -or
            @($result.Tests | Where-Object Status -ne 'Pass').Count -ne 0) {
        $issues += 'The complete 30-case test matrix did not pass.'
    }
    if (@($result.Tests | ForEach-Object Name | Sort-Object -Unique).Count -ne 30) {
        $issues += 'The test matrix contains missing or duplicate case names.'
    }
    $soak = $result.Tests | Where-Object Name -eq 'Session contract: SOAK 120 minutes' |
        Select-Object -First 1
    if ($null -eq $soak -or $soak.Status -ne 'Pass' -or
            $soak.DurationMilliseconds -lt 7200000) {
        $issues += 'The full 120-minute soak did not pass.'
    }
    $timeout = $result.Tests | Where-Object Name -eq 'Negative control: timeout' |
        Select-Object -First 1
    if ($null -eq $timeout -or $timeout.Status -ne 'Pass' -or
            $timeout.TimedOut -ne $true) {
        $issues += 'The negative timeout control did not detect a timeout.'
    }
    foreach ($issue in $issues) { $failures += "$($target.Tier): $issue" }
    $records += [ordered]@{
        Tier = $target.Tier
        Machine = $result.Machine.ComputerName
        Manufacturer = $result.Machine.Manufacturer
        Model = $result.Machine.Model
        OperatingSystem = $result.OperatingSystem.Version
        PowerShellVersion = $result.OperatingSystem.PowerShellVersion
        StartedAt = $result.StartedAt
        CompletedAt = $result.CompletedAt
        ResultFile = $path
        ResultSha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        CaseCount = @($result.Tests).Count
        SoakDurationMilliseconds = if ($null -eq $soak) { $null } else { $soak.DurationMilliseconds }
        Issues = $issues
    }
}
if (@($records | ForEach-Object Machine | Sort-Object -Unique).Count -ne 3) {
    $failures += 'The three declared tiers do not identify distinct test machines.'
}
$baselineNames = @((Get-Content -LiteralPath $NonEsuResult -Raw |
        ConvertFrom-Json).Tests.Name | Sort-Object)
foreach ($path in @($EsuResult, $LegacyResult)) {
    $names = @((Get-Content -LiteralPath $path -Raw |
        ConvertFrom-Json).Tests.Name | Sort-Object)
    if (@(Compare-Object $baselineNames $names).Count -ne 0) {
        $failures += "The target record does not contain the same case names: $path"
    }
}

if (-not $OutputPath) {
    $outputDirectory = Join-Path $PSScriptRoot '..\..\artifacts\release-review'
    [IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    $OutputPath = Join-Path $outputDirectory "$releaseStem-review.json"
}
$review = [ordered]@{
    SchemaVersion = 1
    Status = if ($failures.Count) { 'Fail' } else { 'Pass' }
    ReleaseSet = $releaseSetPath
    ReleaseSetSha256 = $releaseSetHash
    SourceCommit = $releaseSet.SourceCommit
    PackageVersion = $releaseSet.Version
    Records = $records
    Failures = $failures
    EnvironmentDisposition = 'Review the recorded hardware against the project owner-approved tier policy.'
    PublicationApproved = $false
}
$review | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Host "Release review $($review.Status): $OutputPath"
if ($failures.Count) { throw ($failures -join [Environment]::NewLine) }
