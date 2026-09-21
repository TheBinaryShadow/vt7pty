[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$NoVerify
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = $PSScriptRoot
$artifactRoot = Join-Path $repositoryRoot 'artifacts'
$binaryDirectory = Join-Path $artifactRoot "bin\x64\$Configuration"
$packageRoot = Join-Path $artifactRoot 'packages'
$stagingRoot = Join-Path $artifactRoot 'package-staging'
$version = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'VERSION.txt')).Trim()
$packageName = "VT7Pty-$version-win7-x64-$($Configuration.ToLowerInvariant())"
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
    $packageRoot
)
foreach ($directory in $directories) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}

$copyPlan = [ordered]@{
    'bin\winpty.dll' = (Join-Path $binaryDirectory 'winpty.dll')
    'bin\winpty-agent.exe' = (Join-Path $binaryDirectory 'winpty-agent.exe')
    'bin\winpty-debugserver.exe' = (Join-Path $binaryDirectory 'winpty-debugserver.exe')
    'include\winpty.h' = (Join-Path $repositoryRoot 'src\include\winpty.h')
    'include\winpty_constants.h' = (Join-Path $repositoryRoot 'src\include\winpty_constants.h')
    'lib\winpty.lib' = (Join-Path $binaryDirectory 'winpty.lib')
    'symbols\winpty.pdb' = (Join-Path $binaryDirectory 'winpty.pdb')
    'symbols\winpty-agent.pdb' = (Join-Path $binaryDirectory 'winpty-agent.pdb')
    'symbols\winpty-debugserver.pdb' = (Join-Path $binaryDirectory 'winpty-debugserver.pdb')
    'LICENSE.txt' = (Join-Path $repositoryRoot 'LICENSE')
    'CREDITS.md' = (Join-Path $repositoryRoot 'CREDITS.md')
    'UPSTREAM.md' = (Join-Path $repositoryRoot 'UPSTREAM.md')
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
    Configuration = $Configuration
    Architecture = 'x64'
    MinimumOperatingSystem = 'Windows 7 SP1'
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
