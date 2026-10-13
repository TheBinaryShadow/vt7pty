[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$OutputPath=''
)
# Developer-side evidence review, Windows PowerShell 5.1 or later.
# The portable Windows 7 runner remains compatible with PowerShell 2.0.
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
function Read-Record([string]$Relative) {
    return [IO.File]::ReadAllText((Join-Path $Directory $Relative)) | ConvertFrom-Json
}
function Require([bool]$Condition,[string]$Message) {
    if(-not $Condition) { throw ('H3 evidence: '+$Message) }
}
function Same-Identity($a,$b) {
    foreach($property in $a.PSObject.Properties) {
        Require ($property.Value -ceq $b.($property.Name)) ('identity mismatch: '+$property.Name)
    }
}
function Read-Observation([string]$Path,[string]$Text,[int]$Events,[int]$Index,[bool]$Canceled=$false) {
    $p=Read-Record $Path
    $binding=$bindings[[IO.Path]::GetDirectoryName($Path)]
    Require ($p.ProcessId -eq $binding.Fixture -and $p.ParentId -eq $binding.Agent -and $p.SelfQuery.NormalizedPid -eq $binding.Host) ('read session: '+$Path)
    Require ($p.ReadReturned -eq 1 -and $p.ReadIndex -eq $Index -and $p.Text -ceq $Text -and $p.ControlCount -eq $Events) $Path
    if($Canceled) {
        Require ($p.ReadError -eq 995 -and $p.ControlType -eq 0 -and $p.ReadSucceeded -in @(0,1)) ('aborted read: '+$Path)
    } else {
        Require ($p.ReadSucceeded -eq 1 -and $p.ReadError -eq 0) ('ordinary read: '+$Path)
    }
    return $p
}
$root=Read-Record 'probe.json'
Require ($root.SchemaVersion -eq 1 -and $root.Probe -eq 'HostCancellation' -and $root.Version -eq 1 -and $root.Status -eq 'Pass') 'root status'
Require ($root.SuiteExit -eq 0 -and $root.NeighborExit -eq 0 -and $root.NeighborPolls -eq 17 -and
    $root.DesktopIsolationVerified -and $root.OwnedLifetimesCleaned -and $root.RootDesktop -eq 'Default' -and
    -not $root.MilestoneAccepted -and -not $root.ProductionCancellationImplemented -and $root.Error -eq '') 'root controls'
Require ($root.SuiteDesktop -cmatch '^VT7Pty-H3-[0-9a-f]{32}$' -and $root.NeighborDesktop -cmatch '^VT7Pty-H3-[0-9a-f]{32}$' -and
    $root.SuiteDesktop -cne $root.NeighborDesktop) 'separate private desktops'
