[CmdletBinding()]
param(
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',

    [ValidatePattern('^[a-z0-9][a-z0-9.-]*$')]
    [string]$PackageSuffix
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = $PSScriptRoot
$artifactRoot = Join-Path $repositoryRoot 'artifacts'
$binaryDirectory = Join-Path $artifactRoot 'bin\x64\Release'
$verificationDirectory = Join-Path $artifactRoot 'verification\x64\Release'
$packageRoot = Join-Path $artifactRoot 'packages'
$stagingRoot = Join-Path $artifactRoot 'package-staging'
$verificationRoot = Join-Path $artifactRoot 'package-verify'
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$version = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'VERSION.txt')).Trim()
$apiHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\include\vt7pty_version.h'))
$protocolHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\shared\Protocol.h'))
$apiMajor = [regex]::Match($apiHeader, '(?m)^#define VT7PTY_API_VERSION_MAJOR (?<v>\d+)\r?$')
$apiMinor = [regex]::Match($apiHeader, '(?m)^#define VT7PTY_API_VERSION_MINOR (?<v>\d+)\r?$')
$protocol = [regex]::Match($protocolHeader,
    '(?m)^constexpr int32_t VT7PTY_PROTOCOL_VERSION = (?<v>\d+);\r?$')
if (-not $apiMajor.Success -or -not $apiMinor.Success -or -not $protocol.Success) {
    throw 'Could not read the public API or client-agent protocol version.'
}
$apiVersion = "$($apiMajor.Groups['v'].Value).$($apiMinor.Groups['v'].Value)"
$protocolVersion = [int]$protocol.Groups['v'].Value
$packageStem = "VT7Pty-$version-win7-x64-release"
if ($PackageSuffix) { $packageStem += "-$PackageSuffix" }
$issuedPaths = @(
    foreach ($kind in @('runtime', 'development', 'symbols', 'tests')) {
        Join-Path $packageRoot "$packageStem-$kind.zip"
    }
    Join-Path $packageRoot "$packageStem-release-set.json"
    Join-Path $packageRoot "$packageStem.sha256"
)
foreach ($path in $issuedPaths) {
    if (Test-Path -LiteralPath $path) {
        throw "Candidate output already exists; choose a fresh suffix: $path"
    }
}

function Assert-ChildPath {
    param([string]$Parent, [string]$Child)
    $base = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    $path = [IO.Path]::GetFullPath($Child)
    if (-not $path.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside $base`: $path"
    }
}

function Get-SourceState {
    $commit = (& git -C $repositoryRoot rev-parse --verify HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
        throw "Could not resolve the source commit: $commit"
    }
    $changes = @(& git -C $repositoryRoot status --porcelain=v1 2>&1)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the source tree.' }
    if ($changes.Count -ne 0) {
        throw "Release packaging requires a clean source tree:`n$($changes -join [Environment]::NewLine)"
    }
    return $commit
}

$sourceCommit = Get-SourceState
Write-Host "Clean Release rebuild for $sourceCommit"
& (Join-Path $repositoryRoot 'Build-VT7Pty.ps1') -Configuration Release -Target Rebuild
& (Join-Path $repositoryRoot 'Analyze-VT7Pty.ps1')
& (Join-Path $repositoryRoot 'Verify-VT7Pty.ps1') -Configuration Release -NoBuild
if ((Get-SourceState) -ne $sourceCommit) {
    throw 'The source commit changed during the release build.'
}

