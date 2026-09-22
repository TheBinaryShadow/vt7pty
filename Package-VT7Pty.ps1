[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$NoVerify,

    [ValidatePattern('^[a-z0-9][a-z0-9.-]*$')]
    [string]$PackageSuffix
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = $PSScriptRoot
$artifactRoot = Join-Path $repositoryRoot 'artifacts'
$binaryDirectory = Join-Path $artifactRoot "bin\x64\$Configuration"
$packageRoot = Join-Path $artifactRoot 'packages'
$stagingRoot = Join-Path $artifactRoot 'package-staging'
$version = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'VERSION.txt')).Trim()
$apiVersionHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\include\vt7pty_version.h'))
$protocolHeader = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'src\shared\Protocol.h'))
$apiMajorMatch = [regex]::Match($apiVersionHeader, '(?m)^#define VT7PTY_API_VERSION_MAJOR (?<value>\d+)\r?$')
$apiMinorMatch = [regex]::Match($apiVersionHeader, '(?m)^#define VT7PTY_API_VERSION_MINOR (?<value>\d+)\r?$')
$protocolMatch = [regex]::Match(
    $protocolHeader,
    '(?m)^constexpr int32_t VT7PTY_PROTOCOL_VERSION = (?<value>\d+);\r?$')
if (-not $apiMajorMatch.Success -or -not $apiMinorMatch.Success -or
        -not $protocolMatch.Success) {
    throw 'Could not read the VT7Pty API or protocol version.'
}
$apiVersion = "$($apiMajorMatch.Groups['value'].Value).$($apiMinorMatch.Groups['value'].Value)"
$protocolVersion = [int]$protocolMatch.Groups['value'].Value
$packageName = "VT7Pty-$version-win7-x64-$($Configuration.ToLowerInvariant())"
if ($PackageSuffix) {
    $packageName += "-$PackageSuffix"
}
$stagingDirectory = Join-Path $stagingRoot $packageName
$archivePath = Join-Path $packageRoot ($packageName + '.zip')
$checksumPath = $archivePath + '.sha256'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Assert-ChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Parent,
        [Parameter(Mandatory = $true)][string]$Child
    )
    $resolvedParent = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    $resolvedChild = [IO.Path]::GetFullPath($Child)
    if (-not $resolvedChild.StartsWith($resolvedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify path outside $resolvedParent`: $resolvedChild"
    }
}

if (-not $NoVerify) {
    & (Join-Path $repositoryRoot 'Verify-VT7Pty.ps1') -Configuration $Configuration
}

Assert-ChildPath -Parent $artifactRoot -Child $stagingDirectory
Assert-ChildPath -Parent $artifactRoot -Child $archivePath
Assert-ChildPath -Parent $artifactRoot -Child $checksumPath
if (Test-Path -LiteralPath $stagingDirectory) {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
if (Test-Path -LiteralPath $checksumPath) {
    Remove-Item -LiteralPath $checksumPath -Force
}

$directories = @(
    $stagingDirectory,
    (Join-Path $stagingDirectory 'bin'),
    (Join-Path $stagingDirectory 'include'),
    (Join-Path $stagingDirectory 'lib'),
    (Join-Path $stagingDirectory 'symbols'),
    (Join-Path $stagingDirectory 'symbols\tests'),
    (Join-Path $stagingDirectory 'tests'),
    (Join-Path $stagingDirectory 'tools\platform'),
    $packageRoot
)
foreach ($directory in $directories) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}

$copyPlan = [ordered]@{
    'bin\VT7Pty.dll' = (Join-Path $binaryDirectory 'VT7Pty.dll')
    'bin\VT7Pty-Agent.exe' = (Join-Path $binaryDirectory 'VT7Pty-Agent.exe')
    'bin\VT7Pty-DebugServer.exe' = (Join-Path $binaryDirectory 'VT7Pty-DebugServer.exe')
    'include\vt7pty.h' = (Join-Path $repositoryRoot 'src\include\vt7pty.h')
    'include\vt7pty_constants.h' = (Join-Path $repositoryRoot 'src\include\vt7pty_constants.h')
    'include\vt7pty_version.h' = (Join-Path $repositoryRoot 'src\include\vt7pty_version.h')
    'lib\VT7Pty.lib' = (Join-Path $binaryDirectory 'VT7Pty.lib')
    'symbols\VT7Pty.pdb' = (Join-Path $binaryDirectory 'VT7Pty.pdb')
    'symbols\VT7Pty-Agent.pdb' = (Join-Path $binaryDirectory 'VT7Pty-Agent.pdb')
    'symbols\VT7Pty-DebugServer.pdb' = (Join-Path $binaryDirectory 'VT7Pty-DebugServer.pdb')
    'RUN-WINDOWS7-ACCEPTANCE.cmd' = (Join-Path $repositoryRoot 'RUN-WINDOWS7-ACCEPTANCE.cmd')
    'tools\platform\Invoke-Windows7Acceptance.ps1' = (Join-Path $repositoryRoot 'tools\platform\Invoke-Windows7Acceptance.ps1')
    'LICENSE.txt' = (Join-Path $repositoryRoot 'LICENSE')
    'CREDITS.md' = (Join-Path $repositoryRoot 'CREDITS.md')
    'UPSTREAM.md' = (Join-Path $repositoryRoot 'UPSTREAM.md')
}
$testPrograms = @(
    'ModernCppTest', 'ProtocolTest', 'ProtocolTestAgent', 'BackendSmokeTest',
    'fixture-console-color-grid', 'fixture-output-lines', 'fixture-show-argv',
    'fixture-show-console-input', 'fixture-utf16-echo', 'fixture-win32-echo1',
    'fixture-win32-echo2', 'fixture-win32-write1', 'fixture-write-console',
    'tool-conin-mode', 'tool-conout-mode'
)
foreach ($testProgram in $testPrograms) {
    $copyPlan["tests\$testProgram.exe"] = Join-Path $binaryDirectory "$testProgram.exe"
    $copyPlan["symbols\tests\$testProgram.pdb"] = Join-Path $binaryDirectory "$testProgram.pdb"
}
foreach ($relativePath in $copyPlan.Keys) {
    $sourcePath = $copyPlan[$relativePath]
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required package input does not exist: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $stagingDirectory $relativePath)
}

$sourceCommit = (& git -C $repositoryRoot rev-parse --verify HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Could not resolve source commit: $sourceCommit"
}
$sourceChanges = @(& git -C $repositoryRoot status --porcelain=v1 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect source state: $($sourceChanges -join [Environment]::NewLine)"
}
$files = @(
    Get-ChildItem -LiteralPath $stagingDirectory -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            [ordered]@{
                Path = $_.FullName.Substring($stagingDirectory.Length + 1).Replace('\', '/')
                Bytes = $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
)
$manifest = [ordered]@{
    SchemaVersion = 1
    Product = 'VT7Pty'
    Version = $version
    ApiVersion = $apiVersion
    ProtocolVersion = $protocolVersion
    Configuration = $Configuration
    Architecture = 'x64'
    MinimumOperatingSystem = 'Windows 7 SP1'
    PackageSuffix = $PackageSuffix
    SourceCommit = $sourceCommit
    SourceTreeClean = ($sourceChanges.Count -eq 0)
    SourceTreeChanges = $sourceChanges
    Files = $files
}
$manifest | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $stagingDirectory 'manifest.json') -Encoding UTF8

Compress-Archive -Path (Join-Path $stagingDirectory '*') -DestinationPath $archivePath -CompressionLevel Optimal
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(
    $checksumPath,
    "$archiveHash  $([IO.Path]::GetFileName($archivePath))`r`n",
    $utf8NoBom)

Write-Host "Package created: $archivePath"
Write-Host "Checksum: $checksumPath"
