# Developer-side version-4 checks, loaded by Test-HostFreezeEvidence.ps1.
function Review-CoordinationNative([string]$Native,$Root,$Locations) {
    Require-Trace ($Root.Version -eq 4 -and $Root.BaselineCoordinated -and $Root.NeighborPeriodicDrain -and $Root.NeighborSoakMs -ge 0 -and $Root.NeighborSoakMs -le 90000) 'coordinated research boundary'
    $rows=@();$owned=0;$borrowed=0;$refusals=0;$receipts=0;$exits=0
    foreach($location in $Locations) {
        $dir=Join-Path $Native $location.Path
        $hello=Read-TraceJson (Join-Path $dir ($location.Label+'-hello.json'))
        $calls=@();$events=@();$hostCalls=@()
        foreach($file in @(Get-ChildItem -LiteralPath $dir -File -Filter 'agent-*-request.json')) {
            $q=Read-TraceJson $file.FullName;$calls+=@($q);$events+=@($q.Response.Trace.Events)
        }
        $events=@($events | Sort-Object {[decimal]$_.Sequence})
        foreach($file in @(Get-ChildItem -LiteralPath $dir -File -Filter ($location.Label+'-*-request.json'))) {
            $q=Read-TraceJson $file.FullName
            if($q.ExpectedResult -in @(1,9) -and $q.RequestOperation -in @(5,6,7,8)) {$hostCalls+=@($file.Name)}
        }
        $bound=@{};$index=0
        foreach($file in @(Get-ChildItem -LiteralPath $dir -File -Filter 'coordination-*.json' | Where-Object {$_.Name -cmatch '^coordination-\d+\.json$'} | Sort-Object {[int]([regex]::Match($_.Name,'-(\d+)\.json').Groups[1].Value)})) {
            $p=Read-TraceJson $file.FullName;$index++
            Require-Trace ($file.Name -ceq ('coordination-'+$index+'.json') -and $p.SchemaVersion -eq 1 -and $p.Kind -ceq 'HostCancellationCoordination' -and
                $p.Operation -in @(5,6,7,8) -and -not $p.MilestoneAccepted -and -not $p.RetrySent -and $p.HostRecord -cmatch '^host-\d+-request\.json$') 'coordination record identity'
            Require-Trace (-not $bound.ContainsKey($p.HostRecord)) 'one permit per host operation'
            $path=Join-Path $dir $p.HostRecord
            if($p.Outcome -eq 'RefusedBeforeWrite') {
                Require-Trace ($location.Path -eq 'suite\coordination-expiry' -and -not (Test-Path -LiteralPath $path) -and -not $p.HostWriteStarted -and
                    -not $p.VerifiedBeforeWrite -and [decimal]$p.WriteBeginQpc -eq 0 -and [decimal]$p.CompletionQpc -eq 0 -and
                    $p.Error.StartsWith('CoordinationUnavailable: lease ended before dispatch') -and @($events | Where-Object {$_.Kind -eq 11}).Count -eq 1) 'expired lease refuses before a host write'
                $refusals++;continue
            }
            Require-Trace (Test-Path -LiteralPath $path) 'coordinated operation has host evidence'
            $q=Read-TraceJson $path;$bound[$p.HostRecord]=$true
            Require-Trace ($q.Case -ceq $p.Case -and $q.RequestSequence -ceq $p.RequestSequence -and $q.RequestOperation -eq $p.Operation -and
                $q.RequestSchema -eq 4 -and $q.RequestIdentity.Agent -eq $hello.Identity.Agent -and $q.RequestIdentity.Host -eq $hello.Identity.Host -and
                $p.HostWriteStarted -and $p.VerifiedBeforeWrite -and [decimal]$p.ReadyQpc -ge [decimal]$p.BeginQpc -and
                [decimal]$p.WriteBeginQpc -ge [decimal]$p.ReadyQpc -and [decimal]$p.CompletionQpc -ge [decimal]$p.WriteBeginQpc -and
                [decimal]$p.AckQpc -ge [decimal]$p.CompletionQpc -and [decimal]$p.EndQpc -ge [decimal]$p.AckQpc) 'permit binding and completion boundary'
            $grant=@($calls | Where-Object {$_.Command -in @(3,4,5,6) -and $_.Response.Lease -ceq $p.Lease})
            Require-Trace ($grant.Count -eq 1 -and $grant[0].Response.Mode -eq $p.Mode -and $grant[0].Response.Held -eq $p.Held -and
                $grant[0].Response.CachedFrozen -eq $p.CachedFrozen -and [decimal]$grant[0].AfterQpc -le [decimal]$p.ReadyQpc) 'permit matches exact agent grant'
            $protect=$p.Mode -in @(3,5) -and $p.Held -eq 1 -and $p.CachedFrozen -eq 0
            Require-Trace ([bool]$p.Protected -eq [bool]$protect) 'protection claim follows granted state'
            if($p.OwnedLease) {
                $owned++;Require-Trace ($protect -and $p.Mode -eq 3 -and $q.FixtureBefore.Trace.SelectionFlags -eq 0 -and
                    $q.FixtureBefore.Trace.SelectionError -eq 0 -and $q.FixtureBefore.Trace.InputError -eq 0) 'baseline cancellation uses unfrozen exclusion'
            } else {
                $borrowed++;Require-Trace ($location.Path -like 'suite\focus-*') 'only explicit contrasts borrow a lease'
            }
            if($q.ExpectedResult -eq 9) {
                $receipts++;Require-Trace ($p.Outcome -ceq 'ReceiptOnly' -and -not $p.FinalAckObserved -and $p.Operation -eq 7 -and $p.Released) 'receipt does not become a final acknowledgment'
            } else {
                Require-Trace ($p.Outcome -ceq 'Acknowledged' -and $p.FinalAckObserved -and $q.Response.Result -eq 1) 'final acknowledgment preserved'
                $restored=@($q.Response.Trace.Events | Where-Object {$_.Stage -eq 7})
                Require-Trace ($restored.Count -eq 1 -and [decimal]$restored[0].Qpc -le [decimal]$p.CompletionQpc -and
                    $q.Response.Keyboard.Restored -eq 1 -and $q.Response.Keyboard.Equal -eq 1) 'completion marker follows keyboard restoration'
            }
            if($p.OwnedLease -and $p.AgentExitResolvedLease) {
                $exits++;$finish=Read-TraceJson (Join-Path $dir 'agent-finish.json')
                Require-Trace ($p.FinalAckObserved -and $location.Path -eq 'suite\default-handler' -and -not $finish.AgentAliveAtCollection -and -not $finish.Held) 'owned agent exit resolves completed default-handler scope'
                $end=[decimal]$p.CompletionQpc
            } elseif($p.OwnedLease) {
                $release=@($events | Where-Object {$_.Kind -eq 10 -and [decimal]$_.Qpc -ge [decimal]$p.CompletionQpc -and [decimal]$_.Qpc -le [decimal]$p.EndQpc})
                Require-Trace ($p.Released -and $release.Count -eq 1) 'owned lease resumes after callback completion'
                $end=[decimal]$release[0].Qpc
            } else {$end=[decimal]$p.CompletionQpc;Require-Trace (-not $p.Released) 'borrowed lease remains controlled by the contrast'}
            if($protect) {
                $grantEvent=@($events | Where-Object {$_.Kind -eq 9 -and [decimal]$_.Qpc -ge [decimal]$grant[0].BeforeQpc -and [decimal]$_.Qpc -le [decimal]$grant[0].AfterQpc})
                Require-Trace ($grantEvent.Count -eq 1) 'protected scope grant transition'
                Require-Trace (@($events | Where-Object {$_.Kind -in @(1,5,7,11,13) -and [decimal]$_.Qpc -ge [decimal]$grantEvent[0].Qpc -and [decimal]$_.Qpc -lt $end}).Count -eq 0) 'no scrape/resize/expiry inside protected callback scope'
            }
            $rows+=@([ordered]@{Case=$location.Path;Record=$file.Name;HostRecord=$p.HostRecord;OwnedLease=$p.OwnedLease;Protected=$protect;Mode=$p.Mode;
                Lease=$p.Lease;CompletionQpc=$p.CompletionQpc;Outcome=$p.Outcome;FinalAckObserved=$p.FinalAckObserved;Released=$p.Released;AgentExitResolvedLease=$p.AgentExitResolvedLease})
        }
        Require-Trace ($bound.Count -eq $hostCalls.Count -and @($hostCalls | Where-Object {-not $bound.ContainsKey($_)}).Count -eq 0) 'every accepted mutation has one permit'
    }
    $expiryDir=Join-Path $Native 'suite\coordination-expiry'
    $expiry=Read-TraceJson (Join-Path $expiryDir 'coordination-expiry.json')
    $before=Read-TraceJson (Join-Path $expiryDir 'expiry-before-recovery.json')
    Require-Trace ($refusals -eq 1 -and $expiry.DispatchRefusedBeforeWrite -and $expiry.DispatchDisabled -and $expiry.RetryRefused -and
        $expiry.RequestedLeaseMs -eq 1000 -and -not $expiry.FallbackControlSent -and $expiry.DeferredResizeApplied -and $expiry.NextInputPreserved -and
        -not $before.ReadReturned -and $before.ControlCount -eq 0) 'expiry failure control preserves read and prevents retry'
    foreach($file in @(Get-ChildItem -LiteralPath $expiryDir -File -Filter 'host-*-request.json')) {
        $q=Read-TraceJson $file.FullName
        Require-Trace ($q.RequestOperation -notin @(5,6,7,8)) 'expired scope sends no host cancellation'
    }
    $collector=Read-TraceJson (Join-Path $Native 'neighbor\neighbor-collector.json')
    Require-Trace ($collector.Periodic -and $collector.IntervalMs -eq 250 -and $collector.Drains -gt 0 -and $collector.StorageCapacity -eq 4096 -and
        -not $collector.CapacityIncreased) 'periodic neighbor collection with unchanged storage'
    return [ordered]@{SchemaVersion=1;Kind='H3CoordinationReview';OwnedScopes=$owned;BorrowedScopes=$borrowed;ExpiryRefusals=$refusals;ReceiptOnlyScopes=$receipts;
        AgentExitResolvedScopes=$exits;Scopes=$rows;NeighborCollector=$collector;FinalReceiptAcknowledgmentAvailable=$false;MilestoneAccepted=$false}
}