$verificationPath = Join-Path $verificationDirectory 'verification.json'
$verification = Get-Content -LiteralPath $verificationPath -Raw | ConvertFrom-Json
if ($verification.SourceCommit -ne $sourceCommit -or
        $verification.SourceVersion -ne $version -or
        $verification.ApiVersion -ne $apiVersion -or
        $verification.ProtocolVersion -ne $protocolVersion -or
        $verification.Configuration -ne 'Release' -or
        $verification.Architecture -ne 'x64' -or
        $verification.SourceTreeClean -ne $true -or
        @($verification.Tests).Count -eq 0 -or
        @($verification.Tests | Where-Object { $_.ExitCode -ne 0 -or $_.TimedOut }).Count -ne 0) {
    throw 'Release verification does not match the clean source and package identity.'
}
$verificationHash = (Get-FileHash -LiteralPath $verificationPath -Algorithm SHA256).Hash.ToLowerInvariant()

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = (& $vswherePath -latest -products * -version '[17.0,18.0)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
if (-not $installationPath) { throw 'Could not identify Visual Studio 2022.' }
$msvcDirectory = Get-ChildItem -LiteralPath (Join-Path $installationPath 'VC\Tools\MSVC') -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$compilerPath = Join-Path $msvcDirectory.FullName 'bin\Hostx64\x64\cl.exe'
$sdkSettings = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'Directory.Build.props'))
$sdkMatch = [regex]::Match($sdkSettings,
    '<WindowsTargetPlatformVersion>(?<v>[^<]+)</WindowsTargetPlatformVersion>')
if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf) -or -not $sdkMatch.Success) {
    throw 'Could not identify the compiler or Windows SDK version.'
}
$toolchain = [ordered]@{
    VisualStudio = '2022'
    MSVCDirectoryVersion = $msvcDirectory.Name
    CompilerFileVersion = (Get-Item -LiteralPath $compilerPath).VersionInfo.FileVersion
    WindowsSDKVersion = $sdkMatch.Groups['v'].Value
    PowerShellVersion = $PSVersionTable.PSVersion.ToString()
}

$common = [ordered]@{
    'README.md' = Join-Path $repositoryRoot 'README.md'
    'VERSION.txt' = Join-Path $repositoryRoot 'VERSION.txt'
    'CHANGELOG.md' = Join-Path $repositoryRoot 'CHANGELOG.md'
    'LICENSE.txt' = Join-Path $repositoryRoot 'LICENSE'
    'CREDITS.md' = Join-Path $repositoryRoot 'CREDITS.md'
    'UPSTREAM.md' = Join-Path $repositoryRoot 'UPSTREAM.md'
}
$testPrograms = @(
    'ModernCppTest', 'ProtocolTest', 'ProtocolTestAgent', 'BackendSmokeTest',
    'SessionContractTest', 'SessionFixture',
    'fixture-console-color-grid', 'fixture-output-lines', 'fixture-show-argv',
    'fixture-show-console-input', 'fixture-utf16-echo', 'fixture-win32-echo1',
    'fixture-win32-echo2', 'fixture-win32-write1', 'fixture-write-console',
    'tool-conin-mode', 'tool-conout-mode'
)
$plans = [ordered]@{}
foreach ($kind in @('runtime', 'development', 'symbols', 'tests')) {
    $plan = [ordered]@{}
    foreach ($key in $common.Keys) { $plan[$key] = $common[$key] }
    $plans[$kind] = $plan
}

$plans.runtime['bin\VT7Pty.dll'] = Join-Path $binaryDirectory 'VT7Pty.dll'
$plans.runtime['bin\VT7Pty-Agent.exe'] = Join-Path $binaryDirectory 'VT7Pty-Agent.exe'
$plans.runtime['docs\COMPATIBILITY.md'] = Join-Path $repositoryRoot 'docs\COMPATIBILITY.md'

foreach ($header in @('vt7pty.h', 'vt7pty_constants.h', 'vt7pty_version.h')) {
    $plans.development["include\$header"] = Join-Path $repositoryRoot "src\include\$header"
}
$plans.development['lib\VT7Pty.lib'] = Join-Path $binaryDirectory 'VT7Pty.lib'
$plans.development['docs\NAMING.md'] = Join-Path $repositoryRoot 'docs\NAMING.md'
$plans.development['docs\VERSIONING.md'] = Join-Path $repositoryRoot 'docs\VERSIONING.md'

