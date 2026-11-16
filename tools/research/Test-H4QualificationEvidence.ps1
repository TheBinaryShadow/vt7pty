[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$ExpectedSource='',
    [ValidateRange(1,120)][int]$MinimumSoakMinutes=120,
    [string]$OutputPath=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot 'H4Cases.ps1')
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
$crlf=[string][char]13+[char]10
function Require-Q([bool]$Condition,[string]$Message){if(-not $Condition){throw ('H4 qualification evidence: '+$Message)}}
function Read-Q([string]$Relative){[IO.File]::ReadAllText((Join-Path $Directory $Relative)) | ConvertFrom-Json}
function Preflight-Q([string]$Relative){
    $r=Read-Q $Relative
    Require-Q ($r.Eligible -and $r.WindowStation -ceq 'WinSta0' -and $r.CallerDesktop -ceq $r.InputDesktop -and
        $r.InputDesktopError -eq 0 -and $r.IntegrityRid -eq 8192 -and -not $r.Impersonated -and
        $r.SourceCommit -ceq $ExpectedSource) ('preflight '+$Relative)
}
function Resources-Q($Before,$After){
    Require-Q ($After.Handles -le $Before.Handles+8 -and $After.UserObjects -le $Before.UserObjects+2 -and
        $After.GdiObjects -le $Before.GdiObjects+2 -and
        [uint64]$After.PrivateBytes -le [uint64]$Before.PrivateBytes+67108864) 'caller resource budgets'
}
function Canceled-Q($p,[int]$Events){
    Require-Q ($p.ReadReturned -eq 1 -and $p.ReadError -eq 995 -and $p.Text -ceq '' -and
        $p.ControlCount -eq $Events -and $p.ControlType -eq 0 -and $p.Trace.ReadEntered -eq 1 -and
        $p.Trace.Overflow -eq 0 -and @($p.Trace.Events | Where-Object {$_.Kind -eq 5 -and $_.Detail -eq $Events}).Count -eq 1) 'canceled read and handler exit'
}
function Next-Q($p,[string]$Text,[int]$Events){
    Require-Q ($p.ReadReturned -eq 1 -and $p.ReadSucceeded -eq 1 -and $p.ReadError -eq 0 -and
        $p.Text -ceq ($Text+$crlf) -and $p.ControlCount -eq $Events -and $p.Trace.Overflow -eq 0) 'exact subsequent read'
}
function Keyboard-Q([string]$Before,[string]$During,[string]$After){
    Require-Q ($Before -cmatch '^[0-9a-f]{512}$' -and $Before -ceq $After -and $During -cmatch '^[0-9a-f]{512}$') 'all keyboard bytes restored'
    $bytes=New-Object byte[] 256
    for($i=0;$i -lt 256;$i++){$bytes[$i]=[Convert]::ToByte($Before.Substring($i*2,2),16)}
    foreach($vk in @(16,160,161,17,162,163,18,164,165,91,92)){$bytes[$vk]=$bytes[$vk] -band 1}
    $bytes[17]=$bytes[17] -bor 128;$bytes[162]=$bytes[162] -bor 128
    Require-Q ([BitConverter]::ToString($bytes).Replace('-','').ToLowerInvariant() -ceq $During) 'exact temporary Ctrl state'
}
function Trace-Q {
    Require-Q (([IO.File]::ReadAllText((Join-Path $Directory 'collector-errors.txt'))).Length -eq 0) 'collector errors'
    $ready=@{};$active=@{};$keys=@{};$unknown=@{}
    foreach($line in [IO.File]::ReadLines((Join-Path $Directory 'runtime-trace.jsonl'))){
        $r=$line | ConvertFrom-Json
        Require-Q ($r.commit -ceq $ExpectedSource -and $r.api -ceq '1.1' -and $r.protocol -eq 2) 'trace source/API/protocol'
        if($r.message -match '^host-cancel ready host=(\d+) thread=(\d+) desktop=(VT7Pty-Console-[0-9a-f]{32})$'){
            $context=@{Host=[int]$Matches[1];Thread=[int]$Matches[2];Desktop=$Matches[3];Agent=[int]$r.pid}
            Require-Q (-not $ready.ContainsKey($context.Desktop)) 'unique trace session'
            $ready[$context.Desktop]=$context;$active[[string]$r.pid]=$context
        }elseif($r.message -match '^host-cancel result sequence=(\d+) result=(\d+) host=(\d+) thread=(\d+) captured=(\d+) applied=(\d+) sent=(\d+) restored=(\d+) equal=(\d+) error=(\d+)$'){
            $seq=$Matches[1];$context=$active[[string]$r.pid]
            Require-Q ($null -ne $context -and $Matches[2] -eq '1' -and [int]$Matches[3] -eq $context.Host -and
                [int]$Matches[4] -eq $context.Thread -and ($Matches[5..9] -join '') -ceq '11111' -and $Matches[10] -eq '0') 'restored result context'
            $key=$context.Desktop+':'+$seq
            Require-Q (-not $keys.ContainsKey($key)) 'one result per session sequence'
            $keys[$key]=@{Context=$context}
        }elseif($r.message -match '^host-cancel (before|during|after) sequence=(\d+) keys=([0-9a-f]{512})$'){
            $which=$Matches[1];$sequence=$Matches[2];$hex=$Matches[3]
            $context=$active[[string]$r.pid];Require-Q ($null -ne $context) 'snapshot session ready'
            $key=$context.Desktop+':'+$sequence
            Require-Q ($keys.ContainsKey($key) -and -not $keys[$key].ContainsKey($which)) ('one keyboard snapshot stage '+$key+' '+$which+'; resultPresent='+$keys.ContainsKey($key))
            $keys[$key][$which]=$hex
        }elseif($r.message -match '^host-cancel unavailable sequence=(\d+) outcome=unknown retry=disabled$'){
            $context=$active[[string]$r.pid];Require-Q ($null -ne $context) 'unknown outcome retained session'
            $unknown[$context.Desktop]=[int]$Matches[1]
        }
    }
    $identities=@{};$retainedHosts=@{};$sessions=0
    foreach($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter identity.json -Recurse)){
        $id=[IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json
        $desk=[IO.File]::ReadAllText((Join-Path $file.DirectoryName 'desktop.json')) | ConvertFrom-Json
        $life=[IO.File]::ReadAllText((Join-Path $file.DirectoryName 'lifetime.json')) | ConvertFrom-Json
        Require-Q ($id.IdentityEligible -and $id.SameUserLogonIntegritySession -and $id.NativeX64 -and
            $life.OwnedProcessesCleaned -and $life.HostExited -and -not $life.Error -and
            $life.Host -eq $id.HostProcessId -and $life.Agent -eq $id.Fixture.ParentId -and
            $life.Fixture -eq $id.Fixture.ProcessId -and $desk.ApplicationGuiOrdinary -and $desk.HostWindowPrivate) 'retained session lifecycle'
        Require-Q ($ready.ContainsKey($desk.BackingDesktop) -and
            $ready[$desk.BackingDesktop].Host -eq $id.HostProcessId -and
            $ready[$desk.BackingDesktop].Agent -eq $id.Fixture.ParentId) 'retained session matches readiness'
        $hostKey=[string]$id.HostProcessId+':'+$id.HostCreationTime
        Require-Q (-not $identities.ContainsKey($desk.BackingDesktop) -and -not $retainedHosts.ContainsKey($hostKey) -and
            [uint64]$id.HostCreationTime -gt 0 -and -not $id.Error -and $desk.ApplicationDesktop -ceq (Read-Q 'preflight.json').CallerDesktop) 'unique retained host creation and ordinary application desktop'
        $retainedHosts[$hostKey]=$true
        $identities[$desk.BackingDesktop]=$id;$sessions++
    }
    foreach($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter caller.json -Recurse)){
        $r=[IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json
        $key=[string]$r.Host+':'+$r.HostCreated
        Require-Q ($r.AllOwnedProcessesExited -and $r.HostExitedWithoutTermination -and
            $r.Actor -gt 0 -and [uint64]$r.ActorCreated -gt 0 -and [uint64]$r.AgentCreated -gt 0 -and
            [uint64]$r.ChildCreated -gt 0 -and [uint64]$r.HostCreated -gt 0 -and
            -not $retainedHosts.ContainsKey($key) -and -not $identities.ContainsKey($r.BackingDesktop) -and
            $ready.ContainsKey($r.BackingDesktop) -and $ready[$r.BackingDesktop].Host -eq $r.Host -and
            $ready[$r.BackingDesktop].Agent -eq $r.Agent) 'retained job actor host creation and readiness'
        $identities[$r.BackingDesktop]=$r;$retainedHosts[$key]=$true;$sessions++
    }
    Require-Q ($ready.Count -eq $sessions) 'all ready contexts retain lifecycle evidence'
    foreach($key in $keys.Keys){
        $k=$keys[$key]
        Require-Q ($k.ContainsKey('before') -and $k.ContainsKey('during') -and $k.ContainsKey('after')) 'complete keyboard record'
        Keyboard-Q $k.before $k.during $k.after
        Require-Q ($identities.ContainsKey($k.Context.Desktop)) 'keyboard operation retains host identity'
    }
    return @{Ready=$ready;Keys=$keys;Unknown=$unknown;Identities=$identities;Sessions=$sessions}
}
$probe=Read-Q 'probe.json'
if(-not $ExpectedSource){$ExpectedSource=$probe.SourceCommit}
Require-Q ($probe.Status -eq 'Pass' -and $probe.SourceCommit -ceq $ExpectedSource -and -not $probe.H4Accepted) 'native identity and outcome'
$summary=[ordered]@{Kind='H4QualificationReview';Probe=$probe.Kind;Status='Pass';SourceCommit=$ExpectedSource;H4Accepted=$false}
if($probe.Kind -eq 'H4QualificationProbe'){
    $cases=(Read-Q 'cases.json').Cases;$expected=@(Get-H4QualificationNames)
    Require-Q (($cases.Case -join '|') -ceq ($expected -join '|') -and $probe.Total -eq $expected.Count -and
        $probe.Passed -eq $probe.Total -and @($cases | Where-Object {-not $_.Passed -or -not $_.OwnedProcessesCleaned -or -not $_.HostExited -or $_.Error}).Count -eq 0) 'qualification case inventory and cleanup'
    Preflight-Q 'preflight.json'
    foreach($name in $expected){Preflight-Q ($name+'\preflight.json')}
    $denial=Read-Q 'inactive-desktop\denial.json'
    Require-Q ($denial.Denied -and -not $denial.DesktopSwitched -and -not $denial.Preflight.Eligible -and
        $denial.Preflight.CallerDesktop -cmatch '^VT7Pty-H4-Inactive-' -and $denial.Preflight.CallerDesktop -cne $denial.Preflight.InputDesktop -and
        $denial.Message.Contains('ordinary caller desktop') -and $denial.Message.Contains($denial.Preflight.CallerDesktop)) 'specific inactive desktop denial'
    Preflight-Q 'inactive-desktop\recovery-preflight.json'
    $resources=Read-Q 'concurrent-resources\resources.json'
    Require-Q ($resources.ConcurrentSessions -eq 8 -and $resources.DispatchSequencedForCompleteTrace) 'eight simultaneous sessions'
    Resources-Q $resources.Before $resources.After
    foreach($job in @(@('job-explicit-breakaway-allowed',2048,$true),@('job-silent-breakaway',4096,$false))){
        $r=Read-Q ($job[0]+'\caller.json')
        Require-Q ($r.AllOwnedProcessesExited -and $r.HostExitedWithoutTermination -and $r.EnclosingJobPreserved -and
            $r.JobLimitFlags -eq $job[1] -and $r.ObservedJobLimitFlags -eq ($job[1] -bor 8192) -and
            -not $r.ExplicitBreakawayRequested -and $r.ActorInJob -and
            $r.AgentInJob -eq $job[2] -and $r.FixtureInJob -eq $job[2]) 'native enclosing-job policy'
    }
    $trace=Trace-Q
    foreach($pair in @(@('timeout-before-dispatch',1),@('timeout-during-callback',2),@('agent-death-during-callback',3))){
        $f=Read-Q ($pair[0]+'\fault.json');$id=Read-Q ($pair[0]+'\identity.json');$d=Read-Q ($pair[0]+'\desktop.json')
        Require-Q ($f.Mode -eq $pair[1] -and $f.GateInitialized -eq 1 -and $f.GateEntered -eq 1 -and $f.GateFinished -eq 1 -and
            $f.BeforeValid -and $f.AfterValid -and $f.GateError -eq 0 -and $f.KeyboardRestored -and $f.TemporaryCtrlVerified -and
            $f.ChildExited -and $f.HostExitedWithoutTermination -and $f.PostFailureRpcRefused -and
            $f.ExpectedHost -eq $id.HostProcessId -and $f.ActualHost -eq $id.HostProcessId -and
            $f.ActualThread -eq $f.ExpectedThread -and $trace.Ready[$d.BackingDesktop].Thread -eq $f.ActualThread -and
            [uint64]$f.ReleasedTick -ge [uint64]$f.EnteredTick -and
            [uint64]$f.ReleasedTick - [uint64]$f.EnteredTick -lt 5000) 'retained actual callback and bounded rundown'
        if($pair[1] -eq 1){
            Require-Q ($f.CMessageCount -eq 0 -and -not $f.DuringValid -and $f.Before -ceq $f.After -and
                [uint64]$f.ReleasedTick - [uint64]$f.EnteredTick -ge 3000) 'expired early request has no key dispatch'
        }else{
            Require-Q ($f.CMessageCount -eq 1 -and $f.DuringValid) 'one in-flight C'
            Keyboard-Q $f.Before $f.During $f.After
        }
        if($pair[1] -eq 3){Require-Q $f.AgentExitedBeforeRelease 'agent death while callback active'}
        else{
            Require-Q ($f.UnavailableBeforeRelease -and $trace.Unknown.ContainsKey($d.BackingDesktop) -and
                [uint64]$f.UnavailableObservedTick -ge [uint64]$f.EnteredTick -and
                [uint64]$f.UnavailableObservedTick -le [uint64]$f.ReleasedTick) 'production unknown result disables dispatch before release'
        }
    }
    $summary.Cases=$expected.Count;$summary.KeyboardOperations=$trace.Keys.Count;$summary.Sessions=$trace.Sessions
}elseif($probe.Kind -eq 'H4OptInSoak'){
    Require-Q ($probe.Minutes -ge $MinimumSoakMinutes -and [uint64]$probe.ElapsedMilliseconds -ge $probe.Minutes*60000 -and
        $probe.Cycles -ge 2 -and $probe.LongLivedActiveAndNeighbor) 'full requested opt-in duration'
    Resources-Q $probe.Before $probe.After
    $cold=Read-Q 'resources-cold.json';$baseline=Read-Q 'resources-baseline.json';$final=Read-Q 'resources-final.json'
    foreach($field in @('Handles','UserObjects','GdiObjects','PrivateBytes')){
        Require-Q ($probe.Before.$field -eq $baseline.$field -and $probe.After.$field -eq $final.$field -and
            $cold.$field -ge 0) 'recorded cold, warm and final resource snapshots'
    }
    $cycles=@(Get-ChildItem -LiteralPath $Directory -Directory -Filter 'cycle-*' | Sort-Object {[int]$_.Name.Substring(6)})
    Require-Q ($cycles.Count -eq $probe.Cycles) 'complete soak cycles'
    $nextSequence=1
    for($i=0;$i -lt $cycles.Count;$i++){
        Require-Q ($cycles[$i].Name -ceq ('cycle-'+($i+1))) 'ordered soak directories'
        $relative=$cycles[$i].Name
        Preflight-Q ($relative+'\preflight.json')
        $cancel=Read-Q ($relative+'\cancel.json');$next=Read-Q ($relative+'\next-read.json')
        Canceled-Q $cancel ($i+1);Next-Q $next 'NEXT_PROBE' ($i+1)
        Require-Q ($cancel.ReadIndex -eq 2*$i+1 -and $next.ReadIndex -eq 2*$i+2) 'continuous long-lived read indices'
        foreach($r in @($cancel,$next)){
            $t=$r.Trace
            Require-Q ($t.Windowed -and $t.OldestSequence -le $nextSequence -and $t.NextSequence -ge $nextSequence -and
                @($t.Events | Where-Object {$_.Sequence -ge $nextSequence}).Count -eq $t.NextSequence-$nextSequence) 'no lost fixture trace window'
            $nextSequence=[int]$t.NextSequence
        }
        $neighbor=Read-Q ($relative+'\neighbor.json')
        Require-Q ($neighbor.ReadReturned -eq 0 -and $neighbor.ControlCount -eq 0) 'long-lived pending neighbor'
    }
    Require-Q ($probe.RecreatedSessions -eq [Math]::Floor(($probe.Cycles-1)/10)+1) 'periodic session recreation'
    Next-Q (Read-Q 'neighbor\neighbor-read.json') 'SOAK_NEIGHBOR' 0
    $trace=Trace-Q
    Require-Q ($trace.Unknown.Count -eq 0 -and $trace.Keys.Count -eq $probe.Cycles+$probe.RecreatedSessions+1 -and
        $trace.Sessions -eq $probe.RecreatedSessions+3) 'all opt-in cancellations and live-session identities'
    $summary.Minutes=$probe.Minutes;$summary.Cycles=$probe.Cycles;$summary.KeyboardOperations=$trace.Keys.Count
    $summary.Sessions=$trace.Sessions;$summary.WarmupRecorded=$true
}elseif($probe.Kind -eq 'H4ManualKeyboard'){
    Preflight-Q 'preflight.json'
    $expectedLayout=if($probe.DeclaredLayout -ceq 'US'){1033}elseif($probe.DeclaredLayout -ceq 'HR'){1050}else{0}
    Require-Q ($expectedLayout -ne 0 -and $probe.LayoutLanguage -eq $expectedLayout -and $probe.PhysicalInputRequired) 'physical layout profile'
    $text=if($expectedLayout -eq 1033){'AZaz09[]{}\|'}else{([string][char]269+[char]263+[char]273+[char]353+[char]382+' '+[char]268+[char]262+[char]272+[char]352+[char]381+' @[]{}\|')}
    $textKeys=Read-Q 'text-keys.json'
    Next-Q (Read-Q 'text-read.json') $text 0
    Require-Q ($textKeys.Source -ceq 'PhysicalConsoleInput' -and -not $textKeys.SyntheticInputUsed -and
        $textKeys.LayoutLanguage -eq $expectedLayout -and $textKeys.ForwardedUtf8 -ceq ($text+[char]13) -and
        $textKeys.ShiftObserved -and @($textKeys.Keys).Count -ge $text.Length) 'physical text and Shift records'
    if($expectedLayout -eq 1050){
        Require-Q ($textKeys.AltGrObserved -and @($textKeys.Keys | Where-Object {$_.Down -and $_.Character -ne 0 -and
            ($_.Modifiers -band 1) -ne 0 -and ($_.Modifiers -band 12) -ne 0}).Count -gt 0) 'Croatian AltGr key records'
        $dead=Read-Q 'dead-keys.json'
        Next-Q (Read-Q 'dead-read.json') ([string][char]225) 0
        Require-Q ($dead.LayoutLanguage -eq 1050 -and $dead.DeadPrefixObserved -and
            $dead.ForwardedUtf8 -ceq ([string][char]225+[char]13) -and
            @($dead.Keys | Where-Object {$_.Down -and $_.Character -eq 0 -and
                $_.VirtualKey -notin @(16,17,18,160,161,162,163,164,165)}).Count -gt 0) 'physical dead-key composition'
    }
    foreach($name in @('cancel','next','raw')){
        $k=Read-Q ($name+'-keys.json')
        Require-Q ($k.Source -ceq 'PhysicalConsoleInput' -and -not $k.SyntheticInputUsed -and
            $k.LayoutLanguage -eq $expectedLayout -and @($k.Keys).Count -gt 0 -and @($k.Keys).Count -le 512) ('physical '+$name+' records')
    }
    Require-Q ((Read-Q 'cancel-keys.json').ForwardedUtf8 -ceq ('OLD'+[char]3)) 'physical cancellation input'
    Canceled-Q (Read-Q 'canceled-read.json') 1
    Next-Q (Read-Q 'next-read.json') 'NEXT' 1
    $raw=Read-Q 'raw-read.json'
    Require-Q ($raw.ReadSucceeded -eq 1 -and $raw.ReadError -eq 0 -and $raw.ControlCount -eq 1 -and
        $raw.KeyCount -eq 3 -and (@($raw.Keys | ForEach-Object {$_.Character}) -join ',') -ceq '0,3,122' -and
        (@($raw.Keys | ForEach-Object {$_.VirtualKey}) -join ',') -ceq '17,67,90') 'physical raw routing'
    Next-Q (Read-Q 'session\next-read.json') 'NEXT_PROBE' 1
    $trace=Trace-Q
    Require-Q ($trace.Keys.Count -eq 1 -and $trace.Unknown.Count -eq 0 -and $trace.Sessions -eq 1) 'manual cooked/raw host mutation boundary'
    $summary.Layout=$probe.DeclaredLayout;$summary.KeyboardOperations=$trace.Keys.Count
}elseif($probe.Kind -eq 'H4ManualSession'){
    Preflight-Q 'preflight.json'
    $states=(Read-Q 'transitions.json').States
    Require-Q (@($states | Where-Object {-not $_.Eligible}).Count -ge 1 -and $states[0].Eligible -and
        $states[$states.Count-1].Eligible -and $probe.InactiveObserved -and $probe.Recovered -and
        $probe.StartupDeniedWhileInactive -and $probe.PendingReadPreserved -and -not $probe.DesktopSwitchedByTest) 'observed lock/unlock transition'
    $denied=Read-Q 'inactive-open.json'
    Require-Q ($denied.Denied -and $denied.ErrorCode -ne 0 -and
        ($denied.Message.Contains('ordinary caller desktop') -or $denied.Message.Contains('input desktop'))) 'specific inactive startup refusal'
    $pending=Read-Q 'pending-after-unlock.json'
    Require-Q ($pending.ReadReturned -eq 0 -and $pending.ControlCount -eq 0 -and $pending.Trace.ReadEntered -eq 1) 'pending line preserved during transition'
    Canceled-Q (Read-Q 'cancel-after-unlock.json') 1
    Next-Q (Read-Q 'existing\next-read.json') 'NEXT_PROBE' 1
    $trace=Trace-Q
    Require-Q ($trace.Keys.Count -eq 2 -and $trace.Unknown.Count -eq 0 -and $trace.Sessions -eq 2) 'recovery and newly opened session'
    $summary.KeyboardOperations=$trace.Keys.Count;$summary.SessionTransition='Observed'
}elseif($probe.Kind -eq 'H4ManualAccess'){
    $preflight=Read-Q 'preflight.json'
    Require-Q ($probe.IntegrityRid -gt 8192 -and $preflight.IntegrityRid -eq $probe.IntegrityRid -and
        -not $preflight.Eligible -and -not $preflight.Impersonated -and
        $preflight.CallerDesktop -ceq $preflight.InputDesktop -and $preflight.WindowStation -ceq 'WinSta0' -and
        $probe.OptInDenied -and $probe.ErrorCode -ne 0 -and
        $probe.Message.Contains('medium-integrity interactive caller') -and $probe.FlaglessOpenPassed -and
        $probe.ObservedHostExited) 'actual elevated caller refusal and flagless cleanup'
    $summary.IntegrityRid=$probe.IntegrityRid;$summary.OptInDenied=$true
}else{
    throw ('H4 qualification evidence: unsupported native probe '+$probe.Kind)
}
if($OutputPath){
    $path=[IO.Path]::GetFullPath($OutputPath)
    Require-Q (-not $path.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside original evidence'
    [IO.File]::WriteAllText($path,($summary | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
}
$summary
