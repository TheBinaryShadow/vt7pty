[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [string]$OutputPath=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
$PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
function Need-C([bool]$Okay,[string]$Message){if(-not $Okay){throw ('Input candidate evidence: '+$Message)}}
function Read-C([string]$Path){[IO.File]::ReadAllText($Path) | ConvertFrom-Json}
function Hash-C([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Files-C([string]$Root,$Files){
    $seen=@{}
    foreach($file in $Files){
        $path=[IO.Path]::GetFullPath((Join-Path $Root $file.Path))
        Need-C ($path.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $seen.ContainsKey($path)) 'unique contained inventory'
        $seen[$path]=$true
        Need-C ((Get-Item -LiteralPath $path).Length -eq $file.Bytes -and (Hash-C $path) -ceq $file.Sha256) ('inventory hash '+$file.Path)
    }
}
$m=Read-C (Join-Path $PackageRoot 'manifest.json');$r=Read-C (Join-Path $Directory 'report.json')
$parent='54b0d23146dfde7528c9e4db1aa92d12e662f883'
$frozenHarness='23254008e421080d762804938ecce1cd1c710106'
$source=$m.Runtime.SourceCommit
Need-C ($m.Kind -ceq 'VT7PtyInputCandidate' -and $m.SchemaVersion -eq 2 -and $r.Kind -ceq 'VT7PtyInputResults' -and
    $r.SchemaVersion -eq 2 -and $r.ManifestSha256 -ceq (Hash-C (Join-Path $PackageRoot 'manifest.json')) -and
    $source -cmatch '^[0-9a-f]{40}$' -and $source -cne $parent -and $m.Runtime.SourceTreeClean -and
    -not $m.Runtime.UnchangedFromH4 -and $m.Runtime.ParentSourceCommit -ceq $parent -and $m.Runtime.ApiVersion -ceq '1.1' -and
    $m.Runtime.ProtocolVersion -eq 2 -and $r.Runtime.SourceCommit -ceq $source -and -not $r.Runtime.UnchangedFromH4 -and
    $m.Harness.SourceCommit -ceq $source -and $r.Harness.SourceCommit -ceq $source -and $m.Harness.SourceTreeClean -and
    $m.Guard.SourceCommit -ceq $source -and $m.Guard.OracleUnchangedFromH4) 'matched changed runtime and harness identity'
Need-C ($m.Reference.Unchanged -and $m.Reference.RuntimeSourceCommit -ceq $parent -and $m.Reference.HarnessSourceCommit -ceq $frozenHarness -and
    $m.Reference.ArchiveSha256 -ceq '84f1bd227af856ec624c88d77c504417dea312bf0e4b3e1df03f28b73c4ab8ac' -and
    $m.Reference.ManifestSha256 -ceq '4edbdacedd8faa1ad888a3dd6ca61d1badd805eef0959d62f4f7b737c1165041' -and
    $r.Reference.ArchiveSha256 -ceq $m.Reference.ArchiveSha256 -and
    (Hash-C (Join-Path $PackageRoot 'reference\manifest.json')) -ceq $m.Reference.ManifestSha256) 'unchanged rc1 reference identity'
Need-C (-not $m.Step12Accepted -and -not $r.Step12Accepted -and -not $r.MilestoneAccepted -and $m.ControlFailuresRetained -and
    $r.CtrlBreakTransport -ceq 'Unsupported' -and $r.NativeBreakIsReferenceOnly -and @($m.KnownDevelopmentFailures).Count -eq 0) 'explicit transport and acceptance limits'
foreach($item in @(@('DistinctCtrlBreakTransport','Unsupported'),@('OrdinaryUserProfiles','NotRun'))){
    foreach($list in @($m.PendingCoverage,$r.PendingCoverage)){
        Need-C (@($list | Where-Object {$_.Capability -ceq $item[0] -and $_.Status -ceq $item[1]}).Count -eq 1) 'retained pending coverage'
    }
}
Need-C ($r.Tier -in @('DevelopmentHost','NonESU','ESU','Legacy') -and @($r.PreflightErrors).Count -eq 0) 'declared tier and complete prerequisites'
if($r.Tier -ne 'DevelopmentHost'){
    Need-C ($r.OS.Version.StartsWith('6.1.') -and $r.OS.ServicePackMajorVersion -eq 1 -and $r.OS.ProductType -eq 1) 'actual Windows 7 SP1 tier'
}
if($r.Tier -ceq 'Legacy'){
    Need-C ($r.PowerShellVersion -ceq '2.0' -and $r.CLRVersion.StartsWith('2.0.') -and
        @($r.Updates | Where-Object {$_.Id -ceq 'KB3191566'}).Count -eq 0 -and
        -not $r.PowerShell7.Required -and -not $r.PowerShell7.Applicable) 'legacy PowerShell 2.0/CLR2 prerequisites'
}else{
    Need-C ($r.PowerShell7.Required -and $r.PowerShell7.Applicable -and $r.PowerShell7.Version -ceq '7.2.24' -and
        $r.PowerShell7.Sha256 -cmatch '^[0-9a-f]{64}$' -and $r.PowerShell7.ModuleManifestSha256 -cmatch '^[0-9a-f]{64}$') 'pinned PowerShell 7 inventory'
}
Files-C $PackageRoot $m.Files
$frozen=Read-C (Join-Path $PackageRoot 'reference\manifest.json')
foreach($file in $m.Reference.Files){
    $original=@($frozen.Files | Where-Object {$_.Path -ceq ('input/bin/'+$file.Name)})
    Need-C ($original.Count -eq 1 -and $original[0].Bytes -eq $file.Bytes -and $original[0].Sha256 -ceq $file.Sha256 -and
        (Hash-C (Join-Path $PackageRoot ('reference\bin\'+$file.Name))) -ceq $file.Sha256) 'byte-identical frozen reference binaries'
}
Need-C (@($m.Reference.Files).Count -eq @($frozen.Files | Where-Object {$_.Path.StartsWith('input/bin/')}).Count) 'complete frozen binary inventory'
$golden=Join-Path $PSScriptRoot 'decoder-cases.json';$regressionGolden=Join-Path $PSScriptRoot 'regression-cases.json'
Need-C ((Hash-C (Join-Path $PackageRoot 'tools\input\decoder-cases.json')) -ceq (Hash-C $golden) -and
    $m.Input.RecipeSha256 -ceq (Hash-C $golden) -and $m.Input.Cases -eq 43 -and $m.Input.Repetitions -eq 3 -and
    (Hash-C (Join-Path $PackageRoot 'reference\cases.json')) -ceq (Hash-C (Join-Path $PSScriptRoot 'cases.json')) -and
    (Hash-C (Join-Path $PackageRoot 'tools\input\regression-cases.json')) -ceq (Hash-C $regressionGolden) -and
    $m.Regression.RecipeSha256 -ceq (Hash-C $regressionGolden)) 'pinned current and frozen golden recipes'
$v=Read-C (Join-Path $PackageRoot 'verification.json')
Need-C ((Hash-C (Join-Path $PackageRoot 'verification.json')) -ceq $m.Runtime.VerificationSha256 -and
    $v.SourceCommit -ceq $source -and $v.SourceTreeClean -and $v.ApiVersion -ceq '1.1' -and $v.ProtocolVersion -eq 2 -and
    @($v.Tests | Where-Object {$_.ExitCode -ne 0 -or $_.TimedOut}).Count -eq 0) 'matching clean Release verification'
foreach($file in @($m.Files | Where-Object {$_.Path.StartsWith('runtime/bin/')})){
    $record=@($v.Artifacts | Where-Object {$_.Name -ceq [IO.Path]::GetFileName($file.Path)})
    Need-C ($record.Count -eq 1 -and $record[0].Bytes -eq $file.Bytes -and $record[0].Sha256 -ceq $file.Sha256) 'runtime matches verified artifacts'
}
$files=(Read-C (Join-Path $Directory 'evidence-files.json')).Files
Files-C $Directory $files
Need-C (@(Get-ChildItem -LiteralPath $Directory -Recurse -File).Count -eq $files.Count+1) 'complete returned file inventory'
Need-C ($r.ExpectedTrials -eq 7 -and ($r.Trials.Order -join ',') -ceq '1,2,3,4,5,6,7' -and
    ($r.Trials.Role -join ',') -ceq 'Reference,Guard,Input,Guard,Input,Guard,Input') 'frozen reference and three interleaved pairs'
$reviews=@();$failures=0
foreach($t in $r.Trials){
    Need-C ($t.CollectionStatus -ceq 'Completed' -and -not $t.TimedOut -and -not $t.Error -and
        $t.Directory -ceq ($t.Order.ToString('00')+'-'+$t.Role.ToLowerInvariant())) 'completed native controller'
    $native=Join-Path $Directory ($t.Directory+'\native')
    if($t.Role -ceq 'Reference'){
        $review=& (Join-Path $PSScriptRoot 'Test-InputEvidence.ps1') -Directory $native -ExpectedRuntimeSource $parent -ExpectedHarnessSource $frozenHarness -CaseDefinitionsPath (Join-Path $PackageRoot 'reference\cases.json')
        Need-C ($review.Outcome -ceq 'Fail' -and $review.Passed -eq 30 -and $review.Total -eq 33 -and
            ($review.FailedCases -join '|') -ceq 'utf8-timeout-fragment|utf8-live-replies|escape-live-replies' -and
            $t.Outcome -ceq 'Fail' -and $t.ExitCode -eq 1 -and $t.Passed -eq 30 -and $t.Total -eq 33) 'original rc1 failures remain actual failures'
        $delayed=Read-C (Join-Path $native 'utf8-timeout-fragment\input-observed.json')
        $live=Read-C (Join-Path $native 'utf8-live-replies\input-observed.json')
        foreach($pair in @(@($delayed,'z'),@($live,('A[1;1R'+[char]8364+[char]55357+[char]56898+'Z')))){
            $text=-join @($pair[0].Records | Where-Object {$_.Type -eq 1 -and $_.Down -and $_.Character -ne 57344 -and $_.VirtualKey -notin @(16,17,18)} | ForEach-Object {[string][char]$_.Character})
            Need-C ($text -ceq $pair[1]) 'retained rc1 Unicode failure signature'
        }
    }elseif($t.Role -ceq 'Input'){
        $review=& (Join-Path $PSScriptRoot 'Test-InputEvidence.ps1') -Directory $native -ExpectedRuntimeSource $source -ExpectedHarnessSource $source -CaseDefinitionsPath $golden
        Need-C ($t.Outcome -ceq $review.Outcome -and $t.Passed -eq $review.Passed -and $t.Total -eq 43 -and
            (($t.ExitCode -eq 0) -eq ($review.Outcome -ceq 'Pass'))) 'recomputed candidate input outcome'
        $failures+=43-$review.Passed
    }else{
        $review=& (Join-Path $PSScriptRoot '..\research\Test-H4RuntimeEvidence.ps1') -Directory $native
        Need-C ($t.Outcome -ceq 'Pass' -and $t.ExitCode -eq 0 -and $t.Total -eq $m.Guard.CasesByTier.($r.Tier) -and
            $t.Passed -eq $t.Total -and (Read-C (Join-Path $native 'probe.json')).SourceCommit -ceq $source) 'candidate H4 regression outcome'
    }
    $reviews+=@(@{Order=$t.Order;Role=$t.Role;Review=$review})
}
Need-C ($r.Qualification.Status -ceq 'Pass' -and $r.Qualification.ExitCode -eq 0 -and -not $r.Qualification.Error -and
    $r.Qualification.Directory -ceq 'qualification' -and $m.Qualification.Cases -eq 7) 'focused H4 qualification completed'
$q=& (Join-Path $PSScriptRoot '..\research\Test-H4QualificationEvidence.ps1') -Directory (Join-Path $Directory 'qualification') -ExpectedSource $source -ExpectedRuntimeSource $source
$recipes=(Read-C $regressionGolden).Cases
Need-C ($r.Regression.Status -ceq 'Pass' -and -not $r.Regression.Error -and $m.Regression.Cases -eq 14 -and
    ($r.Regression.Cases.Name -join '|') -ceq ($recipes.Name -join '|')) 'complete focused regression inventory'
for($n=0;$n -lt $recipes.Count;$n++){
    $test=$r.Regression.Cases[$n];$recipe=$recipes[$n]
    Need-C ($test.Executable -ceq $recipe.Executable -and $test.Arguments -ceq $recipe.Arguments -and
        $test.Status -ceq 'Pass' -and -not $test.Error -and -not $test.TimedOut -and $test.ExitCode -eq 0 -and
        $test.StandardOutput.Contains($recipe.ExpectedOutput)) ('regression completion '+$recipe.Name)
}
$result=[ordered]@{Kind='InputCandidateBundleReview';CollectionStatus='Completed';ComputerName=$r.ComputerName;Tier=$r.Tier
    CandidateOutcome=$(if($failures){'Fail'}else{'Pass'});SourceCommit=$source;InputCases=129;InputFailures=$failures
    ReferenceOutcome='Fail';ReferencePassed=30;ReferenceTotal=33;ReferenceFailuresRetained=$true
    Reviews=$reviews;Qualification=$q;RegressionPassed=14;CtrlBreakTransport='Unsupported';Step12Accepted=$false}
if($OutputPath){
    $OutputPath=[IO.Path]::GetFullPath($OutputPath)
    Need-C (-not $OutputPath.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside original evidence'
    [IO.File]::WriteAllText($OutputPath,($result | ConvertTo-Json -Depth 35),[Text.UTF8Encoding]::new($false))
}
$result
