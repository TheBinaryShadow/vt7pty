[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$PackageRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here '..\platform\Windows7Compat.ps1')
. (Join-Path $here 'OutputEvidence.ps1')
. (Join-Path $here 'ScreenEvidence.ps1')

$Directory = [IO.Path]::GetFullPath($Directory).TrimEnd('\')
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
$package = Read-ScreenPackage $PackageRoot
$inventory = Read-VT7Json (Join-Path $Directory 'evidence-files.json')
$names = @{}
foreach ($file in $inventory.Files) {
    if ($file.Path -notmatch '^[a-zA-Z0-9_.-]+(/[a-zA-Z0-9_.-]+)*$' -or
        $file.Path -match '(^|/)\.\.?(/|$)' -or $names.ContainsKey($file.Path)) { throw 'Invalid evidence path.' }
    $path = Join-Path $Directory $file.Path
    if ((Get-Item -LiteralPath $path).Length -ne $file.Bytes -or (Get-VT7Sha256 $path) -cne $file.Sha256) {
        throw ('Evidence hash differs: ' + $file.Path)
    }
    $names[$file.Path] = $true
}
if ($inventory.SchemaVersion -ne 1 -or -not $names.ContainsKey('report.json') -or
    @(Get-ChildItem -LiteralPath $Directory -Recurse | Where-Object { -not $_.PSIsContainer }).Count -ne $names.Count + 1) {
    throw 'Incomplete evidence inventory.'
}

$run = Read-VT7Json (Join-Path $Directory 'report.json')
if ($run.SchemaVersion -ne 1 -or $run.Kind -cne 'VT7PtyScreenResults' -or
    @('DevelopmentHost','NonESU','ESU','Legacy') -cnotcontains $run.Tier -or
    $run.ManifestSha256 -cne (Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json')) -or
    $run.Harness.SourceCommit -cne $package.Harness.SourceCommit -or $run.SessionFlags -ne $package.SessionFlags -or
    -not $run.CharacterizationComplete -or $run.Step14Accepted -or $run.MilestoneAccepted -or
    @($run.PreflightErrors).Count -ne 0 -or $run.Oracle.ExitCode -ne 0 -or $run.Oracle.TimedOut -or
    $run.Oracle.StandardOutput -cne 'VT7Pty VT state oracle tests passed.' -or @($run.Cases).Count -ne 2 * $package.Cases.Count) {
    throw 'Incomplete screen results or wrong package identity.'
}
if ($run.Tier -cne 'DevelopmentHost' -and
    ($run.OS.Version -cnotlike '6.1.*' -or $run.OS.ServicePackMajorVersion -ne 1 -or $run.OS.ProductType -ne 1)) {
    throw 'Target results require Windows 7 SP1.'
}
if ($run.Tier -ceq 'Legacy' -and ($run.PowerShellVersion -cne '2.0' -or $run.CLRVersion -cnotlike '2.0.*' -or
    @($run.Updates | Where-Object { $_.Id -ceq 'KB3191566' }).Count)) { throw 'Wrong legacy engine or update state.' }

foreach ($role in @('baseline','accepted')) {
    $original = $package.Runtimes[$role]
    $actual = $run.Runtimes[$role]
    if ($actual.SourceCommit -cne $original.SourceCommit -or $actual.ManifestSha256 -cne $original.ManifestSha256 -or
        $actual.ArchiveSha256 -cne $original.ArchiveSha256) { throw 'Runtime observation provenance differs.' }
    foreach ($spec in $package.Cases) {
        $cases = @($run.Cases | Where-Object { $_.Runtime -ceq $role -and $_.Name -ceq $spec.Name })
        $relative = $role + '/' + $spec.Name
        if ($cases.Count -ne 1 -or $cases[0].Area -cne $spec.Area -or $cases[0].Directory -cne $relative -or
            $cases[0].Status -cne 'Observed' -or $cases[0].Controller.TimedOut) { throw ('Invalid case inventory: ' + $relative) }
        $record = Read-ScreenCase (Join-Path $Directory $relative) $spec $cases[0].Controller.ExitCode $package
        $outcome = if ($record.Status -ceq 'Pass') { 'Match' } else { 'Difference' }
        if ($cases[0].Outcome -cne $outcome) { throw ('Aggregate outcome differs: ' + $relative) }
    }
}
$matches = @($run.Cases | Where-Object { $_.Outcome -ceq 'Match' }).Count
Write-Host ('Screen evidence verified on ' + $run.ComputerName + ' (' + $run.Tier + '). Matches ' + $matches +
    ', differences ' + ($run.Cases.Count - $matches) + '.')
