[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Directory,[Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
function Assert-M([bool]$Condition,[string]$Message){if(-not $Condition){throw ('H4 manual evidence: '+$Message)}}
function Read-M([string]$Path){[IO.File]::ReadAllText($Path) | ConvertFrom-Json}
function Files-M([string]$Root,$Files){
    $seen=@{}
    foreach($f in $Files){
        $path=[IO.Path]::GetFullPath((Join-Path $Root $f.Path))
        Assert-M ($path.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $seen.ContainsKey($path)) 'contained unique file'
        $seen[$path]=$true
        Assert-M ((Get-Item -LiteralPath $path).Length -eq $f.Bytes -and
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $f.Sha256) ('hash '+$f.Path)
    }
}
$manifest=Read-M (Join-Path $PackageRoot 'manifest.json')
$report=Read-M (Join-Path $Directory 'report.json')
Assert-M ($manifest.Kind -ceq 'VT7PtyH4RuntimeCandidate' -and $manifest.Qualification.Revision -eq 1 -and
    $report.Kind -ceq 'VT7PtyH4ManualResults' -and $report.SchemaVersion -eq 1 -and
    $report.SourceCommit -ceq $manifest.Runtime.SourceCommit -and
    $report.ManifestSha256 -ceq (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -and
    -not $report.H4Accepted -and -not $report.ManualQualificationAccepted) 'matched candidate collection'
Files-M $PackageRoot $manifest.Files
$files=(Read-M (Join-Path $Directory 'evidence-files.json')).Files
Files-M $Directory $files
Assert-M (@(Get-ChildItem -LiteralPath $Directory -File -Recurse).Count -eq $files.Count+1) 'complete file inventory'
$requested=@($report.RequestedChecks)
Assert-M (($requested -join ',') -in @('US,HR,Session','US','HR','Session','Elevated') -and
    (@($report.Steps.Check) -join ',') -ceq ($requested -join ',')) 'complete requested manual profile'
$reviews=@()
foreach($step in $report.Steps){
    Assert-M ($step.Status -ceq 'Pass' -and $step.CollectionStatus -ceq 'Completed' -and $step.ExitCode -eq 0 -and
        -not $step.Error -and $step.Directory -ceq $step.Check.ToLowerInvariant()) 'completed manual check'
    $native=Join-Path $Directory $step.Directory
    $probe=Read-M (Join-Path $native 'probe.json')
    $kind=if($step.Check -eq 'Session'){'H4ManualSession'}elseif($step.Check -eq 'Elevated'){'H4ManualAccess'}else{'H4ManualKeyboard'}
    Assert-M ($probe.Kind -ceq $kind) 'native check identity'
    if($kind -eq 'H4ManualKeyboard'){Assert-M ($probe.DeclaredLayout -ceq $step.Check) 'native physical layout'}
    $reviews+=@(& (Join-Path $PSScriptRoot 'Test-H4QualificationEvidence.ps1') -Directory $native -ExpectedSource $manifest.Runtime.SourceCommit)
}
[ordered]@{Kind='H4ManualBundleReview';Status='Pass';ComputerName=$report.ComputerName;Checks=$requested;Reviews=$reviews;H4Accepted=$false}
