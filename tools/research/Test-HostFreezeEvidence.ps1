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
    Require-Trace ($root.SchemaVersion -eq 1 -and $root.Probe -eq 'HostCancellation' -and $root.Version -eq 3 -and $root.ResearchAgent -eq $true -and $root.AgentGateProtocol -eq 1 -and
        $root.DiagnosticOnly -and -not $root.H3Accepted -and -not $root.MilestoneAccepted -and
        -not $root.ProductionCancellationImplemented -and $root.OriginalDeadlineFailuresRetained) 'trace identity and research boundary'
    Require-Trace ($root.Rotation -in @(0,1,2)) 'rotation'
    $names=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial','host-key-record')
    foreach($read in @('completed','pending')) { foreach($position in 0..3) { $names+=@('focus-'+$read+'-'+@('quiet','selected','coordinated','delay-control')[($position+$root.Rotation)%4]) } }
    $names+=@('focus-selection-bracket')
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
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter ($location.Label+'-*-request.json') -File | Sort-Object {[int]([regex]::Match($_.Name,'-(\d+)-request').Groups[1].Value)})) {
            $q=Read-TraceJson $file.FullName; $r=$q.Response; $requests++
            Same-TraceIdentity $q.RequestIdentity $id
            Require-Trace ($q.RequestSchema -eq 3 -and $q.RequestBytes -eq 120 -and $q.RequestNonceLow -ceq $hello.Response.NonceLow -and
                $q.RequestNonceHigh -ceq $hello.Response.NonceHigh -and $r.NonceLow -ceq $hello.Response.NonceLow -and
                $r.NonceHigh -ceq $hello.Response.NonceHigh -and $r.Result -eq $q.ExpectedResult -and
                $r.Sequence -eq $q.RequestSequence -and $r.Operation -eq $q.RequestOperation) 'bound request/response'
            Require-Trace ($r.Host -eq $id.Host -and $r.Thread -eq $id.Thread -and $r.HostCreated -ceq $id.HostCreated -and
                $r.Window -eq $id.Window -and $r.WindowOwner -eq $id.Host -and $r.WindowThread -eq $id.Thread -and
                $r.ActualDesktop -ceq $location.Desktop -and $r.ValidationStage -eq 0) 'actual callback identity'
            if($r.Result -in @(1,9)) { Require-Trace ($q.RequestSequence -eq $sequence+1) 'accepted sequence'; if($q.RequestOperation -ne 10) { $sequence++ } }
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

function Review-FreezeNative([string]$Native) {
    $core=Review-TraceNative $Native
    $root=Read-TraceJson (Join-Path $Native 'probe.json')
    $cases=Read-TraceJson (Join-Path $Native 'suite\cases.json')
    $locations=@([pscustomobject]@{Path='neighbor';Label='neighbor'})
    $locations+=@($cases.Cases | ForEach-Object {[pscustomobject]@{Path=('suite\'+$_.Case);Label='host'}})
    $gaps=New-Object 'Collections.Generic.List[string]'
    $issues=New-Object 'Collections.Generic.List[string]'
    $conditions=@();$windows=0;$agentEvents=0;$freezeMessages=0;$escapeMessages=0;$leaseChecks=0
    foreach($location in $locations) {
        $dir=Join-Path $Native $location.Path
        $helloPath=Join-Path $dir ($location.Label+'-hello.json')
        if(-not (Test-Path -LiteralPath $helloPath)) { $gaps.Add($location.Path+': missing binding'); continue }
        $hello=Read-TraceJson $helloPath;$id=$hello.Identity
        $hostEvents=@();$cursor=[decimal]0;$lastTime=[decimal]0
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter ($location.Label+'-*-request.json') -File | Sort-Object {[int]([regex]::Match($_.Name,'-(\d+)-request').Groups[1].Value)})) {
            $q=Read-TraceJson $file.FullName;$t=$q.Response.WindowTrace
            Require-Trace (@($t.Events).Count -le 32 -and [decimal]$t.NextCursor -ge $cursor -and [decimal]$t.NextCursor -le [decimal]$t.Total) 'host message chunk bounds'
            if([decimal]$t.Lost -ne 0) { $gaps.Add($location.Path+': overwritten host messages');$cursor+=[decimal]$t.Lost }
            foreach($e in $t.Events) {
                Require-Trace ([decimal]$e.Sequence -eq $cursor+1 -and [decimal]$e.Qpc -ge $lastTime -and $e.Pid -eq $id.Host -and
                    $e.Thread -eq $id.Thread -and [decimal]$e.Window -eq [decimal]$id.Window -and $e.Phase -in @(1,2)) 'host message sequence/context'
                Require-Trace (($e.Message -eq 274 -and $e.WParam -in @(65522,65525)) -or ($e.Message -eq 258 -and $e.WParam -eq 27) -or
                    ($e.Message -eq 256 -and $e.WParam -eq 67)) 'bounded host message filter'
                $cursor++;$lastTime=[decimal]$e.Qpc;$windows++;$hostEvents+=@($e)
                if($e.Phase -eq 1 -and $e.Message -eq 274) { $freezeMessages++ }
                if($e.Phase -eq 1 -and $e.Message -eq 258) { $escapeMessages++ }
            }
            Require-Trace ($cursor -eq [decimal]$t.NextCursor) 'host chunk ends at declared cursor'
        }
        $events=@();$calls=@();$cursor=[decimal]0;$seq=0;$thread=0;$nonceLow='';$nonceHigh='';$lastTime=[decimal]0
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter 'agent-*-request.json' -File | Sort-Object {[int]([regex]::Match($_.Name,'-(\d+)-request').Groups[1].Value)})) {
            $q=Read-TraceJson $file.FullName;$r=$q.Response;$t=$r.Trace;$seq++
            if($seq -eq 1) {$nonceLow=$q.NonceLow;$nonceHigh=$q.NonceHigh;$thread=$r.Thread;Require-Trace ($q.Command -eq 1) 'first agent command binds'}
            Require-Trace ($q.Command -in @(1,2,3,4,5,6,7,8) -and [decimal]$q.RequestSequence -eq $seq -and
                [decimal]$r.Sequence -eq $seq -and $r.Agent -eq $id.Agent -and $r.Controller -eq $id.Controller -and $r.Thread -eq $thread -and
                $r.Created -ceq $id.AgentCreated -and $q.AgentCreated -ceq $id.AgentCreated -and $q.ControllerCreated -ceq $id.ControllerCreated -and
                $q.NonceLow -ceq $nonceLow -and $q.NonceHigh -ceq $nonceHigh -and $r.Status -eq 0 -and [decimal]$r.Frequency -gt 0 -and
                [decimal]$q.AfterQpc -ge [decimal]$q.BeforeQpc) 'agent request identity/sequence/interval'
            Require-Trace (@($t.Events).Count -le 32 -and [decimal]$t.NextCursor -ge $cursor -and [decimal]$t.NextCursor -le [decimal]$t.Total) 'agent chunk bounds'
            if([decimal]$t.Lost -ne 0) {$gaps.Add($location.Path+': overwritten agent events');$cursor+=[decimal]$t.Lost}
            foreach($e in $t.Events) {
                Require-Trace ([decimal]$e.Sequence -eq $cursor+1 -and [decimal]$e.Qpc -ge $lastTime -and [decimal]$e.Qpc -le [decimal]$q.AfterQpc -and
                    $e.Pid -eq $id.Agent -and $e.Thread -eq $thread -and $e.Kind -in @(1..14) -and $e.Frozen -in @(0,1)) 'agent event publication/context'
                $cursor++;$lastTime=[decimal]$e.Qpc;$agentEvents++;$events+=@($e)
            }
            Require-Trace ($cursor -eq [decimal]$t.NextCursor) 'agent chunk ends at declared cursor'
            $calls+=@($q)
        }
        if(-not $seq) {$gaps.Add($location.Path+': no agent observations')}
        $finishPath=Join-Path $dir 'agent-finish.json'
        if(Test-Path -LiteralPath $finishPath) {
            $finish=Read-TraceJson $finishPath
            if(-not $finish.CollectionHealthy -or $finish.Held) {$issues.Add($location.Path+': agent collection/resume failed')}
        } else {$gaps.Add($location.Path+': no agent finish record')}
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter '*-condition.json' -File)) {
            $p=Read-TraceJson $file.FullName
            $prefix=$file.BaseName.Substring(0,$file.BaseName.Length-'-condition'.Length)
            $before=Read-TraceJson (Join-Path $dir ($prefix+'-before.json'))
            $deadline=Read-TraceJson (Join-Path $dir ($prefix+'-deadline.json'))
            $released=Read-TraceJson (Join-Path $dir ($prefix+'-released.json'))
            $mode=@{quiet=3;selected=4;coordinated=5;'delay-control'=6}[$p.Condition]
            Require-Trace ($mode -gt 0 -and $p.GrantedMode -eq $mode -and $p.SchemaVersion -eq 1 -and $p.CharacterizationOnly -and
                $p.EvidenceCollected -and -not $p.MilestoneAccepted -and $p.DelayControlMs -eq 50 -and $p.RequestedLeaseMs -eq 20000 -and
                [decimal]$p.SendQpc -ge [decimal]$p.GrantQpc -and [decimal]$p.ReleaseBeginQpc -ge [decimal]$p.SendQpc -and
                [decimal]$p.ReleaseEndQpc -ge [decimal]$p.ReleaseBeginQpc) 'condition identity/timing'
            foreach($packet in @($before,$deadline,$released)) {Review-TracePacket $packet $id $file.Name}
            $predicate=$deadline.ControlCount -eq $p.ExpectedEvents -and $deadline.ControlType -eq 0
            if($p.PendingRead) {$predicate=$predicate -and $deadline.ReadReturned -eq 1 -and $deadline.ReadError -eq 995 -and $deadline.Text -eq ''}
            else {$predicate=$predicate -and $deadline.ReadReturned -eq 1 -and $deadline.ReadSucceeded -eq 1 -and $deadline.ReadError -eq 0 -and $deadline.Text -ceq ("COMPLETED"+[char]13+[char]10)}
            Require-Trace ([bool]$p.OriginalDeadlineMet -eq [bool]$predicate) 'original condition deadline predicate'
            $keys=@($hostEvents | Where-Object {$_.Message -eq 256 -and $_.WParam -eq 67 -and $_.Phase -eq 1 -and
                [decimal]$_.Qpc -ge [decimal]$p.SendQpc -and [decimal]$_.Qpc -lt [decimal]$p.ReleaseBeginQpc})
            Require-Trace ($keys.Count -eq 1) 'one key operation per condition'
            $key=$keys[0]
            Require-Trace (@($hostEvents | Where-Object {$_.Message -eq 256 -and $_.Phase -eq 2 -and $_.Request -ceq $key.Request -and
                [decimal]$_.Qpc -ge [decimal]$key.Qpc -and [decimal]$_.Qpc -le [decimal]$p.ReleaseBeginQpc}).Count -eq 1) 'condition key returned before release'
            $acquire=@($calls | Where-Object {$_.Command -eq $mode -and $_.Response.Lease -ceq $p.GrantedLease})
            Require-Trace ($acquire.Count -eq 1) 'condition grant binds exact agent lease'
            Require-Trace ($acquire[0].Response.Mode -eq $p.GrantedMode -and $acquire[0].Response.Held -eq $p.GrantedHeld -and
                $acquire[0].Response.CachedFrozen -eq $p.GrantedCachedFrozen -and [decimal]$acquire[0].AfterQpc -le [decimal]$p.GrantQpc) 'condition grant state matches agent response'
            $grantKind=if($mode -eq 6) {14} else {9}
            $grantEvents=@($events | Where-Object {$_.Kind -eq $grantKind -and [decimal]$_.Qpc -ge [decimal]$acquire[0].BeforeQpc -and
                [decimal]$_.Qpc -le [decimal]$acquire[0].AfterQpc})
            Require-Trace ($grantEvents.Count -eq 1) 'grant transition published during acquire'
            $grantTime=[decimal]$grantEvents[0].Qpc
            $deadlineLease=@($calls | Where-Object {$_.Command -eq 2 -and [decimal]$_.BeforeQpc -ge [decimal]$deadline.Trace.Qpc -and
                [decimal]$_.AfterQpc -le [decimal]$p.ReleaseBeginQpc -and $_.Response.Lease -ceq $p.GrantedLease})
            Require-Trace ([bool]$p.LeaseActiveAtDeadline -eq ($deadlineLease.Count -gt 0)) 'deadline lease claim matches agent observation'
            $releaseEvents=@($events | Where-Object {$_.Kind -eq 10 -and [decimal]$_.Qpc -ge [decimal]$p.ReleaseBeginQpc -and [decimal]$_.Qpc -le [decimal]$p.ReleaseEndQpc})
            Require-Trace ($releaseEvents.Count -eq 1) 'condition resumes agent exactly once'
            $untilRelease=[decimal]$releaseEvents[0].Qpc
            $during=@($events | Where-Object {[decimal]$_.Qpc -ge $grantTime -and [decimal]$_.Qpc -lt $untilRelease})
            $violations=@($during | Where-Object {$_.Kind -in @(1,5,7,11,13)})
            if($mode -ne 6 -and $violations.Count) {$issues.Add($location.Path+'/'+$prefix+': work/expiry inside held interval')}
            if(-not $p.LeaseActiveAtDeadline) {$issues.Add($location.Path+'/'+$prefix+': lease ended before deadline observation')}
            if(-not $p.ConditionObserved) {$issues.Add($location.Path+'/'+$prefix+': intended public state not observed')}
            if(-not $p.KeyboardRestored -or -not $p.NextReadPreserved -or -not $p.ActivityResumed -or -not $p.ResizeResumed) {$issues.Add($location.Path+'/'+$prefix+': restoration/resumption check failed')}
            $selectionGood=$before.Trace.SelectionError -eq 0 -and $before.Trace.InputError -eq 0 -and ($before.InputMode -band 1) -ne 0
            if($mode -eq 4) {$selectionGood=$selectionGood -and ($before.Trace.SelectionFlags -band 1) -ne 0 -and $p.GrantedHeld -eq 1 -and $p.GrantedCachedFrozen -eq 1}
            elseif($mode -eq 6) {$selectionGood=$selectionGood -and $p.GrantedHeld -eq 0}
            else {$selectionGood=$selectionGood -and $before.Trace.SelectionFlags -eq 0 -and $p.GrantedHeld -eq 1 -and $p.GrantedCachedFrozen -eq 0}
            Require-Trace ([bool]$p.ConditionObserved -eq [bool]$selectionGood) 'condition state claim matches public sample'
            $computed=$p.OriginalDeadlineMet -and $p.KeyboardRestored -and $p.ConditionObserved -and $p.LeaseActiveAtDeadline -and
                $p.NextReadPreserved -and $p.ActivityResumed -and $p.ResizeResumed
            Require-Trace ([bool]$p.CancellationPassed -eq [bool]$computed) 'condition outcome cannot hide a failed predicate'
            $freezeReturns=@($hostEvents | Where-Object {$_.Message -eq 274 -and $_.Phase -eq 2 -and [decimal]$_.Qpc -lt [decimal]$key.Qpc})
            $escapes=@($hostEvents | Where-Object {$_.Message -eq 258 -and $_.Phase -eq 1 -and [decimal]$_.Qpc -ge $untilRelease -and [decimal]$_.Qpc -le [decimal]$p.ReleaseEndQpc})
            $conditions+=@([ordered]@{Case=$location.Path;Phase=$prefix;Condition=$p.Condition;PendingRead=$p.PendingRead;OriginalDeadlineMet=$p.OriginalDeadlineMet;
                CancellationPassed=$p.CancellationPassed;ConditionObserved=$p.ConditionObserved;LeaseActiveAtDeadline=$p.LeaseActiveAtDeadline;
                SelectionBefore=$before.Trace.SelectionFlags;SelectionAtDeadline=$deadline.Trace.SelectionFlags;SelectionAfterRelease=$released.Trace.SelectionFlags;
                ControlBefore=$before.ControlCount;ControlAtDeadline=$deadline.ControlCount;ControlAfterRelease=$released.ControlCount;
                CachedFrozenAtRelease=$releaseEvents[0].Frozen;EarlierFreezeReturns=$freezeReturns.Count;EscapesDuringRelease=$escapes.Count;
                WorkEventsInsideLease=$violations.Count;ComparableDelayActualMs=[Math]::Round(([double]([decimal]$p.SendQpc-[decimal]$p.GrantQpc)*1000/[double]$acquire[0].Response.Frequency),3)})
            $leaseChecks++
        }
        foreach($file in @(Get-ChildItem -LiteralPath $dir -Filter 'shell-barriers.json' -File)) {
            $v=Read-TraceJson $file.FullName
            Require-Trace ($v.CompleteContentRequired -and $v.CommandSpecificPromptRequired -and -not $v.ReadBlockedProven) 'shell barrier boundaries'
            if([decimal]$v.FirstCompleteContentQpc -gt 0) {Require-Trace ([decimal]$v.FirstCompleteContentQpc -ge [decimal]$v.FirstFileExistsQpc) 'file content cannot precede first existence observation'}
        }
    }
    return [ordered]@{SchemaVersion=1;Kind='H3FreezeNativeReview';Core=$core;HostMessages=$windows;AgentEvents=$agentEvents;FreezeMessageEntries=$freezeMessages;
        EscapeMessageEntries=$escapeMessages;ConditionPhases=$leaseChecks;Conditions=$conditions;CoverageGaps=@($gaps);OperationalIssues=@($issues);
        MilestoneAccepted=$false;ProductionCancellationImplemented=$false}
}
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')) {
    $result=Review-FreezeNative $Directory
} else {
    Require-Trace ([bool]$PackageRoot) 'supply PackageRoot for a combined review'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $manifest=Read-TraceJson (Join-Path $PackageRoot 'manifest.json')
    $report=Read-TraceJson (Join-Path $Directory 'report.json')
    Require-Trace ($manifest.Kind -eq 'VT7PtyHostCancellationFreezeDiagnostics' -and $report.Kind -eq 'VT7PtyHostCancellationFreezeResults' -and
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
            Require-Trace ($root.Version -eq 2) 'unchanged trace reference'
            $review=& (Join-Path $PSScriptRoot 'Test-HostCancellationDiagnosticEvidence.ps1') -Directory $native
        } else {
            Require-Trace ($root.Version -eq 3 -and $root.Rotation -eq $trial.Rotation) 'experiment version/rotation'
            $review=Review-FreezeNative $native
        }
        $trials+=@([ordered]@{Order=$trial.Order;Role=$role;Outcome=$trial.Outcome;CollectionStatus=$trial.CollectionStatus;StrictControlAccepted=$strict;StrictControlError=$errorText;Review=$review})
    }
    Require-Trace ((@($report.Trials.Order) -join ',') -eq '1,2,3,4,5,6,7,8,9') 'nine distinct ordered trials'
    $result=[ordered]@{SchemaVersion=1;Kind='H3FreezeBundleReview';RunId=$report.RunId;VerifiedEvidenceFiles=$hashes;Trials=$trials;MilestoneAccepted=$false;ProductionCancellationImplemented=$false}
}
if($OutputPath) {
    $target=[IO.Path]::GetFullPath($OutputPath)
    Require-Trace (-not $target.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside raw results'
    [IO.File]::WriteAllText($target,($result | ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))
}
$result
