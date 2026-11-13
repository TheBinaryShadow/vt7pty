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
function Review-TraceNative([string]$Native) {
    $root=Read-TraceJson (Join-Path $Native 'probe.json')
    $cases=Read-TraceJson (Join-Path $Native 'suite\cases.json')
    $neighbor=Read-TraceJson (Join-Path $Native 'neighbor\neighbor.json')
    Require-Trace ($root.SchemaVersion -eq 1 -and $root.Probe -eq 'HostCancellation' -and $root.Version -eq 2 -and
        $root.DiagnosticOnly -and -not $root.H3Accepted -and -not $root.MilestoneAccepted -and
        -not $root.ProductionCancellationImplemented -and $root.OriginalDeadlineFailuresRetained) 'trace identity and research boundary'
    $names=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial','host-key-record')
    Require-Trace ((@($cases.Cases | ForEach-Object {$_.Case}) -join '|') -ceq ($names -join '|')) 'trace case inventory'
    Require-Trace ($root.SuiteDesktop -cmatch '^VT7Pty-H3-[0-9a-f]{32}$' -and
        $root.NeighborDesktop -cmatch '^VT7Pty-H3-[0-9a-f]{32}$' -and $root.SuiteDesktop -cne $root.NeighborDesktop -and $root.RootDesktop -eq 'Default') 'private desktop identity'
    $locations=@([pscustomobject]@{Path='neighbor';Label='neighbor';Passed=$neighbor.Passed;Desktop=$root.NeighborDesktop})
    $locations+=@($cases.Cases | ForEach-Object {[pscustomobject]@{Path=('suite\'+$_.Case);Label='host';Passed=$_.Passed;Desktop=$root.SuiteDesktop}})
    $gaps=New-Object 'Collections.Generic.List[string]'
    $findings=@(); $sessions=0; $requests=0; $keyOperations=0; $entries=0; $returns=0; $published=0; $late=0
    foreach($location in $locations) {
        $dir=Join-Path $Native $location.Path
        $helloPath=Join-Path $dir ($location.Label+'-hello.json')
        if(-not (Test-Path -LiteralPath $helloPath)) {
            Require-Trace (-not $location.Passed) 'passing case missing greeting'
            $gaps.Add($location.Path+': no completed channel'); continue
        }
        $hello=Read-TraceJson $helloPath; $id=$hello.Identity; $sessions++
        Require-Trace ($hello.Desktop -ceq $location.Desktop -and $hello.ControllerDesktop -ceq $location.Desktop -and
            $hello.PipePeer -eq $id.Host -and $hello.Response.Host -eq $id.Host -and $hello.Response.Thread -eq $id.Thread -and
            $hello.Response.HostCreated -ceq $id.HostCreated -and $hello.Response.Window -eq $id.Window -and
            $hello.Response.WindowOwner -eq $id.Host -and $hello.Response.WindowThread -eq $id.Thread) 'actual greeting context'
        $h1=Read-TraceJson (Join-Path $dir ($location.Label+'-identity.json'))
        Require-Trace ($h1.IdentityEligible -and $h1.NativeX64 -and $h1.SameUserLogonIntegritySession -and
            $h1.HostProcessId -eq $id.Host -and $h1.HostCreationTime -ceq $id.HostCreated) 'H1 prerequisite'
        foreach($query in @($h1.Fixture.SelfQuery,$h1.ChildQuery,$h1.AgentQuery)) {
            Require-Trace ($query.NtStatus -eq 0 -and $query.ReturnLength -eq 8 -and $query.NormalizedPid -eq $id.Host -and
                (([uint64]$query.RawHostValue -band [uint64]4294967292) -eq [uint64]$id.Host)) 'console-host association'
        }
        $discovery=Read-TraceJson (Join-Path $dir ($location.Label+'-discovery.json'))
        Require-Trace (-not $discovery.Overflow -and $discovery.FirstThread -eq $id.Thread -and $discovery.SecondThread -eq $id.Thread) 'dispatch discovery'
        foreach($event in $discovery.Events) {
            Require-Trace ($event.ThreadOwner -eq $id.Host -and $event.EventThread -eq $id.Thread -and
                $event.Window -eq $id.Window -and $event.CallbackThread -eq $id.ControllerThread) 'scoped title event'
        }
        $sequence=0
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter '*-request.json' -File | Sort-Object {[int]([regex]::Match($_.Name,'-(\d+)-request').Groups[1].Value)})) {
            $q=Read-TraceJson $file.FullName; $r=$q.Response; $requests++
            Same-TraceIdentity $q.RequestIdentity $id
            Require-Trace ($q.RequestSchema -eq 2 -and $q.RequestBytes -eq 112 -and $q.RequestNonceLow -ceq $hello.Response.NonceLow -and
                $q.RequestNonceHigh -ceq $hello.Response.NonceHigh -and $r.NonceLow -ceq $hello.Response.NonceLow -and
                $r.NonceHigh -ceq $hello.Response.NonceHigh -and $r.Result -eq $q.ExpectedResult -and
                $r.Sequence -eq $q.RequestSequence -and $r.Operation -eq $q.RequestOperation) 'bound request/response'
            Require-Trace ($r.Host -eq $id.Host -and $r.Thread -eq $id.Thread -and $r.HostCreated -ceq $id.HostCreated -and
                $r.Window -eq $id.Window -and $r.WindowOwner -eq $id.Host -and $r.WindowThread -eq $id.Thread -and
                $r.ActualDesktop -ceq $location.Desktop -and $r.ValidationStage -eq 0) 'actual callback identity'
            if($r.Result -in @(1,9)) { Require-Trace ($q.RequestSequence -eq $sequence+1) 'accepted sequence'; $sequence++ }
            Require-Trace ($r.AcceptedSequence -eq $sequence) 'rejections preserve sequence'
            Require-Trace ([decimal]$q.ControllerAfterQpc -ge [decimal]$q.ControllerBeforeQpc -and
                $q.OutputBytesAfter -ge $q.OutputBytesBefore) 'controller observation interval'
            foreach($packet in @($q.FixtureBefore,$q.FixtureAfter)) {
                if($null -ne $packet) { Review-TracePacket $packet $id $file.Name }
            }
            if($q.AfterObservationError) { $gaps.Add($location.Path+': '+$q.AfterObservationError) }
            if($r.Result -eq 1 -and $r.Operation -in @(5,6,8)) {
                $keyOperations++; $t=$r.Trace; $k=$r.Keyboard
                Require-Trace ([decimal]$t.Sequence -eq [decimal]$q.RequestSequence -and [decimal]$t.Frequency -gt 0 -and
                    @($t.Events).Count -le 16) 'key trace correlation/bounds'
                if($t.Overflow) { $gaps.Add($file.Name+': key trace overflow') }
                $lastTime=[decimal]0; $stages=@()
                foreach($e in $t.Events) {
                    Require-Trace ($e.Stage -in @(1,2,3,4,5,6,7,8) -and [decimal]$e.Qpc -ge $lastTime -and
                        [decimal]$e.Qpc -ge [decimal]$q.ControllerBeforeQpc -and [decimal]$e.Qpc -le [decimal]$q.ControllerAfterQpc -and
                        $e.Pid -eq $id.Host -and $e.Thread -eq $id.Thread -and [decimal]$e.Window -eq [decimal]$id.Window -and
                        $e.Keys -cmatch '^[0-9a-f]{512}$') 'nested trace context/clock'
                    if($e.Stage -in @(3,4,5,6)) { Require-Trace ($e.Message -eq 256 -and [decimal]$e.WParam -eq 67) 'observed key message' }
                    $lastTime=[decimal]$e.Qpc; $stages+=@($e.Stage)
                }
                $entry=@($t.Events | Where-Object {$_.Stage -eq 4}); $ret=@($t.Events | Where-Object {$_.Stage -eq 5})
                $entries+=$entry.Count; $returns+=$ret.Count
                if($k.Sent -eq 1 -and ($entry.Count -ne 1 -or $ret.Count -ne 1)) { $gaps.Add($location.Path+': nested key entry/return count differs from one') }
                if($k.Captured -and $k.Restored -and $k.Equal) { Require-Trace ($k.Before -ceq $k.After) 'claimed restoration bytes' }
                if($k.Sent -eq 1) {
                    Require-Trace (@($stages | Where-Object {$_ -eq 3}).Count -eq 1 -and @($stages | Where-Object {$_ -eq 6}).Count -eq 1) 'one recorded key call'
                }
                $findings+=@([ordered]@{
                    Case=$location.Path;Request=$file.Name;Sequence=$q.RequestSequence;Sent=$k.Sent;KeyboardError=$k.Error;Stages=$stages
                    EntryCount=$entry.Count;ReturnCount=$ret.Count
                    EntryCtrl=@($entry | ForEach-Object {([int]$_.Ctrl -band 32768) -ne 0})
                    EntryLeftCtrl=@($entry | ForEach-Object {([int]$_.LeftCtrl -band 32768) -ne 0})
                    EntryKeysMatchApplied=@($entry | ForEach-Object {$_.Keys -ceq $k.During})
                    EntryWindowOwners=@($entry | ForEach-Object {$_.WindowOwner})
                    EntryWindowThreads=@($entry | ForEach-Object {$_.WindowThread})
                    ReturnedStateRestored=($k.Restored -eq 1 -and $k.Equal -eq 1)
                })
            }
        }
        $finalPath=Join-Path $dir 'fixture-last.json'
        if(Test-Path -LiteralPath $finalPath) {
            $packet=Read-TraceJson $finalPath; Review-TracePacket $packet $id 'fixture-last'
            $published+=@($packet.Trace.Events).Count
            if($packet.Trace.Overflow) { $gaps.Add($location.Path+': fixture timeline overflow') }
        } elseif($location.Passed) { $gaps.Add($location.Path+': no last fixture observation') }
        $framesPath=Join-Path $dir 'output-trace.json'
        if(Test-Path -LiteralPath $framesPath) {
            $frames=Read-TraceJson $framesPath
            Require-Trace ($frames.SchemaVersion -eq 1 -and -not $frames.ObserverEpochIsReadinessProof -and @($frames.Frames).Count -le 512) 'output trace bounds/claim'
            if($frames.Overflow) { $gaps.Add($location.Path+': output frame overflow') }
            $offset=0; $time=[decimal]0
            foreach($frame in $frames.Frames) {
                Require-Trace ($frame.OutputBytes -gt $offset -and [decimal]$frame.Qpc -ge $time) 'output positions and timestamps'
                $offset=$frame.OutputBytes; $time=[decimal]$frame.Qpc
            }
            Require-Trace ($offset -eq (Get-Item -LiteralPath (Join-Path $dir 'stream.bin')).Length) 'output trace covers preserved bytes'
        } elseif($location.Passed) { $gaps.Add($location.Path+': no output trace') }
        foreach($lateFile in @(Get-ChildItem -LiteralPath $dir -Filter '*-late.json' -File)) {
            $extra=Read-TraceJson $lateFile.FullName; $late++
            Require-Trace (-not $extra.OriginalDeadlineMet -and $extra.ExtendedWindowMs -eq 5000 -and -not $location.Passed -and
                [decimal]$extra.EndQpc -ge [decimal]$extra.BeginQpc) 'late observation cannot upgrade original failure'
            Review-TracePacket $extra.Observation $id $lateFile.Name
        }
        $promptPath=Join-Path $dir 'prompt-generations.json'
        if(Test-Path -LiteralPath $promptPath) {
            $prompt=Read-TraceJson $promptPath
            Require-Trace (-not $prompt.ObserverEpochIsReadinessProof -and -not $prompt.ReadBlockedProvenByPrompt) 'prompt readiness limits'
        }
    }
    $files=@(Get-ChildItem -LiteralPath $Native -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{Path=$_.FullName.Substring($Native.Length+1).Replace('\','/');Bytes=$_.Length;Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    return [ordered]@{
        SchemaVersion=1;Kind='H3TraceEvidenceReview';NativeStatus=$root.Status;PassedCases=@($cases.Cases | Where-Object {$_.Passed}).Count
        FailedCases=@($cases.Cases | Where-Object {-not $_.Passed} | ForEach-Object {$_.Case})
        Sessions=$sessions;Requests=$requests;KeyOperations=$keyOperations;NestedEntries=$entries;NestedReturns=$returns
        LastSnapshotPublishedFixtureEvents=$published;ExtendedObservations=$late;CoverageGaps=@($gaps);KeyFindings=$findings
        MilestoneAccepted=$false;ProductionCancellationImplemented=$false;Files=$files
    }
}
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')) {
    $result=Review-TraceNative $Directory
} else {
    Require-Trace ([bool]$PackageRoot) 'supply PackageRoot for a combined result review'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $manifest=Read-TraceJson (Join-Path $PackageRoot 'manifest.json')
    $report=Read-TraceJson (Join-Path $Directory 'report.json')
    Require-Trace ($report.Kind -eq 'VT7PtyHostCancellationDiagnosticResults' -and $report.Trials.Count -eq 6 -and
        $report.ManifestSha256 -ceq (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -and
        -not $report.MilestoneAccepted -and -not $report.ProductionCancellationImplemented -and $report.DiagnosticOnly -and
        -not $report.FindingsAreAcceptance) 'combined report identity/boundary'
    Require-Trace ((Canonical-Trace $report.Control) -ceq (Canonical-Trace $manifest.Control) -and
        (Canonical-Trace $report.Trace) -ceq (Canonical-Trace $manifest.Trace) -and
        (Canonical-Trace $report.Runtime) -ceq (Canonical-Trace $manifest.Runtime)) 'combined source identity'
    $trials=@(); $hashes=0
    foreach($trial in $report.Trials) {
        $expectedRole=if($trial.Order % 2) {'Control'} else {'Trace'}
        Require-Trace ($trial.Role -ceq $expectedRole -and $trial.Pair -eq [Math]::Ceiling($trial.Order/2.0) -and
            $trial.Directory -ceq ($trial.Order.ToString('00')+'-'+$expectedRole.ToLowerInvariant())) 'interleaved order'
        $trialDir=Join-Path $Directory $trial.Directory
        $native=Join-Path $trialDir 'native'
        $inventory=Read-TraceJson (Join-Path $trialDir 'evidence-files.json')
        foreach($file in $inventory.Files) {
            $path=[IO.Path]::GetFullPath((Join-Path $trialDir $file.Path))
            Require-Trace ($path.StartsWith($trialDir+'\',[StringComparison]::OrdinalIgnoreCase) -and
                (Get-Item -LiteralPath $path).Length -eq $file.Bytes -and
                (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $file.Sha256) 'returned evidence hash'
            $hashes++
        }
        Require-Trace (@(Get-ChildItem -LiteralPath $trialDir -File -Recurse).Count -eq $inventory.Files.Count+1) 'complete trial inventory'
        if($trial.CollectionStatus -ne 'Completed') {
            $trials+=@([ordered]@{Order=$trial.Order;Role=$trial.Role;Outcome=$trial.Outcome;CollectionStatus=$trial.CollectionStatus;Partial=$true})
            continue
        }
        $root=Read-TraceJson (Join-Path $native 'probe.json'); $cases=Read-TraceJson (Join-Path $native 'suite\cases.json'); $neighbor=Read-TraceJson (Join-Path $native 'neighbor\neighbor.json')
        foreach($pair in @(@($root,$trial.Observation),@($cases,$trial.Cases),@($neighbor,$trial.Neighbor))) { Require-Trace ((Canonical-Trace $pair[0]) -ceq (Canonical-Trace $pair[1])) 'portable/native aggregates' }
        Require-Trace (-not $trial.TimedOut -and (($trial.Outcome -eq 'Pass') -eq ($trial.ExitCode -eq 0 -and $root.Status -eq 'Pass'))) 'original outcome preserved'
        $nativeReview=$null; $strictAccepted=$false; $strictError=''
        if($trial.Role -eq 'Control') {
            Require-Trace ($root.Version -eq 1) 'original control protocol'
            try { $null=& (Join-Path $PSScriptRoot 'Test-HostCancellationEvidence.ps1') -Directory $native; $strictAccepted=$true }
            catch { $strictError=$_.Exception.Message }
            Require-Trace ($strictAccepted -eq ($trial.Outcome -eq 'Pass')) 'strict control review agrees with reported outcome'
        } else { $nativeReview=Review-TraceNative $native }
        $trials+=@([ordered]@{Order=$trial.Order;Role=$trial.Role;Outcome=$trial.Outcome;CollectionStatus=$trial.CollectionStatus;StrictControlAccepted=$strictAccepted;StrictControlError=$strictError;TraceReview=$nativeReview})
    }
    Require-Trace ((@($report.Trials.Order) -join ',') -eq '1,2,3,4,5,6') 'six distinct ordered trials'
    $result=[ordered]@{SchemaVersion=1;Kind='H3DiagnosticBundleReview';RunId=$report.RunId;VerifiedEvidenceFiles=$hashes;Trials=$trials;MilestoneAccepted=$false;ProductionCancellationImplemented=$false}
}
if($OutputPath) {
    $target=[IO.Path]::GetFullPath($OutputPath)
    Require-Trace (-not $target.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output must be outside raw results'
    [IO.File]::WriteAllText($target,($result | ConvertTo-Json -Depth 22),[Text.UTF8Encoding]::new($false))
}
$result
