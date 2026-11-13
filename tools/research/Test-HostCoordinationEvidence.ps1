[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$PackageRoot='',
    [string]$OutputPath=''
)
# Developer-side independent review. The target runner uses PowerShell 2.0.
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
function Require-Trace([bool]$Condition,[string]$Reason) { if(-not $Condition) { throw ('H3 diagnostic evidence: '+$Reason) } }
function Read-TraceJson([string]$Path) { return [IO.File]::ReadAllText($Path) | ConvertFrom-Json }
function Canonical-Trace($Value) {
    if($null -eq $Value) { return 'null' }
    if($Value -is [string] -or $Value -is [ValueType]) { return ConvertTo-Json -InputObject $Value -Compress }
    if($Value -is [Collections.IDictionary]) {
        $parts=@(foreach($key in @($Value.Keys | Sort-Object)) { (ConvertTo-Json -InputObject ([string]$key) -Compress)+':'+(Canonical-Trace $Value[$key]) })
        return '{'+($parts -join ',')+'}'
    }
    if($Value -is [Collections.IEnumerable]) {
        $parts=@(foreach($item in $Value) { Canonical-Trace $item }); return '['+($parts -join ',')+']'
    }
    $parts=@(foreach($p in @($Value.PSObject.Properties | Sort-Object Name)) { (ConvertTo-Json -InputObject $p.Name -Compress)+':'+(Canonical-Trace $p.Value) })
    return '{'+($parts -join ',')+'}'
}
function Same-TraceIdentity($a,$b) {
    foreach($p in $a.PSObject.Properties) { Require-Trace ($p.Value -ceq $b.($p.Name)) ('session identity '+$p.Name) }
}
function Review-TracePacket($p,$identity,[string]$Where) {
    Require-Trace ($p.ProcessId -eq $identity.Fixture -and $p.ParentId -eq $identity.Agent -and
        $p.SelfQuery.NormalizedPid -eq $identity.Host) ('fixture binding: '+$Where)
    $t=$p.Trace
    Require-Trace ([decimal]$t.Frequency -gt 0 -and [decimal]$t.BeginQpc -gt 0 -and
        [decimal]$t.Qpc -ge [decimal]$t.BeginQpc -and $t.AtomicSnapshot -eq $false -and @($t.Events).Count -le 64) ('fixture observation interval: '+$Where)
    $last=0
    foreach($event in $t.Events) {
        Require-Trace ($event.Sequence -gt $last -and $event.Sequence -le 64 -and $event.Kind -in @(1,2,3,4,5,6) -and
            $event.Thread -gt 0 -and [decimal]$event.Qpc -gt 0 -and [decimal]$event.Qpc -le [decimal]$t.Qpc) ('published timeline event: '+$Where)
        $last=$event.Sequence
    }
}
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')) {
    $result=& (Join-Path $PSScriptRoot 'Test-HostFreezeEvidence.ps1') -Directory $Directory
} else {
    Require-Trace ([bool]$PackageRoot) 'supply PackageRoot for a combined review'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $manifest=Read-TraceJson (Join-Path $PackageRoot 'manifest.json')
    $report=Read-TraceJson (Join-Path $Directory 'report.json')
    Require-Trace ($manifest.Kind -eq 'VT7PtyHostCoordinationDiagnostics' -and $report.Kind -eq 'VT7PtyHostCoordinationResults' -and
        $report.Trials.Count -eq 9 -and $report.ManifestSha256 -ceq (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -and
        -not $report.MilestoneAccepted -and -not $report.ProductionCancellationImplemented -and $report.DiagnosticOnly -and -not $report.FindingsAreAcceptance) 'bundle identity/boundary'
    foreach($name in @('Control','Reference','Experiment','Runtime')) {Require-Trace ((Canonical-Trace $report.$name) -ceq (Canonical-Trace $manifest.$name)) 'source identities'}
    foreach($file in $manifest.Files) {
        $path=[IO.Path]::GetFullPath((Join-Path $PackageRoot $file.Path))
        Require-Trace ($path.StartsWith($PackageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and (Get-Item -LiteralPath $path).Length -eq $file.Bytes -and
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $file.Sha256) 'package file identity'
    }
    $trials=@();$hashes=0
    foreach($trial in $report.Trials) {
        $role=@('Control','Reference','Experiment')[($trial.Order-1)%3]
        Require-Trace ($trial.Role -ceq $role -and $trial.Group -eq [Math]::Ceiling($trial.Order/3.0) -and $trial.Rotation -eq $trial.Group-1 -and
            $trial.Directory -ceq ($trial.Order.ToString('00')+'-'+$role.ToLowerInvariant())) 'three interleaved groups and rotation'
        $trialDir=Join-Path $Directory $trial.Directory;$native=Join-Path $trialDir 'native'
        $inventory=Read-TraceJson (Join-Path $trialDir 'evidence-files.json')
        foreach($file in $inventory.Files) {
            $path=[IO.Path]::GetFullPath((Join-Path $trialDir $file.Path))
            Require-Trace ($path.StartsWith($trialDir+'\',[StringComparison]::OrdinalIgnoreCase) -and (Get-Item -LiteralPath $path).Length -eq $file.Bytes -and
                (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $file.Sha256) 'returned evidence hash'
            $hashes++
        }
        Require-Trace (@(Get-ChildItem -LiteralPath $trialDir -File -Recurse).Count -eq $inventory.Files.Count+1) 'complete trial inventory'
        if($trial.CollectionStatus -ne 'Completed') {$trials+=@([ordered]@{Order=$trial.Order;Role=$role;Outcome=$trial.Outcome;CollectionStatus=$trial.CollectionStatus;Partial=$true});continue}
        $root=Read-TraceJson (Join-Path $native 'probe.json');$cases=Read-TraceJson (Join-Path $native 'suite\cases.json');$neighbor=Read-TraceJson (Join-Path $native 'neighbor\neighbor.json')
        foreach($pair in @(@($root,$trial.Observation),@($cases,$trial.Cases),@($neighbor,$trial.Neighbor))) {Require-Trace ((Canonical-Trace $pair[0]) -ceq (Canonical-Trace $pair[1])) 'portable/native aggregates'}
        Require-Trace (-not $trial.TimedOut -and (($trial.Outcome -eq 'Pass') -eq ($trial.ExitCode -eq 0 -and $root.Status -eq 'Pass'))) 'original outcome preserved'
        $review=$null;$strict=$false;$errorText=''
        if($role -eq 'Control') {
            Require-Trace ($root.Version -eq 1) 'unchanged control version'
            try {$null=& (Join-Path $PSScriptRoot 'Test-HostCancellationEvidence.ps1') -Directory $native;$strict=$true}catch{$errorText=$_.Exception.Message}
            Require-Trace ($strict -eq ($trial.Outcome -eq 'Pass')) 'control strict review agrees'
        } elseif($role -eq 'Reference') {
            Require-Trace ($root.Version -eq 3) 'unchanged version-3 reference'
            $review=& (Join-Path $PSScriptRoot 'Test-HostFreezeEvidence.ps1') -Directory $native
        } else {
            Require-Trace ($root.Version -eq 4 -and $root.Rotation -eq $trial.Rotation) 'experiment version/rotation'
            $review=& (Join-Path $PSScriptRoot 'Test-HostFreezeEvidence.ps1') -Directory $native
        }
        $trials+=@([ordered]@{Order=$trial.Order;Role=$role;Outcome=$trial.Outcome;CollectionStatus=$trial.CollectionStatus;StrictControlAccepted=$strict;StrictControlError=$errorText;Review=$review})
    }
    Require-Trace ((@($report.Trials.Order) -join ',') -eq '1,2,3,4,5,6,7,8,9') 'nine distinct ordered trials'
    $result=[ordered]@{SchemaVersion=1;Kind='H3CoordinationBundleReview';RunId=$report.RunId;VerifiedEvidenceFiles=$hashes;Trials=$trials;MilestoneAccepted=$false;ProductionCancellationImplemented=$false}
}
if($OutputPath) {
    $target=[IO.Path]::GetFullPath($OutputPath)
    Require-Trace (-not $target.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside raw results'
    [IO.File]::WriteAllText($target,($result | ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))
}
$result
