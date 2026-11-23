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

$Directory = [IO.Path]::GetFullPath($Directory).TrimEnd('\')
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
$package = Read-OutputPackage $PackageRoot
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
    @((Get-ChildItem -LiteralPath $Directory -Recurse -File)).Count -ne $names.Count + 1) { throw 'Incomplete evidence inventory.' }

$run = Read-VT7Json (Join-Path $Directory 'report.json')
if ($run.SchemaVersion -ne 1 -or $run.Kind -cne 'VT7PtyOutputResults' -or
    $run.ManifestSha256 -cne (Get-VT7Sha256 (Join-Path $PackageRoot 'manifest.json')) -or
    $run.Harness.SourceCommit -cne $package.Harness.SourceCommit -or $run.SessionFlags -ne $package.SessionFlags -or
    -not $run.CharacterizationComplete -or $run.Step13Accepted -or $run.MilestoneAccepted -or
    @($run.PreflightErrors).Count -ne 0 -or $run.Oracle.ExitCode -ne 0 -or $run.Oracle.TimedOut -or
    $run.Oracle.StandardOutput -cne 'VT7Pty VT state oracle tests passed.' -or @($run.Cases).Count -ne 2 * $package.Cases.Count) {
    throw 'Output results do not match this package or required collection.'
}
if ($run.Tier -cne 'DevelopmentHost' -and
    ($run.OS.Version -cnotlike '6.1.*' -or $run.OS.ServicePackMajorVersion -ne 1 -or $run.OS.ProductType -ne 1)) {
    throw 'Target results require Windows 7 SP1.'
}
if ($run.Tier -ceq 'Legacy' -and ($run.PowerShellVersion -cne '2.0' -or $run.CLRVersion -cnotlike '2.0.*' -or
    @($run.Updates | Where-Object { $_.Id -ceq 'KB3191566' }).Count)) { throw 'Wrong legacy engine or update state.' }

$utf8 = New-Object Text.UTF8Encoding($false,$true)
$observations = @{}
foreach ($role in @('baseline','accepted')) {
    $original = $package.Runtimes[$role]
    $actual = $run.Runtimes[$role]
    if ($actual.SourceCommit -cne $original.SourceCommit -or $actual.ManifestSha256 -cne $original.ManifestSha256 -or
        $actual.ArchiveSha256 -cne $original.ArchiveSha256) { throw 'Runtime observation provenance differs.' }
    foreach ($spec in $package.Cases) {
        $reported = @($run.Cases | Where-Object { $_.Runtime -ceq $role -and $_.Name -ceq $spec.Name })
        $relative = $role + '/' + $spec.Name
        if ($reported.Count -ne 1 -or $reported[0].Area -cne $spec.Area -or $reported[0].Directory -cne $relative -or
            $reported[0].Status -cne 'Observed' -or $reported[0].Controller.TimedOut) { throw ('Invalid case inventory: ' + $relative) }
        foreach ($name in @('case.json','stream.bin','console-1.bin','console-2.bin')) {
            if (-not $names.ContainsKey($relative + '/' + $name)) { throw ('Missing recorded observation: ' + $relative + '/' + $name) }
        }
        $caseDirectory = Join-Path $Directory $relative
        $native = Read-OutputCase $caseDirectory $spec $reported[0].Controller.ExitCode $package.Harness.SnapshotBytes
        $outcome = if ($native.Status -ceq 'Fail') { 'Difference' }
            elseif ($native.CapabilityUnavailable) { 'CapabilityUnavailable' } else { 'Match' }
        if ($reported[0].Outcome -cne $outcome) { throw ('Aggregate outcome differs: ' + $relative) }

        $stream = [IO.File]::ReadAllBytes((Join-Path $caseDirectory 'stream.bin'))
        $valid = $true
        try { [void]$utf8.GetCharCount($stream) } catch { $valid = $false }
        if ($valid -ne $native.CompleteStreamValidUtf8) { throw ('UTF-8 status differs: ' + $relative) }
        foreach ($checkpoint in $native.OutputCheckpoints) {
            $snapshot = [IO.File]::ReadAllBytes((Join-Path $caseDirectory ('console-' + $checkpoint.Phase + '.bin')))
            if ([BitConverter]::ToUInt32($snapshot, 0) -ne 0x31465456 -or [BitConverter]::ToUInt32($snapshot, 4) -ne 1) {
                throw ('Invalid snapshot magic or schema: ' + $relative)
            }
            $fields = @('Columns','Rows','BufferColumns','BufferRows','WindowLeft','WindowTop','CursorX','CursorY')
            for ($i = 0; $i -lt $fields.Count; $i++) {
                if ([BitConverter]::ToUInt32($snapshot, 8 + 4 * $i) -ne $checkpoint.Console[$fields[$i]]) {
                    throw ('Snapshot geometry differs: ' + $relative)
                }
            }
            if ([BitConverter]::ToUInt32($snapshot, 48) -ne $checkpoint.Console.OutputMode -or
                [BitConverter]::ToUInt32($snapshot, 56) -ne $checkpoint.Console.OutputCP) {
                throw ('Snapshot output mode or code page differs: ' + $relative)
            }
        }
        $observations[$relative] = @{
            Outcome = $outcome; BackingConsoleDiffers = $native.BackingConsoleDiffers
            ReconstructionDiffers = $native.ReconstructionDiffers; CapabilityUnavailable = $native.CapabilityUnavailable
        }
    }
}
$pairs = @($package.Cases | ForEach-Object {
    @{Name = $_.Name; Area = $_.Area; Baseline = $observations['baseline/' + $_.Name]
        Accepted = $observations['accepted/' + $_.Name]}
})
@{Kind = 'OutputCharacterizationReview'; Review = 'Complete'; RunId = $run.RunId
    Machine = $run.ComputerName; Tier = $run.Tier; CasePairs = $pairs; RawFiles = $names.Count + 1
    ManifestSha256 = $run.ManifestSha256; Step13Accepted = $false}
