[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',

    [ValidateSet('Build', 'Rebuild', 'Clean')]
    [string]$Target = 'Build',

    [switch]$DetailedOutput
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRoot = $PSScriptRoot
$solutionPath = Join-Path $repositoryRoot 'VT7Pty.sln'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) {
    throw "Visual Studio Installer discovery tool was not found at $vswherePath"
}

$installationPath = (& $vswherePath -latest -products * -version '[17.0,18.0)' `
    -requires Microsoft.Component.MSBuild `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
if (-not $installationPath) {
    throw 'Visual Studio 2022 with MSBuild and the MSVC x64 tools was not found. Import .vsconfig in Visual Studio Installer.'
}

$msbuildPath = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuildPath)) {
    throw "MSBuild was not found at $msbuildPath"
}

$configurations = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
$verbosity = if ($DetailedOutput) { 'normal' } else { 'minimal' }
$legacyArtifactNames = @(
    'winpty.dll', 'winpty.exp', 'winpty.lib', 'winpty.pdb',
    'winpty-agent.exe', 'winpty-agent.pdb',
    'winpty-debugserver.exe', 'winpty-debugserver.pdb',
    'trivial_test.exe', 'trivial_test.pdb'
)

foreach ($configurationName in $configurations) {
    $binaryDirectory = Join-Path $repositoryRoot "artifacts\bin\x64\$configurationName"
    foreach ($legacyArtifactName in $legacyArtifactNames) {
        $legacyArtifactPath = Join-Path $binaryDirectory $legacyArtifactName
        if (Test-Path -LiteralPath $legacyArtifactPath -PathType Leaf) {
            Remove-Item -LiteralPath $legacyArtifactPath -Force
        }
    }

    Write-Host "Building VT7Pty native targets: $configurationName|x64"
    $arguments = @(
        $solutionPath,
        '/nologo',
        '/m',
        "/t:$Target",
        "/p:Configuration=$configurationName",
        '/p:Platform=x64',
        '/p:PreferredToolArchitecture=x64',
        "/verbosity:$verbosity"
    )
    & $msbuildPath @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed for $configurationName|x64 with exit code $LASTEXITCODE."
    }
}