foreach ($binary in @('VT7Pty', 'VT7Pty-Agent', 'VT7Pty-DebugServer')) {
    $plans.symbols["symbols\$binary.pdb"] = Join-Path $binaryDirectory "$binary.pdb"
}
foreach ($testProgram in $testPrograms) {
    $plans.symbols["symbols\tests\$testProgram.pdb"] = Join-Path $binaryDirectory "$testProgram.pdb"
}
$plans.symbols['docs\DIAGNOSTICS.md'] = Join-Path $repositoryRoot 'docs\DIAGNOSTICS.md'

foreach ($binary in @('VT7Pty.dll', 'VT7Pty-Agent.exe', 'VT7Pty-DebugServer.exe')) {
    $plans.tests["bin\$binary"] = Join-Path $binaryDirectory $binary
}
foreach ($testProgram in $testPrograms) {
    $plans.tests["tests\$testProgram.exe"] = Join-Path $binaryDirectory "$testProgram.exe"
}
$plans.tests['RUN-WINDOWS7-ACCEPTANCE.cmd'] = Join-Path $repositoryRoot 'RUN-WINDOWS7-ACCEPTANCE.cmd'
$plans.tests['tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1'] =
    Join-Path $repositoryRoot 'tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1'
$plans.tests['tools\platform\Invoke-Windows7Acceptance.ps1'] =
    Join-Path $repositoryRoot 'tools\platform\Invoke-Windows7Acceptance.ps1'
foreach ($doc in @('COMPATIBILITY', 'DIAGNOSTICS', 'SECURITY_REVIEW', 'TESTING',
        'WINDOWS7_ACCEPTANCE', 'SSH_BASELINE', 'RELEASING')) {
    $plans.tests["docs\$doc.md"] = Join-Path $repositoryRoot "docs\$doc.md"
}
foreach ($name in @('verification.json', 'tests.json', 'tests.txt',
        'VT7Pty.dll.exports.txt', 'VT7Pty.dll.headers-imports.txt',
        'VT7Pty-Agent.exe.headers-imports.txt',
        'VT7Pty-DebugServer.exe.headers-imports.txt',
        'VT7Pty.dll.manifest.xml', 'VT7Pty-Agent.exe.manifest.xml',
        'VT7Pty-DebugServer.exe.manifest.xml')) {
    $plans.tests["verification\$name"] = Join-Path $verificationDirectory $name
}

