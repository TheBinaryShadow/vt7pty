[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$repositoryRootPath = [IO.Path]::GetFullPath($RepositoryRoot)
$version = [IO.File]::ReadAllText((Join-Path $repositoryRootPath 'VERSION.txt')).Trim()
if (-not $version) {
    throw 'VERSION.txt is empty.'
}
$commit = (& git -C $repositoryRootPath rev-parse --verify HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) {
    throw "Could not resolve the source commit: $commit"
}

$content = "$version`r`n$commit`r`n"
$resolvedOutputPath = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory((Split-Path -Parent $resolvedOutputPath)) | Out-Null
$encoding = New-Object Text.UTF8Encoding($false)
if (-not (Test-Path -LiteralPath $resolvedOutputPath) -or
        [IO.File]::ReadAllText($resolvedOutputPath) -ne $content) {
    [IO.File]::WriteAllText($resolvedOutputPath, $content, $encoding)
}