$expected=@('cooked-repeated','no-pending-read','completed-before-cancel','receipt-without-ack','close-in-request','default-handler','raw-input-guard','shell-owner-exit','cmd-empty','cmd-partial','powershell-empty','powershell-partial')
$cases=Read-Record 'suite\cases.json'
Require ($cases.SchemaVersion -eq 1 -and $cases.Passed -and (@($cases.Cases | ForEach-Object { $_.Case }) -join '|') -ceq ($expected -join '|')) 'case inventory'
foreach($case in $cases.Cases) {
    Require ($case.Passed -and $case.OwnedProcessesCleaned -and $case.HostExited -and $case.HookRemoved -and $case.Error -eq '') $case.Case
}
$contexts=@{}
$bindings=@{}
$accepted=0; $rejected=0; $received=0; $restored=0; $events=0
$locations=@(@{Path='neighbor';Label='neighbor';Desktop=$root.NeighborDesktop})
$locations+=@($expected | ForEach-Object { @{Path=('suite\'+$_);Label='host';Desktop=$root.SuiteDesktop} })
foreach($location in $locations) {
    $prefix=$location.Path+'\'+$location.Label
    $hello=Read-Record ($prefix+'-hello.json')
    $identity=$hello.Identity
    $bindings[$location.Path]=$identity
    Require ($hello.Response.Result -eq 0 -and $hello.Response.Sequence -eq 0 -and $hello.Response.AcceptedSequence -eq 0) 'hello state'
    Require ($hello.Desktop -ceq $location.Desktop -and $hello.ControllerDesktop -ceq $location.Desktop -and $hello.PipePeer -eq $identity.Host) 'hello desktop/peer'
    Require (-not $contexts.ContainsKey([string]$identity.Host)) 'unique live host identity'
    $contexts[[string]$identity.Host]=$identity
    $h1=Read-Record ($prefix+'-identity.json')
    Require ($h1.IdentityEligible -and $h1.SameUserLogonIntegritySession -and $h1.NativeX64 -and
        $h1.HostProcessId -eq $identity.Host -and $h1.HostCreationTime -ceq $identity.HostCreated -and
        $h1.Fixture.ProcessId -eq $identity.Fixture -and $h1.Fixture.ParentId -eq $identity.Agent) 'H1 identity'
    foreach($query in @($h1.Fixture.SelfQuery,$h1.ChildQuery,$h1.AgentQuery)) {
        Require ($query.NtStatus -eq 0 -and $query.ReturnLength -eq 8 -and $query.NormalizedPid -eq $identity.Host -and
            $query.RawHostValue -eq $h1.ChildQuery.RawHostValue -and $query.LowBits -ge 0 -and $query.LowBits -le 3) 'common host query'
    }
    $discovery=Read-Record ($prefix+'-discovery.json')
    Require (-not $discovery.Overflow -and $discovery.FirstThread -eq $identity.Thread -and $discovery.SecondThread -eq $identity.Thread -and
        @($discovery.Events).Count -ge 2 -and @($discovery.Events).Count -le 128) 'repeatable dispatch discovery'
    foreach($event in $discovery.Events) {
        Require ($event.ThreadOwner -eq $identity.Host -and $event.EventThread -eq $identity.Thread -and
            $event.CallbackThread -eq $identity.ControllerThread -and $event.Window -eq $identity.Window -and
            $event.Object -eq 0 -and $event.Child -eq 0 -and $event.Scope -in @(1,2)) 'owned title event'
        $events++
    }
    $requests=@(Get-ChildItem -LiteralPath (Join-Path $Directory $location.Path) -Filter '*-request.json' -File |
        Sort-Object { [int]([regex]::Match($_.Name,'-(\d+)-request').Groups[1].Value) })
    $sequence=0
    $responses=@($hello.Response)
    foreach($file in $requests) {
        $q=[IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json
        Same-Identity $q.RequestIdentity $identity
        Require ($q.RequestSchema -eq 1 -and $q.RequestBytes -eq 112 -and
            $q.RequestNonceLow -ceq $hello.Response.NonceLow -and $q.RequestNonceHigh -ceq $hello.Response.NonceHigh) 'bound request'
        $r=$q.Response
        Require ($r.Result -eq $q.ExpectedResult -and $r.Sequence -eq $q.RequestSequence -and $r.Operation -eq $q.RequestOperation) 'request result correlation'
        if($r.Result -in @(1,9)) {
            Require ($q.RequestSequence -eq $sequence+1) 'accepted sequence'
            $sequence++
            if($r.Result -eq 1) { $accepted++ } else { $received++; Require ($r.Operation -eq 7) 'receipt-only operation' }
        } else {
            $rejected++
            Require ($r.Result -in @(5,6,10)) 'measured rejection'
        }
        Require ($r.AcceptedSequence -eq $sequence) 'rejected requests preserve sequence'
        if($r.Result -eq 1 -and $r.Operation -in @(5,6,8)) {
            $k=$r.Keyboard
            Require ($k.Captured -eq 1 -and $k.Applied -eq 1 -and $k.Restored -eq 1 -and $k.Equal -eq 1 -and $k.Error -eq 0 -and
                $k.Before -cmatch '^[0-9a-f]{512}$' -and $k.During -cmatch '^[0-9a-f]{512}$' -and $k.After -ceq $k.Before) 'full keyboard restoration'
            $temporary=New-Object byte[] 256
            for($i=0;$i -lt 256;$i++) { $temporary[$i]=[Convert]::ToByte($k.Before.Substring($i*2,2),16) }
            foreach($vk in @(16,160,161,17,162,163,18,164,165,91,92)) { $temporary[$vk]=$temporary[$vk] -band 1 }
            foreach($vk in @(17,162)) { $temporary[$vk]=$temporary[$vk] -bor 128 }
            Require (([BitConverter]::ToString($temporary).Replace('-','').ToLowerInvariant()) -ceq $k.During) 'exact temporary state'
            if($r.Operation -eq 6) { Require ($k.Sent -eq 0 -and $k.InjectedFailure -eq 1) 'failure restoration control' }
            else { Require ($k.Sent -eq 1 -and $k.InjectedFailure -eq 0) 'one host key message' }
            $restored++
        } else {
            Require ($r.Keyboard.Captured -eq 0 -and $r.Keyboard.Applied -eq 0 -and $r.Keyboard.Sent -eq 0) 'no false mutation evidence'
        }
        $responses+=@($r)
    }
    foreach($r in $responses) {
        Require ($r.Host -eq $identity.Host -and $r.Thread -eq $identity.Thread -and $r.HostCreated -ceq $identity.HostCreated -and
            $r.Window -eq $identity.Window -and $r.WindowOwner -eq $identity.Host -and $r.WindowThread -eq $identity.Thread -and
            $r.ActualDesktop -ceq $location.Desktop -and $r.NonceLow -ceq $hello.Response.NonceLow -and
            $r.NonceHigh -ceq $hello.Response.NonceHigh -and $r.ValidationStage -eq 0) 'actual callback context'
    }
}
Require ($contexts.Count -eq 13 -and $accepted -eq 34 -and $rejected -eq 5 -and $received -eq 1 -and $restored -eq 12) 'context/request/restoration totals'
$registrations=@(Get-ChildItem -LiteralPath (Join-Path $Directory 'suite') -Filter 'lifetime-*.json' -File)
$registrations+=@(Get-ChildItem -LiteralPath (Join-Path $Directory 'neighbor') -Filter 'lifetime-*.json' -File)
Require ($registrations.Count -eq 18) 'registered lifetime count'
$shells=0
foreach($file in $registrations) {
    $r=[IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json
    Same-Identity $r.Identity $contexts[[string]$r.Identity.Host]
    Require ($r.ConsoleWindowEnumeratedOnDesktop -and $r.Desktop -in @($root.SuiteDesktop,$root.NeighborDesktop)) 'independent desktop enumeration'
    if($r.Shell) { $shells++; Require ([decimal]$r.ShellCreated -ge [decimal]$r.Identity.FixtureCreated) 'owned shell creation' }
}
Require ($shells -eq 5) 'registered shells'
$crlf=[string][char]13+[char]10
for($i=1;$i -le 3;$i++) {
    $pending=Read-Record ('suite\cooked-repeated\cancel-'+$i+'-pending.json')
    Require ($pending.ReadReturned -eq 0 -and $pending.ReadIndex -eq 2*$i-1 -and $pending.ControlCount -eq $i-1) 'pending cooked barrier'
    [void](Read-Observation ('suite\cooked-repeated\cancel-'+$i+'-read.json') '' $i (2*$i-1) $true)
    [void](Read-Observation ('suite\cooked-repeated\cancel-'+$i+'-next.json') ('NEXT_PROBE'+$crlf) $i (2*$i))
}
[void](Read-Observation 'suite\no-pending-read\no-read-next.json' ('NEXT_PROBE'+$crlf) 1 1)
$noRead=Read-Record 'suite\no-pending-read\no-read.json'
Require ($noRead.ReadReturned -eq 0 -and $noRead.ReadIndex -eq 0 -and $noRead.ControlCount -eq 1 -and $noRead.ControlType -eq 0) 'event without read'
[void](Read-Observation 'suite\completed-before-cancel\before.json' ('COMPLETED'+$crlf) 0 1)
[void](Read-Observation 'suite\completed-before-cancel\after.json' ('COMPLETED'+$crlf) 1 1)
[void](Read-Observation 'suite\completed-before-cancel\completed-next.json' ('NEXT_PROBE'+$crlf) 1 2)
foreach($pair in @(@('receipt-without-ack','ambiguous'),@('close-in-request','closed'))) {
    [void](Read-Observation ('suite\'+$pair[0]+'\'+$pair[1]+'-read.json') '' 1 1 $true)
    [void](Read-Observation ('suite\'+$pair[0]+'\'+$pair[1]+'-next.json') ('NEXT_PROBE'+$crlf) 1 2)
}
$ambiguity=Read-Record 'suite\receipt-without-ack\host-ambiguity.json'
Require ($ambiguity.ReceiptObserved -and $ambiguity.FinalAckTimedOut -and $ambiguity.DispatchDisabled -and $ambiguity.RetryRefused -and -not $ambiguity.FallbackControlSent) 'unknown outcome, no retry/fallback'
$default=Read-Record 'suite\default-handler\default.json'
Require ($default.ExitCode -eq 3221225786 -and $default.KeyboardRestored) 'default Ctrl+C exit'
$raw=Read-Record 'suite\raw-input-guard\raw.json'
$characters=@($raw.Keys | Where-Object { $_.Character -ne 0 })
Require ($raw.ReadReturned -eq 1 -and $raw.ReadSucceeded -eq 1 -and $raw.ReadError -eq 0 -and $raw.ControlCount -eq 0 -and
    $raw.ControlType -eq 4294967295 -and ($raw.InputMode -band 1) -eq 0 -and @($raw.Keys).Count -eq $raw.KeyCount -and $characters.Count -eq 2) 'raw read'
Require ($characters[0].Character -eq 3 -and $characters[0].VirtualKey -eq 67 -and ($characters[0].Modifiers -band 8) -eq 8 -and
    $characters[1].Character -eq 122 -and $characters[1].VirtualKey -eq 90 -and ($characters[1].Modifiers -band 12) -eq 0 -and
    $characters[0].Repeat -eq 1 -and $characters[1].Repeat -eq 1) 'raw characters and modifiers'
foreach($key in @($raw.Keys | Where-Object { $_.Character -eq 0 })) { Require ($key.VirtualKey -eq 17 -and $key.Repeat -eq 1) 'recorded modifier key' }
$death=Read-Record 'suite\shell-owner-exit\owner-exit.json'
Require ($death.FixtureExited -and $death.FixtureExit -eq 73 -and $death.ShellExited -and $death.ShellExit -ne 259 -and -not $death.ExplicitShellTermination) 'shell job cleanup'
foreach($name in @('cmd-empty','cmd-partial','powershell-empty','powershell-partial')) {
    $shell=Read-Record ('suite\'+$name+'\shell.json')
    $event=Read-Record ('suite\'+$name+'\event-observation.json')
    Require ($shell.FreshPrompt -and $shell.CanceledCommandAbsent -and $shell.NextCommand -and $shell.ShellAlive -and $shell.KeyboardRestored -and
        $shell.FixtureControlCount -eq 1 -and $event.ControlCount -eq 1 -and $event.ControlType -eq 0 -and $event.ShellAlive -eq 1 -and
        -not (Test-Path -LiteralPath (Join-Path $Directory ('suite\'+$name+'\canceled-command.txt'))) -and
        [IO.File]::ReadAllText((Join-Path $Directory ('suite\'+$name+'\continued-command.txt'))).Trim() -ceq 'CONTINUE') 'shell prompt and file barriers'
    if($name.StartsWith('powershell')) { Require ($shell.ShellIdentity.EndsWith('|0|ConsoleHost')) 'native shell identity' }
}
[void](Read-Observation 'neighbor\neighbor-read.json' ('BRAVO_KEEP'+$crlf) 0 1)
$neighbor=Read-Record 'neighbor\neighbor.json'
Require ($neighbor.Passed -and $neighbor.HostExited -and $neighbor.PendingObservations -eq 18) 'neighbor isolation'
$files=@(Get-ChildItem -LiteralPath $Directory -File -Recurse)
Require ($files.Count -eq 150) 'complete 150-file native evidence set'
$summary=[ordered]@{
    SchemaVersion=1;Passed=$true;Cases=12;HostLifetimes=13;RegisteredShells=$shells;TitleEvents=$events
    AcceptedRequests=$accepted;RejectedRequests=$rejected;ReceiptOnlyRequests=$received;RestoredSnapshots=$restored
    CanceledReads=5;OrdinaryReads=9;RawReads=1;ShellWorkflows=4;MilestoneAccepted=$false
    Files=@($files | Sort-Object FullName | ForEach-Object {
        [ordered]@{Path=$_.FullName.Substring($Directory.Length+1).Replace('\','/');Bytes=$_.Length;Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}
if($OutputPath) { [IO.File]::WriteAllText([IO.Path]::GetFullPath($OutputPath),($summary | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false)) }
$summary