[IO.Directory]::CreateDirectory($packageRoot) | Out-Null
[IO.Directory]::CreateDirectory($stagingRoot) | Out-Null
[IO.Directory]::CreateDirectory($verificationRoot) | Out-Null
$archiveRecords = @()
foreach ($kind in $plans.Keys) {
    $name = "$packageStem-$kind.zip"
    $stagingDirectory = Join-Path $stagingRoot "$packageStem-$kind"
    $inspectionDirectory = Join-Path $verificationRoot "$packageStem-$kind"
    $archivePath = Join-Path $packageRoot $name
    foreach ($path in @($stagingDirectory, $inspectionDirectory, $archivePath)) {
        Assert-ChildPath -Parent $artifactRoot -Child $path
    }
    foreach ($directory in @($stagingDirectory, $inspectionDirectory)) {
        if (Test-Path -LiteralPath $directory) {
            Remove-Item -LiteralPath $directory -Recurse -Force
        }
        [IO.Directory]::CreateDirectory($directory) | Out-Null
    }
    $files = @()
    foreach ($relativePath in @($plans[$kind].Keys | Sort-Object)) {
        $sourcePath = $plans[$kind][$relativePath]
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Required $kind package input is missing: $sourcePath"
        }
        $destinationPath = Join-Path $stagingDirectory $relativePath
        [IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath)) | Out-Null
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
        $file = Get-Item -LiteralPath $destinationPath
        $files += [ordered]@{
            Path = $relativePath.Replace('\', '/')
            Bytes = $file.Length
            Sha256 = (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $manifest = [ordered]@{
        SchemaVersion = 2
        Product = 'VT7Pty'
        Version = $version
        ApiVersion = $apiVersion
        ProtocolVersion = $protocolVersion
        Configuration = 'Release'
        Architecture = 'x64'
        MinimumOperatingSystem = 'Windows 7 SP1'
        PackageKind = $kind
        PackageSuffix = $PackageSuffix
        SourceCommit = $sourceCommit
        SourceTreeClean = $true
        SourceTreeChanges = @()
        Toolchain = $toolchain
        Verification = [ordered]@{
            SourceCommit = $sourceCommit
            TestCount = @($verification.Tests).Count
            RecordSha256 = $verificationHash
            CleanReleaseRebuild = $true
            StaticAnalysisRebuild = $true
        }
        Files = $files
    }
    $manifestPath = Join-Path $stagingDirectory 'manifest.json'
    $manifest | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Compress-Archive -Path (Join-Path $stagingDirectory '*') `
        -DestinationPath $archivePath -CompressionLevel Optimal

    Expand-Archive -LiteralPath $archivePath -DestinationPath $inspectionDirectory
    $extractedManifest = Get-Content -LiteralPath (Join-Path $inspectionDirectory 'manifest.json') -Raw |
        ConvertFrom-Json
    if ($extractedManifest.PackageKind -ne $kind -or
            $extractedManifest.SourceCommit -ne $sourceCommit -or
            @($extractedManifest.Files).Count -ne $files.Count) {
        throw "The extracted $kind archive has an unexpected manifest."
    }
    foreach ($entry in $extractedManifest.Files) {
        $extractedPath = Join-Path $inspectionDirectory $entry.Path.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $extractedPath -PathType Leaf) -or
                (Get-FileHash -LiteralPath $extractedPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Sha256) {
            throw "The extracted $kind archive failed integrity verification: $($entry.Path)"
        }
    }
    $actualCount = @(Get-ChildItem -LiteralPath $inspectionDirectory -File -Recurse).Count
    if ($actualCount -ne $files.Count + 1) {
        throw "The extracted $kind archive contains unexpected files."
    }
    $archiveRecords += [ordered]@{
        Kind = $kind
        Name = $name
        Bytes = (Get-Item -LiteralPath $archivePath).Length
        Sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        ManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        FileCount = $files.Count
    }
    Remove-Item -LiteralPath $inspectionDirectory -Recurse -Force
    Write-Host "Verified $kind archive: $archivePath"
}

$releaseSetPath = Join-Path $packageRoot "$packageStem-release-set.json"
$checksumPath = Join-Path $packageRoot "$packageStem.sha256"
foreach ($path in @($releaseSetPath, $checksumPath)) {
    Assert-ChildPath -Parent $artifactRoot -Child $path
}
$releaseSet = [ordered]@{
    SchemaVersion = 1
    Product = 'VT7Pty'
    Version = $version
    ApiVersion = $apiVersion
    ProtocolVersion = $protocolVersion
    SourceCommit = $sourceCommit
    SourceTreeClean = $true
    Configuration = 'Release'
    Architecture = 'x64'
    MinimumOperatingSystem = 'Windows 7 SP1'
    PackageSuffix = $PackageSuffix
    Toolchain = $toolchain
    Verification = [ordered]@{
        SourceCommit = $sourceCommit
        TestCount = @($verification.Tests).Count
        RecordSha256 = $verificationHash
        CleanReleaseRebuild = $true
        StaticAnalysisRebuild = $true
    }
    Archives = $archiveRecords
}
$releaseSet | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $releaseSetPath -Encoding UTF8
$checksumLines = @(
    foreach ($archive in $archiveRecords) {
        "$($archive.Sha256)  $($archive.Name)"
    }
    "$((Get-FileHash -LiteralPath $releaseSetPath -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($releaseSetPath))"
)
[IO.File]::WriteAllText($checksumPath,
    (($checksumLines -join "`r`n") + "`r`n"), $utf8NoBom)
Write-Host "Release set: $releaseSetPath"
Write-Host "Checksums: $checksumPath"
