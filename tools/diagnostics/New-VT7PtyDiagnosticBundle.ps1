[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$LogPath,

    [string]$PackageDirectory,

    [string]$OutputDirectory = (Join-Path (Get-Location) (
        'VT7Pty-diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),

    [switch]$NoArchive
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path (Split-Path -Parent $scriptDirectory) 'platform\Windows7Compat.ps1')
if (-not $PackageDirectory) {
    $PackageDirectory = Split-Path -Parent (Split-Path -Parent $scriptDirectory)
}
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$packageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$outputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$logPath = [IO.Path]::GetFullPath($LogPath)

if (Test-Path -LiteralPath $outputDirectory) {
    throw "Output directory already exists: $outputDirectory"
}
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$manifestPath = Join-Path $packageDirectory 'manifest.json'
$manifest = if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    Read-VT7Json $manifestPath
} else {
    $null
}

$logCopy = Join-Path $outputDirectory 'vt7pty-diagnostics.jsonl'
Copy-Item -LiteralPath $logPath -Destination $logCopy
$records = @()
$invalidLineCount = 0
foreach ($line in @(Get-Content -LiteralPath $logCopy)) {
    if ($null -eq $line -or $line.Trim().Length -eq 0) { continue }
    try {
        $records += $script:vt7Json.DeserializeObject($line)
    } catch {
        $invalidLineCount++
    }
}

$inputPatterns = @(
    '^input chars:', '^keypress:', '^mouse input:', '^mouse event:'
)
$containsOptInInput = @($records | Where-Object {
    $message = [string]$_.message
    @($inputPatterns | Where-Object { $message -match $_ }).Count -ne 0
}).Count -ne 0

$windowsKey = Get-ItemProperty -LiteralPath `
    'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$servicePackProperty = $windowsKey.PSObject.Properties['CSDVersion']
$servicePack = if ($null -eq $servicePackProperty) {
    $null
} else {
    $servicePackProperty.Value
}
$hotFixes = @()
try {
    $hotFixes = @(Get-HotFix | Sort-Object HotFixID | ForEach-Object {
        @{ HotFixId = $_.HotFixID; InstalledOn = [string]$_.InstalledOn }
    })
} catch {
    $hotFixes = @(@{ CollectionError = $_.Exception.Message })
}

$binaryRecords = @()
$binDirectory = Join-Path $packageDirectory 'bin'
if (Test-Path -LiteralPath $binDirectory -PathType Container) {
    foreach ($file in @(Get-ChildItem -LiteralPath $binDirectory |
            Where-Object { -not $_.PSIsContainer } | Sort-Object Name)) {
        $binaryRecords += @{
            Name = $file.Name
            Bytes = $file.Length
            Sha256 = [string](Get-VT7Sha256 $file.FullName)
            FileVersion = $file.VersionInfo.FileVersion
            ProductVersion = $file.VersionInfo.ProductVersion
        }
    }
}

$identities = @($records | ForEach-Object {
    "$($_.version)|$($_.commit)|$($_.api)|$($_.protocol)"
} | Sort-Object -Unique)
$severityCounts = @{}
$subsystemCounts = @{}
foreach ($entry in $records) {
    $severity = [string]$entry['severity']
    $subsystem = [string]$entry['subsystem']
    if ($severityCounts.ContainsKey($severity)) {
        $severityCounts[$severity]++
    } else {
        $severityCounts[$severity] = 1
    }
    if ($subsystemCounts.ContainsKey($subsystem)) {
        $subsystemCounts[$subsystem]++
    } else {
        $subsystemCounts[$subsystem] = 1
    }
}

$bundle = @{
    SchemaVersion = 1
    CollectedUtc = (Get-Date).ToUniversalTime().ToString('o')
    Package = if ($null -eq $manifest) { $null } else { @{
        Product = $manifest.Product
        Version = $manifest.Version
        ApiVersion = $manifest.ApiVersion
        ProtocolVersion = $manifest.ProtocolVersion
        SourceCommit = $manifest.SourceCommit
        SourceTreeClean = $manifest.SourceTreeClean
    }}
    Host = @{
        ComputerName = $env:COMPUTERNAME
        ProductName = $windowsKey.ProductName
        CurrentVersion = $windowsKey.CurrentVersion
        BuildNumber = $windowsKey.CurrentBuildNumber
        ServicePack = $servicePack
        Architecture = $env:PROCESSOR_ARCHITECTURE
        PowerShellVersion = $PSVersionTable.PSVersion.ToString()
    }
    Diagnostics = @{
        RecordCount = $records.Count
        InvalidLineCount = $invalidLineCount
        BuildIdentities = [string[]]$identities
        SeverityCounts = $severityCounts
        SubsystemCounts = $subsystemCounts
        ContainsOptInInputRecords = $containsOptInInput
        LogBytes = (Get-Item -LiteralPath $logCopy).Length
        LogSha256 = [string](Get-VT7Sha256 $logCopy)
    }
    Binaries = $binaryRecords
    HotFixes = $hotFixes
}
$bundlePath = Join-Path $outputDirectory 'diagnostic-manifest.json'
Write-VT7Json $bundlePath $bundle

$readme = @"
VT7Pty diagnostic bundle

Review vt7pty-diagnostics.jsonl before sharing it. Default trace output excludes
command lines, environment values, pipe capability names, handles, and terminal
input. ContainsOptInInputRecords is $containsOptInInput. If it is True, the
explicit VT7PTY_DEBUG=input option captured keyboard or mouse details.
"@
[IO.File]::WriteAllText(
    (Join-Path $outputDirectory 'README.txt'), $readme, $utf8NoBom)

if (-not $NoArchive) {
    if ($PSVersionTable.PSVersion.Major -lt 5) {
        throw 'Use -NoArchive on Windows PowerShell 2.0; copy the diagnostic directory.'
    }
    $archivePath = $outputDirectory + '.zip'
    if (Test-Path -LiteralPath $archivePath) {
        throw "Archive already exists: $archivePath"
    }
    Compress-Archive -LiteralPath $outputDirectory -DestinationPath $archivePath `
        -CompressionLevel Optimal
    Write-Host "Diagnostic archive: $archivePath"
}
Write-Host "Diagnostic directory: $outputDirectory"
Write-Host "Records: $($records.Count); invalid lines: $invalidLineCount"
if ($containsOptInInput) {
    Write-Warning 'The log contains opt-in input records. Review it before sharing.'
}
