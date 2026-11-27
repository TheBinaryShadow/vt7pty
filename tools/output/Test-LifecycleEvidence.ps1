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
. (Join-Path $here 'LifecycleEvidence.ps1')

$Directory = [IO.Path]::GetFullPath($Directory).TrimEnd('\')
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
$package = Read-LifecyclePackage $PackageRoot
$schedule = @(Get-LifecycleCaseSchedule $package)
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
if ($run.SchemaVersion -ne 1 -or $run.Kind -cne 'VT7PtyLifecycleResults' -or
    @('DevelopmentHost','NonESU','ESU','Legacy') -cnotcontains $run.Tier -or
    $run.ManifestSha256 -cne (Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json')) -or
    $run.Harness.SourceCommit -cne $package.Harness.SourceCommit -or $run.SessionFlags -ne $package.SessionFlags -or
    -not $run.CharacterizationComplete -or $run.Step15Accepted -or $run.MilestoneAccepted -or
    @($run.PreflightErrors).Count -ne 0 -or $run.Oracle.ExitCode -ne 0 -or $run.Oracle.TimedOut -or
    $run.Oracle.StandardOutput -cne 'VT7Pty VT state oracle tests passed.' -or
    $run.Runtimes.Count -ne 2 -or $run.Cases.Count -ne $schedule.Count -or
    $run.Regressions.Count -ne 2 * $package.Regressions.Count) {
    throw 'Incomplete lifecycle results or wrong package identity.'
}
if ($run.Tier -cne 'DevelopmentHost' -and ($run.OS.Version -cnotlike '6.1.*' -or
    $run.OS.ServicePackMajorVersion -ne 1 -or $run.OS.ProductType -ne 1)) {
    throw 'Target results require Windows 7 SP1.'
}
if ($run.Tier -ceq 'Legacy' -and ($run.PowerShellVersion -cne '2.0' -or $run.CLRVersion -cnotlike '2.0.*' -or
    @($run.Updates | Where-Object { $_.Id -ceq 'KB3191566' }).Count)) { throw 'Wrong legacy engine or update state.' }

if ($package.SchemaVersion -ge 2) {
    $expectedOrder = @($schedule | ForEach-Object { $_.Runtime + '/' + $_.Spec.Name })
    $observedOrder = @($run.Cases | ForEach-Object { $_.Runtime + '/' + $_.Name })
    if (($expectedOrder -join ',') -cne ($observedOrder -join ',')) { throw 'Wrong lifecycle runtime order.' }
}
if ($package.SchemaVersion -ge 4) {
    foreach ($field in @('SchemaVersion','SourceCommit','ParentCommit','AgentSha256','Capacity')) {
        if ($run.ResizeTrace[$field] -cne $package.ResizeTrace[$field]) { throw 'Diagnostic resize provenance differs.' }
    }
}

foreach ($item in $schedule) {
    $role = $item.Runtime
    $spec = $item.Spec
    $relative = $role + '/' + $spec.Name
    $cases = @($run.Cases | Where-Object { $_.Runtime -ceq $role -and $_.Name -ceq $spec.Name })
    if ($cases.Count -ne 1 -or $cases[0].Directory -cne $relative -or $cases[0].Area -cne $spec.Area -or
        $cases[0].Status -cne 'Observed' -or $cases[0].Controller.TimedOut) {
        throw ('Invalid lifecycle case inventory: ' + $relative)
    }
    $record = Read-LifecycleCase (Join-Path $Directory $relative) $spec $cases[0].Controller.ExitCode $package $role
    if ($cases[0].Outcome -cne (Get-LifecycleOutcome $record)) { throw ('Lifecycle outcome differs: ' + $relative) }
}

foreach ($role in @('baseline','accepted')) {
    $actual = $run.Runtimes[$role]
    $expected = $package.Runtimes[$role]
    if ($actual.SourceCommit -cne $expected.SourceCommit -or $actual.ManifestSha256 -cne $expected.ManifestSha256 -or
        $actual.ArchiveSha256 -cne $expected.ArchiveSha256) { throw ('Runtime provenance differs: ' + $role) }
    foreach ($name in $package.Regressions) {
        $checks = @($run.Regressions | Where-Object { $_.Runtime -ceq $role -and $_.Name -ceq $name })
        if ($checks.Count -ne 1 -or $checks[0].Error -or -not (Test-LifecycleRegression $checks[0])) {
            throw ('Lifecycle regression failed: ' + $role + '/' + $name)
        }
    }
}
$counts = @($run.Cases | Group-Object { $_.Outcome } | ForEach-Object { $_.Name + ' ' + $_.Count })
Write-Host ('Lifecycle evidence verified on ' + $run.ComputerName + ' (' + $run.Tier + '). ' + ($counts -join ', ') + '.')
