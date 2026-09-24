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
    (Join-Path $stagingDirectory 'docs'),
    (Join-Path $stagingDirectory 'tools\diagnostics'),
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
    'tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1' = (Join-Path $repositoryRoot 'tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1')
    'tools\platform\Invoke-Windows7Acceptance.ps1' = (Join-Path $repositoryRoot 'tools\platform\Invoke-Windows7Acceptance.ps1')
    'docs\DIAGNOSTICS.md' = (Join-Path $repositoryRoot 'docs\DIAGNOSTICS.md')
    'docs\SECURITY_REVIEW.md' = (Join-Path $repositoryRoot 'docs\SECURITY_REVIEW.md')
    'docs\TESTING.md' = (Join-Path $repositoryRoot 'docs\TESTING.md')
    'docs\WINDOWS7_ACCEPTANCE.md' = (Join-Path $repositoryRoot 'docs\WINDOWS7_ACCEPTANCE.md')
    'docs\SSH_BASELINE.md' = (Join-Path $repositoryRoot 'docs\SSH_BASELINE.md')
    'LICENSE.txt' = (Join-Path $repositoryRoot 'LICENSE')
    'CREDITS.md' = (Join-Path $repositoryRoot 'CREDITS.md')
    'UPSTREAM.md' = (Join-Path $repositoryRoot 'UPSTREAM.md')
}
$testPrograms = @(
    'ModernCppTest', 'ProtocolTest', 'ProtocolTestAgent', 'BackendSmokeTest',
    'SessionContractTest', 'SessionFixture',
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
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = (& $vswherePath -latest -products * -version '[17.0,18.0)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
if (-not $installationPath) {
    throw 'Could not identify the Visual Studio C++ toolchain used for packaging.'
}
$msvcDirectory = Get-ChildItem -LiteralPath (Join-Path $installationPath 'VC\Tools\MSVC') -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$compilerPath = Join-Path $msvcDirectory.FullName 'bin\Hostx64\x64\cl.exe'
$sdkSettings = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'Directory.Build.props'))
$sdkMatch = [regex]::Match($sdkSettings, '<WindowsTargetPlatformVersion>(?<value>[^<]+)</WindowsTargetPlatformVersion>')
if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf) -or -not $sdkMatch.Success) {
    throw 'Could not identify the compiler or Windows SDK version.'
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
    Toolchain = [ordered]@{
        VisualStudio = '2022'
        MSVCDirectoryVersion = $msvcDirectory.Name
        CompilerFileVersion = (Get-Item -LiteralPath $compilerPath).VersionInfo.FileVersion
        WindowsSDKVersion = $sdkMatch.Groups['value'].Value
        PowerShellVersion = $PSVersionTable.PSVersion.ToString()
    }
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
