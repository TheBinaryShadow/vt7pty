# Developer-side protocol-5 review. Target collection stays compatible with PowerShell 2.0.
function Get-HostGuardNames([bool]$Ps7) {
    $names=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial','host-key-record','coordination-expiry','race-completion-enter-biased','race-completion-concurrent','race-completion-cancel-biased','race-completion-outcomes','race-close-published','race-close-completion','race-close-receipt','kernel-access-denied','child-cmd-etx','child-cmd-host','child-powershell-etx','child-powershell-host')
    if($Ps7){$names+=@('child-ps7-native-etx','child-ps7-native-host','child-ps7-readline-etx','child-ps7-readline-host','ps7-native-empty','ps7-readline-empty','ps7-native-partial','ps7-readline-partial')}
    return $names
}
function Test-GuardCanceled($P) {return $P.ReadReturned -eq 1 -and $P.ReadError -eq 995 -and $P.Text -ceq '' -and $P.ControlCount -eq 1 -and $P.ControlType -eq 0}
function Test-GuardNext($P,[string]$Text,[int]$Events) {return $P.ReadReturned -eq 1 -and $P.ReadSucceeded -eq 1 -and $P.ReadError -eq 0 -and $P.Text -ceq ($Text+[char]13+[char]10) -and $P.ControlCount -eq $Events}
function Test-GuardFence($P) {
    if($P.ReadReturned -ne 1 -or $P.ReadSucceeded -ne 1 -or $P.ReadError -ne 0 -or $P.ControlCount -ne 1 -or @($P.GuardInput).Count -lt 2 -or @($P.GuardInput).Count -gt 32){return $false}
    $ed=0;$eu=0;$md=0;$mu=0
    foreach($r in $P.GuardInput) {
        if($r.Type -ne 1){continue};if($r.Down -eq 1 -and $r.Repeat -ne 1){return $false}
        if($r.VirtualKey -eq 13){if($r.Down -eq 1){$ed++}else{$eu++}}
        elseif($r.VirtualKey -eq 90){if($r.Down -eq 1 -and $r.Character -eq 122){$md++}elseif($r.Down -eq 0){$mu++}else{return $false}}
        elseif($r.Down -eq 1){return $false}
    }
    return $ed -le 1 -and $eu -le 1 -and $md -eq 1 -and $mu -eq 1
}
function Review-HostGuardNative([string]$Native,$Root,$Cases,$Coordination) {
    Require-Trace ($Root.Version -eq 5 -and $Root.GuardQualification -and -not $Root.CharacterizationConditions -and $Root.GuardTier -in @('Development','NonESU','ESU','Legacy') -and [bool]$Root.PowerShell7Required -eq ($Root.GuardTier -ne 'Legacy')) 'guard identity and application policy'
    $policy=Read-TraceJson (Join-Path $Native 'suite\guard-applicability.json');$ps7=[bool]$Root.PowerShell7Required
    Require-Trace ($policy.Tier -ceq $Root.GuardTier -and [bool]$policy.PowerShell7Required -eq $ps7 -and $policy.ExpectedCases -eq $(if($ps7){34}else{26}) -and $policy.PowerShell7Cases -eq $(if($ps7){8}else{0}) -and $policy.PowerShell7NotApplicableCases -eq $(if($ps7){0}else{8}) -and $policy.CharacterizationContrastsRunInUnchangedReference) 'explicit application applicability'
    Require-Trace ($Coordination.BorrowedScopes -eq 0) 'guard requests never borrow characterization leases'
    $lifetimes=@(Get-ChildItem -LiteralPath (Join-Path $Native 'suite') -File -Filter 'lifetime-*.json' | ForEach-Object {Read-TraceJson $_.FullName})
    $rows=New-Object 'Collections.Generic.List[object]';$missing=New-Object 'Collections.Generic.List[string]';$completed=0;$canceled=0
    foreach($case in $Cases.Cases) {
        $name=$case.Case;$dir=Join-Path $Native ('suite\'+$name)
        $file=if($name -like 'race-completion-*' -and $name -ne 'race-completion-outcomes'){'completion-race.json'}elseif($name -eq 'race-completion-outcomes'){'race-coverage.json'}elseif($name -like 'race-close-*'){'close-race.json'}elseif($name -eq 'kernel-access-denied'){'denial.json'}elseif($name -like 'child-*'){'child-guard.json'}elseif($name -like 'ps7-readline-*'){'editor-guard.json'}elseif($name -like 'ps7-native-*'){'shell.json'}else{''}
        if(-not $file){continue}
        if(-not (Test-Path -LiteralPath (Join-Path $dir $file))) {Require-Trace (-not $case.Passed) ('passing guard lacks evidence: '+$name);$missing.Add($name);continue}
        $p=Read-TraceJson (Join-Path $dir $file);$hello=Read-TraceJson (Join-Path $dir 'host-hello.json');$id=$hello.Identity
        $mutations=@(Get-ChildItem -LiteralPath $dir -File -Filter 'host-*-request.json' | ForEach-Object {Read-TraceJson $_.FullName} | Where-Object {$_.RequestOperation -in @(5,6,7,8) -and $_.ExpectedResult -in @(1,9)})
        $computed=$false
        if($file -eq 'completion-race.json') {
            Require-Trace ($mutations.Count -eq 1 -and $mutations[0].RequestOperation -eq 5) 'one completion-race cancellation'
            $key=@($mutations[0].Response.Trace.Events | Where-Object {$_.Stage -eq 4})
            Require-Trace ($key.Count -eq 1 -and [decimal]$p.KeyEntryQpc -eq [decimal]$key[0].Qpc -and [decimal]$p.ContenderReadyQpc -gt 0 -and [decimal]$p.RequestPublishedQpc -ge [decimal]$p.ContenderReadyQpc -and [decimal]$p.SignalBeginQpc -ge [decimal]$p.RequestPublishedQpc -and [decimal]$p.SignalEndQpc -ge [decimal]$p.SignalBeginQpc -and [decimal]$p.EnterBeginQpc -ge [decimal]$p.RequestPublishedQpc -and [decimal]$p.EnterEndQpc -ge [decimal]$p.EnterBeginQpc -and $p.ConcurrentControllerRequests -and -not $p.SimultaneousKernelExecutionClaimed -and $p.CancellationDispatches -eq 1 -and $p.CompletionEnters -eq 1 -and -not $p.FallbackControlSent) 'contender clocks and single input'
            $read=Read-TraceJson (Join-Path $dir 'race-read.json');Review-TracePacket $read $id 'race read'
            $ret=@($read.Trace.Events | Where-Object {$_.Kind -eq 3 -and $_.ReadIndex -eq 1});$handler=@($read.Trace.Events | Where-Object {$_.Kind -eq 5 -and $_.ReadIndex -eq 1})
            $aborted=Test-GuardCanceled $read;$normal=Test-GuardNext $read 'racekeep' 1;$actual=if($aborted){'Canceled'}elseif($normal){'Completed'}else{'Invalid'}
            Require-Trace ($p.Outcome -ceq $actual -and [decimal]$p.Frequency -eq [decimal]$read.Trace.Frequency) 'race outcome follows raw read'
            if($aborted){$canceled++};if($normal){$completed++}
            $deadline=$ret.Count -eq 1 -and $handler.Count -eq 1
            if($deadline){$deadline=[decimal]$p.ReadReturnQpc -eq [decimal]$ret[0].Qpc -and [decimal]$p.HandlerExitQpc -eq [decimal]$handler[0].Qpc -and [decimal]$ret[0].Qpc -le [decimal]$p.KeyEntryQpc+2*[decimal]$p.Frequency -and [decimal]$handler[0].Qpc -le [decimal]$p.KeyEntryQpc+2*[decimal]$p.Frequency}
            $fence=Read-TraceJson (Join-Path $dir 'race-fence.json');$next=Read-TraceJson (Join-Path $dir 'race-next.json')
            Review-TracePacket $fence $id 'race fence';Review-TracePacket $next $id 'race next'
            $fenced=Test-GuardFence $fence;$preserved=Test-GuardNext $next 'NEXT_PROBE' 1
            Require-Trace ([bool]$p.InputFencePassed -eq [bool]$fenced -and [bool]$p.NextReadPreserved -eq [bool]$preserved) 'race guards follow raw input'
            $computed=$deadline -and $p.OriginalDeadlineMet -and ($aborted -or $normal) -and $fenced -and $preserved
        } elseif($file -eq 'race-coverage.json') {
            $computed=$p.Completed -eq $completed -and $p.Canceled -eq $canceled -and $p.RequiredCases -eq 3 -and $completed -ge 1 -and $canceled -ge 1 -and $completed+$canceled -eq 3
            Require-Trace ([bool]$p.BothOutcomesObserved -eq [bool]$computed) 'both race outcomes must actually occur'
        } elseif($file -eq 'close-race.json') {
            Require-Trace ($mutations.Count -eq 1 -and $p.CloseSawActiveRequest -and $p.DispatchDisabled -and $p.FurtherRequestRefused -and $p.CancellationDispatches -eq 1 -and -not $p.FallbackControlSent -and -not $p.HostCallbackWaitAdded) 'concurrent close has one committed request'
            Require-Trace ([decimal]$p.RequesterReadyQpc -le [decimal]$p.RequestPublishedQpc -and [decimal]$p.CloseRequestedQpc -ge [decimal]$p.RequestPublishedQpc -and [decimal]$p.DispatchResumedQpc -ge [decimal]$p.CloseRequestedQpc -and [decimal]$p.ResponseConsumedQpc -ge [decimal]$p.DispatchResumedQpc -and [decimal]$p.RequestEndedQpc -ge [decimal]$p.ResponseConsumedQpc -and [decimal]$p.RundownQpc -ge [decimal]$p.RequestEndedQpc) 'close defers in-flight resource release'
            if($p.Phase -eq 'CompletionBeforeAckConsumption'){Require-Trace ([decimal]$p.KeyboardCompletionObservedQpc -gt 0 -and [decimal]$p.CloseRequestedQpc -ge [decimal]$p.KeyboardCompletionObservedQpc) 'completion-side close'}
            else{Require-Trace ($p.Phase -eq 'PublishedBeforeNotify' -and [decimal]$p.KeyboardCompletionObservedQpc -eq 0) 'publication-side close'}
            if($p.ReceiptOnly){
                Require-Trace (-not $p.FinalAcknowledgmentObserved -and $mutations[0].ExpectedResult -eq 9 -and $mutations[0].RequestOperation -eq 7) 'receipt stays unacknowledged'
                $u=Read-TraceJson (Join-Path $dir 'host-ambiguity.json');Require-Trace ($u.ReceiptObserved -and $u.FinalAckTimedOut -and $u.DispatchDisabled -and $u.RetryRefused -and -not $u.FallbackControlSent) 'unknown outcome disables retries'
            }else{Require-Trace ($p.FinalAcknowledgmentObserved -and $mutations[0].ExpectedResult -eq 1) 'acknowledged close'}
            $read=Read-TraceJson (Join-Path $dir 'close-race-read.json');$next=Read-TraceJson (Join-Path $dir 'close-race-next.json')
            Review-TracePacket $read $id 'close read';Review-TracePacket $next $id 'close next'
            $computed=(Test-GuardCanceled $read) -and (Test-GuardNext $next 'NEXT_PROBE' 1) -and $p.ReadCanceled -and $p.NextReadPreserved
        } elseif($file -eq 'denial.json') {
            $k=Read-TraceJson (Join-Path $dir 'kernel-access-denied.json')
            Require-Trace ($k.Kind -eq 'H3KernelAccessDenial' -and $k.Thread -eq $id.Thread -and $k.Host -eq $id.Host -and $k.DesiredAccess -eq 1050624 -and $k.Win32Error -eq 5 -and $k.KernelDenied -and $k.TokenRestricted -and $k.SameAuthenticatedUser -and $k.RestrictingSids -eq 1 -and $k.ImpersonationReverted -and $k.OriginalAccessRestored -and -not $k.HostSecurityChanged -and -not $k.HookInstalled -and -not $k.HostWriteStarted -and -not $k.InjectedProtocolDenial -and $mutations.Count -eq 0) 'kernel denial without host mutation'
            $before=Read-TraceJson (Join-Path $dir 'denial-read.json');$recovery=Read-TraceJson (Join-Path $dir 'denial-recovery.json');$next=Read-TraceJson (Join-Path $dir 'denial-next.json')
            foreach($packet in @($before,$recovery,$next)){Review-TracePacket $packet $id 'denied setup'}
            $computed=-not $before.ReadReturned -and $before.ControlCount -eq 0 -and (Test-GuardNext $recovery 'accesskept' 0) -and (Test-GuardNext $next 'NEXT_PROBE' 0) -and $p.KernelDenied -and $p.PendingReadPreserved -and $p.OriginalTextPreserved -and $p.NextReadPreserved -and $p.CancellationDispatches -eq 0 -and -not $p.RetrySent -and -not $p.FallbackControlSent
        } elseif($file -eq 'child-guard.json') {
            $identity=Read-TraceJson (Join-Path $dir 'child\identity.json');$childId=$identity.Identity;$life=@($lifetimes | Where-Object {$_.GuardChild -eq $p.Child -and $_.Identity.Fixture -eq $id.Fixture})
            Require-Trace ($life.Count -eq 1 -and $life[0].GuardChildCreated -ceq $identity.Created -and $life[0].GuardChildImage -ceq $identity.Image -and $life[0].Shell -eq $p.ParentShell -and $identity.ParentShell -eq $p.ParentShell -and $childId.ProcessId -eq $p.Child -and $childId.ParentId -eq $p.ParentShell -and $identity.SameUserLogonIntegritySession) 'independently retained native grandchild'
            if($name -like '*ps7-*'){Require-Trace ($p.ShellIdentity.StartsWith('7.2.24|') -and $life[0].ShellImage -ieq $Root.PowerShell7Path) 'pinned PowerShell 7 parent'}
            $binding=[pscustomobject]@{Fixture=$p.Child;Agent=$p.ParentShell;Host=$id.Host}
            $ready=Read-TraceJson (Join-Path $dir 'child\pending.json');$final=Read-TraceJson (Join-Path $dir 'child\final.json')
            foreach($packet in @($childId,$ready,$final)){
                Review-TracePacket $packet $binding 'owned child'
                Require-Trace ($packet.CtrlIgnoreExplicitlyReset -and @($packet.ConsoleMembers).Count -eq 4 -and (($packet.ConsoleMembers | Sort-Object) -join ',') -eq ((@($p.Child,$p.ParentShell,$id.Fixture,$id.Agent) | Sort-Object) -join ',')) 'child console and handler policy'
            }
            $hostPath=$p.InputRoute -eq 'HostCancellation'
            Require-Trace ($p.InputRoute -in @('HostCancellation','PublicETX') -and [bool]$p.ReadCancellationRequired -eq $hostPath -and -not $p.PublicCookedReadCancellationClaimed -and $p.CancellationDispatches -eq $(if($hostPath){1}else{0}) -and $p.PublicETXWrites -eq $(if($hostPath){0}else{1}) -and $mutations.Count -eq $(if($hostPath){1}else{0}) -and -not $p.FallbackControlSent) 'child route and read contract'
            $childInputMatches=$final.ControlCount -eq 1 -and $final.ControlType -eq 0 -and -not $ready.ReadReturned -and $ready.ControlCount -eq 0 -and ($ready.InputMode -band 1) -ne 0
            if($hostPath){$next=Read-TraceJson (Join-Path $dir 'child\next.json');Review-TracePacket $next $binding 'child next';$childInputMatches=$childInputMatches -and (Test-GuardCanceled $final) -and (Test-GuardNext $next 'GUARD_NEXT' 1) -and $p.ChildReadCanceled -and $p.ChildNextReadPreserved}
            else{$childInputMatches=$childInputMatches -and $ready.ReadIndex -eq 0 -and -not $ready.Trace.ReadEntered -and -not $final.ReadReturned -and $p.ChildFinalOutput}
            $handler=@($final.Trace.Events | Where-Object {$_.Kind -eq 5})
            $withinDeadline=$handler.Count -eq 1 -and [decimal]$p.ControlBeginQpc -gt 0 -and [decimal]$p.ControlEndQpc -ge [decimal]$p.ControlBeginQpc -and
                [decimal]$p.Frequency -eq [decimal]$final.Trace.Frequency
            if($withinDeadline){$withinDeadline=[decimal]$handler[0].Qpc -ge [decimal]$p.ControlBeginQpc -and [decimal]$handler[0].Qpc -le [decimal]$p.ControlEndQpc+2*[decimal]$p.Frequency}
            $childInputMatches=$childInputMatches -and $withinDeadline
            $parent=Read-TraceJson (Join-Path $dir 'parent-events.json');Review-TracePacket $parent $id 'parent after child'
            $status=([IO.File]::ReadAllText((Join-Path $dir 'parent-child-exit.txt'))).Trim()
            $computed=$childInputMatches -and $p.OriginalDeadlineMet -and $p.ChildControlMatches -and $p.ChildExited -and $p.ChildExitCode -eq 37 -and $p.ParentReportedExit -ceq '37' -and $status -ceq '37' -and $p.ParentPrompt -and $p.ParentContinued -and $p.ParentAlive -and $parent.ControlCount -eq 1 -and $p.FixtureControlCount -eq 1 -and ([IO.File]::ReadAllText((Join-Path $dir 'parent-continued.txt'))).Trim() -ceq 'CONTINUE'
        } elseif($file -eq 'shell.json') {
            $event=Read-TraceJson (Join-Path $dir 'event-observation.json');Review-TracePacket $event $id 'native PS7 event'
            $generation=Read-TraceJson (Join-Path $dir 'prompt-generations.json')
            $shells=@($lifetimes | Where-Object {$_.Identity.Fixture -eq $id.Fixture -and $_.Shell -ne 0 -and $_.GuardChild -eq 0})
            Require-Trace ($p.ShellIdentity.StartsWith('7.2.24|0|ConsoleHost') -and $shells.Count -eq 1 -and $shells[0].ShellImage -ieq $Root.PowerShell7Path -and $mutations.Count -eq 1 -and $mutations[0].RequestOperation -eq 5) 'pinned native PS7 editor and host route'
            $computed=$p.FreshPrompt -and $p.CanceledCommandAbsent -and -not (Test-Path -LiteralPath (Join-Path $dir 'canceled-command.txt')) -and $p.NextCommand -and
                $p.ShellAlive -and $p.KeyboardRestored -and $p.FixtureControlCount -eq 1 -and $event.ControlCount -eq 1 -and $event.ControlType -eq 0 -and
                $generation.PowerShellCounter -and $generation.AtOriginalDeadline -gt $generation.Before -and
                ([IO.File]::ReadAllText((Join-Path $dir 'continued-command.txt'))).Trim() -ceq 'CONTINUE'
        } elseif($file -eq 'editor-guard.json') {
            $before=Read-TraceJson (Join-Path $dir 'editor-before.json');$after=Read-TraceJson (Join-Path $dir 'editor-after.json')
            Review-TracePacket $before $id 'editor before';Review-TracePacket $after $id 'editor after'
            Require-Trace ($p.ShellIdentity.StartsWith('7.2.24|') -and $p.ShellIdentity.EndsWith('|Windows|SaveNothing') -and $p.ShellIdentity -notmatch '\|none\|' -and $mutations.Count -eq 0 -and $p.CancellationDispatches -eq 0 -and $p.PublicETXWrites -eq 1 -and -not $p.FallbackControlSent) 'PSReadLine uses public input'
            $computed=($before.InputMode -band 1) -eq 0 -and $p.RawInputObserved -and $p.FreshPrompt -and $p.CanceledCommandAbsent -and -not (Test-Path -LiteralPath (Join-Path $dir 'canceled-command.txt')) -and $p.NextCommand -and $p.ShellAlive -and $p.FixtureControlCount -eq 0 -and $after.ControlCount -eq 0 -and ([IO.File]::ReadAllText((Join-Path $dir 'parent-continued.txt'))).Trim() -ceq 'CONTINUE'
        }
        if($case.Passed){Require-Trace ([bool]$computed) ('passing guard contradicts raw evidence: '+$name)}
        $rows.Add([pscustomobject]@{Case=$name;NativePassed=$case.Passed;PredicatesMet=[bool]$computed;OriginalError=$case.Error})
    }
    return [ordered]@{SchemaVersion=1;Kind='H3GuardNativeReview';GuardTier=$Root.GuardTier;PowerShell7Required=$ps7;ExpectedCases=$policy.ExpectedCases;CompletedContenders=$completed;CanceledContenders=$canceled;Guards=$rows.ToArray();IncompleteGuards=$missing.ToArray();AllNativeCasesPassed=(@($Cases.Cases | Where-Object {-not $_.Passed}).Count -eq 0);Qualified=($Root.Status -eq 'Pass' -and @($Cases.Cases | Where-Object {-not $_.Passed}).Count -eq 0 -and $missing.Count -eq 0 -and @($rows.ToArray() | Where-Object {-not $_.PredicatesMet -or -not $_.NativePassed}).Count -eq 0);MilestoneAccepted=$false}
}
