[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$PackageRoot='',
    [string]$OutputPath=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot 'H4Cases.ps1')
function Assert-H4([bool]$Condition,[string]$Message) { if(-not $Condition) { throw ('H4 evidence: '+$Message) } }
function Read-H4([string]$Path) { [IO.File]::ReadAllText($Path) | ConvertFrom-Json }
function Resolve-H4([string]$Root,[string]$Relative) {
    $path=[IO.Path]::GetFullPath((Join-Path $Root $Relative))
    Assert-H4 ($path.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase)) 'path leaves evidence root'
    return $path
}
function Verify-H4Files([string]$Root,$Files) {
    $seen=@{}
    foreach($file in $Files) {
        Assert-H4 (-not $seen.ContainsKey($file.Path)) 'duplicate inventory path'
        $seen[$file.Path]=$true
        $path=Resolve-H4 $Root $file.Path
        Assert-H4 ((Get-Item -LiteralPath $path).Length -eq $file.Bytes -and
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $file.Sha256) ('hash '+$file.Path)
    }
}
function Review-H4Native([string]$Root,[bool]$PowerShell7) {
    $probe=Read-H4 (Join-Path $Root 'probe.json')
    $cases=(Read-H4 (Join-Path $Root 'cases.json')).Cases
    $expected=@(Get-H4CaseNames $PowerShell7)
    Assert-H4 ($probe.Kind -eq 'H4RuntimeProbe' -and $probe.SchemaVersion -eq 1 -and $probe.RuntimeIntegrated -and -not $probe.H4Accepted) 'native identity'
    Assert-H4 ((@($cases | ForEach-Object {$_.Case}) -join '|') -ceq ($expected -join '|') -and $probe.Total -eq $expected.Count) 'exact case inventory'
    Assert-H4 ($probe.Status -eq 'Pass' -and $probe.Passed -eq $probe.Total) 'native outcome'
    foreach($case in $cases) {
        Assert-H4 ($case.Passed -and $case.OwnedProcessesCleaned -and $case.HostExited -and -not $case.Error) ('case outcome '+$case.Case)
    }
    $identities=@{};$sessions=0
    foreach($file in @(Get-ChildItem -LiteralPath $Root -Filter identity.json -File -Recurse)) {
        $identity=Read-H4 $file.FullName
        Assert-H4 ($identity.IdentityEligible -and $identity.SameUserLogonIntegritySession -and $identity.NativeX64 -and -not $identity.Error) 'retained identity'
        $agent=[string]$identity.Fixture.ParentId
        $desktop=Read-H4 (Join-Path $file.DirectoryName 'desktop.json')
        $life=Read-H4 (Join-Path $file.DirectoryName 'lifetime.json')
        Assert-H4 ($desktop.ApplicationDesktop -ceq $probe.ApplicationDesktop -and
            $desktop.BackingDesktop -cmatch '^VT7Pty-Console-[0-9a-f]{32}$' -and
            $desktop.ApplicationGuiOrdinary -and $desktop.HostWindowPrivate -and
            $life.Agent -eq [int]$agent -and $life.Fixture -eq $identity.Fixture.ProcessId -and
            $life.Host -eq $identity.HostProcessId -and $life.OwnedProcessesCleaned -and $life.HostExited -and -not $life.Error) 'desktop and lifetime'
        Assert-H4 (-not $identities.ContainsKey($agent)) 'unique live session agent identities'
        $identities[$agent]=$identity
        $sessions++
    }
    # Recompute read results, not just the harness's Passed flags.
    $cancellations=0;$nextReads=0
    foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse | Where-Object {
            $_.Name -match '^cancel-\d+\.json$|^burst-read\.json$|^transition\.json$|^child-cancel\.json$'})) {
        $p=Read-H4 $file.FullName
        Assert-H4 ($p.ReadReturned -eq 1 -and $p.ReadError -eq 995 -and $p.Text -ceq '' -and
            $p.ControlCount -ge 1 -and $p.ControlType -eq 0 -and $p.Trace.ReadEntered -eq 1 -and $p.Trace.Overflow -eq 0) 'canceled-read evidence'
        $events=@($p.Trace.Events | Where-Object {$_.Kind -eq 5})
        Assert-H4 ($events.Count -eq $p.ControlCount) 'one handler exit per event'
        $cancellations++
    }
    foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse | Where-Object {$_.Name -in @('next-read.json','child-next.json')})) {
        $p=Read-H4 $file.FullName
        $text=if($file.Name -eq 'child-next.json') {"CHILD_NEXT"+[char]13+[char]10} else {"NEXT_PROBE"+[char]13+[char]10}
        Assert-H4 ($p.ReadReturned -eq 1 -and $p.ReadSucceeded -eq 1 -and $p.ReadError -eq 0 -and $p.Text -ceq $text -and $p.Trace.Overflow -eq 0) 'exact subsequent read'
        $nextReads++
    }
    foreach($name in @('raw-input','mode-transitions')) {
        $p=Read-H4 (Join-Path $Root ($name+'\raw-read.json'))
        Assert-H4 ($p.ReadReturned -eq 1 -and $p.ReadSucceeded -eq 1 -and $p.ReadError -eq 0 -and $p.ControlCount -eq 0 -and
            $p.KeyCount -eq 3 -and (@($p.Keys | ForEach-Object {$_.Character}) -join ',') -ceq '0,3,122' -and
            (@($p.Keys | ForEach-Object {$_.VirtualKey}) -join ',') -ceq '17,67,90' -and
            ($p.Keys[0].Modifiers -band 8) -and ($p.Keys[1].Modifiers -band 8) -and -not ($p.Keys[2].Modifiers -band 12) -and
            (@($p.GuardInput | ForEach-Object {$_.Down}) -join ',') -ceq '1,1,0,0,1' -and
            (@($p.GuardInput | ForEach-Object {$_.VirtualKey}) -join ',') -ceq '17,67,67,17,90') 'raw modifier/C/z records'
    }
    $close=Read-H4 (Join-Path $Root 'close-rpc\close.json')
    Assert-H4 ($close.CloseSucceeded -and $close.Idempotent -and $close.PostCloseRpcRejected -and $close.AllocationRetained -and
        $close.ConcurrentRpcSuccesses -ge 1 -and $close.UnexpectedRpcErrors -eq 0) 'two-phase close contract'
    $resources=Read-H4 (Join-Path $Root 'recreate-resources\resources.json')
    Assert-H4 ($resources.Cycles -eq 6 -and $resources.HandlesAfter -le $resources.HandlesBefore+2) 'caller handle budget'
    $denial=Read-H4 (Join-Path $Root 'impersonation-denied\denial.json')
    Assert-H4 ($denial.Denied -and $denial.ErrorCode -ne 0 -and $denial.Message.Contains('unimpersonated')) 'real impersonation denial'
    $death=Read-H4 (Join-Path $Root 'agent-death\agent-death.json')
    Assert-H4 ($death.OwnedAgentTerminated -and $death.ChildExitedWithoutHarnessTermination -and $death.HostExitedWithoutTermination) 'agent death cleanup'

    $raceOutcomes=@()
    foreach($name in @('race-enter-biased','race-concurrent','race-cancel-biased')) {
        $race=Read-H4 (Join-Path $Root ($name+'\race.json'))
        $p=Read-H4 (Join-Path $Root ($name+'\race-read.json'))
        Assert-H4 ($race.ETXWrites -eq 1 -and $race.CompletionEnters -eq 1 -and -not $race.RetrySent -and $race.InputFencePassed -and $race.NextReadPreserved -and
            [decimal]$race.EnterEndQpc -ge [decimal]$race.EnterBeginQpc -and [decimal]$race.CancelEndQpc -ge [decimal]$race.CancelBeginQpc) 'single concurrent input pair'
        Assert-H4 ($p.ReadReturned -eq 1 -and $p.ControlCount -eq 1 -and $p.ControlType -eq 0 -and
            (($race.Outcome -eq 'Canceled' -and $p.ReadError -eq 995 -and $p.Text -ceq '') -or
             ($race.Outcome -eq 'Completed' -and $p.ReadError -eq 0 -and $p.ReadSucceeded -eq 1 -and $p.Text -ceq ("racekeep"+[char]13+[char]10)))) 'race read outcome'
        $raceOutcomes+=@($race.Outcome)
    }
    Assert-H4 ($raceOutcomes -contains 'Completed' -and $raceOutcomes -contains 'Canceled') 'both race outcomes'
    foreach($file in @(Get-ChildItem -LiteralPath $Root -Filter shell.json -File -Recurse)) {
        $shell=Read-H4 $file.FullName
        Assert-H4 ($shell.FreshPrompt -and $shell.CanceledCommandAbsent -and $shell.NextCommand -and $shell.Alive -and $shell.ETXWrites -eq 1 -and -not $shell.RetrySent) 'shell recovery'
        Assert-H4 (-not (Test-Path -LiteralPath (Join-Path $file.DirectoryName 'canceled-command.txt'))) 'forbidden command absent'
    }
    foreach($file in @(Get-ChildItem -LiteralPath $Root -Filter child.json -File -Recurse)) {
        $child=Read-H4 $file.FullName
        Assert-H4 ($child.ChildExitCode -eq 37 -and $child.ParentContinued -and $child.ParentAlive -and $child.ETXWrites -eq 1 -and -not $child.RetrySent) 'active child and parent'
        Assert-H4 (([IO.File]::ReadAllText((Join-Path $file.DirectoryName 'child-exit.txt'))).Trim() -ceq '37') 'parent observed exact child status'
    }
    foreach($name in @('caller-death','caller-and-agent-death','enclosing-job')) {
        $caller=Read-H4 (Join-Path $Root ($name+'\caller.json'))
        Assert-H4 ($caller.AllOwnedProcessesExited -and $caller.HostExitedWithoutTermination) 'abrupt lifetime'
    }
    foreach($name in @('helper-missing','helper-wrong')) {
        $startup=Read-H4 (Join-Path $Root ($name+'\startup.json'))
        Assert-H4 ($startup.OptInDenied -and $startup.ErrorCode -ne 0 -and $startup.FlaglessOpenPassed -and $startup.OwnedAgentExited -and $startup.ObservedHostExited) 'module startup control'
    }
    Assert-H4 (([IO.File]::ReadAllText((Join-Path $Root 'collector-errors.txt'))).Length -eq 0) 'collector errors or truncation'
    $trace=@([IO.File]::ReadLines((Join-Path $Root 'runtime-trace.jsonl')) | ForEach-Object {$_ | ConvertFrom-Json})
    Assert-H4 ($trace.Count -gt 0) 'trace present'
    $keys=@{};$ready=@{}
    foreach($record in $trace) {
        Assert-H4 ($record.version -ceq $probe.PackageVersion -and $record.commit -ceq $probe.SourceCommit -and $record.api -ceq '1.1' -and $record.protocol -eq 2) 'trace build identity'
        if($record.message -match '^host-cancel ready host=(\d+) thread=(\d+) desktop=(VT7Pty-Console-[0-9a-f]{32})$') {
            $ready[[string]$record.pid]=@{Host=[int]$Matches[1];Thread=[int]$Matches[2];Desktop=$Matches[3]}
        }
        if($record.message -match '^host-cancel result sequence=(\d+) result=(\d+) host=(\d+) thread=(\d+) captured=(\d+) applied=(\d+) sent=(\d+) restored=(\d+) equal=(\d+) error=(\d+)$') {
            $key=[string]$record.pid+':'+$Matches[1]
            Assert-H4 (-not $keys.ContainsKey($key) -and $Matches[2] -eq '1' -and
                ($Matches[5..9] -join '') -ceq '11111' -and $Matches[10] -eq '0') 'accepted restored keyboard result'
            $keys[$key]=@{Agent=[string]$record.pid;Host=[int]$Matches[3];Thread=[int]$Matches[4]}
        } elseif($record.message -match '^host-cancel (before|during|after) sequence=(\d+) keys=([0-9a-f]{512})$') {
            $which=$Matches[1];$key=[string]$record.pid+':'+$Matches[2];$hex=$Matches[3]
            Assert-H4 ($keys.ContainsKey($key) -and -not $keys[$key].ContainsKey($which)) 'one snapshot per result stage'
            $keys[$key][$which]=$hex
        }
    }
    foreach($key in $keys.Keys) {
        $k=$keys[$key]
        Assert-H4 ($k.ContainsKey('before') -and $k.ContainsKey('during') -and $k.ContainsKey('after') -and $k.before -ceq $k.after -and $ready.ContainsKey($k.Agent)) 'all 256 keyboard bytes restored'
        $r=$ready[$k.Agent]
        Assert-H4 ($identities.ContainsKey($k.Agent) -and $identities[$k.Agent].HostProcessId -eq $r.Host) 'retained host matches trace'
        Assert-H4 ($k.Host -eq $r.Host -and $k.Thread -eq $r.Thread) 'verified host callback context'
        $expectedBytes=New-Object byte[] 256
        for($i=0;$i -lt 256;$i++) {$expectedBytes[$i]=[Convert]::ToByte($k.before.Substring($i*2,2),16)}
        foreach($vk in @(16,160,161,17,162,163,18,164,165,91,92)) {$expectedBytes[$vk]=$expectedBytes[$vk] -band 1}
        $expectedBytes[17]=$expectedBytes[17] -bor 128;$expectedBytes[162]=$expectedBytes[162] -bor 128
        Assert-H4 (([BitConverter]::ToString($expectedBytes).Replace('-','').ToLowerInvariant()) -ceq $k.during) 'exact temporary Ctrl table'
    }
    $minimum=if($PowerShell7){36}else{30}
    Assert-H4 ($keys.Count -ge $minimum) 'all required cancellation trace records'
    return [ordered]@{Kind='H4NativeReview';Status='Pass';Cases=$cases.Count;Sessions=$sessions;CanceledReadObservations=$cancellations;ExactNextReads=$nextReads;KeyboardOperations=$keys.Count;H4Accepted=$false}
}
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')) {
    $root=Read-H4 (Join-Path $Directory 'probe.json')
    $result=Review-H4Native $Directory ($root.Total -eq 41)
} else {
    Assert-H4 ([bool]$PackageRoot) 'PackageRoot required for a combined run'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $manifest=Read-H4 (Join-Path $PackageRoot 'manifest.json')
    $report=Read-H4 (Join-Path $Directory 'report.json')
    Assert-H4 ($manifest.Kind -eq 'VT7PtyH4RuntimeCandidate' -and $report.Kind -eq 'VT7PtyH4RuntimeResults' -and
        $report.ManifestSha256 -ceq (Get-FileHash -LiteralPath (Join-Path $PackageRoot 'manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant() -and
        -not $report.H4Accepted -and $report.Trials.Count -eq 9) 'bundle identity'
    Verify-H4Files $PackageRoot $manifest.Files
    $inventory=Read-H4 (Join-Path $Directory 'evidence-files.json')
    Verify-H4Files $Directory $inventory.Files
    Assert-H4 (@(Get-ChildItem -LiteralPath $Directory -File -Recurse).Count -eq $inventory.Files.Count+1) 'complete collection inventory'
    if($report.Tier -ne 'DevelopmentHost') {
        Assert-H4 ($report.Regression.Status -eq 'Pass' -and $report.Regression.ExitCode -eq 0 -and $report.Regression.SoakMinutes -eq 120) 'target regression completed'
        $regressionFiles=@(Get-ChildItem -LiteralPath (Join-Path $Directory 'regression') -File -Filter '*.json')
        Assert-H4 ($regressionFiles.Count -eq 1) 'one target acceptance report'
        $regression=Read-H4 $regressionFiles[0].FullName
        Assert-H4 ($regression.Status -eq 'Pass' -and $regression.DeclaredTier -ceq $report.Tier -and
            $regression.Package.SourceCommit -ceq $manifest.Runtime.SourceCommit -and
            $regression.Package.ManifestSha256 -ceq $manifest.Regression.ManifestSha256 -and
            $regression.Tests.Count -eq 30 -and @($regression.Tests | Where-Object {$_.Status -ne 'Pass'}).Count -eq 0 -and
            @($regression.Tests | Where-Object {$_.Name -ceq 'Session contract: SOAK 120 minutes' -and $_.Status -eq 'Pass'}).Count -eq 1) 'all target regression and soak cases'
    } else {Assert-H4 ($report.Regression.Status -eq 'NotApplicable') 'development target-acceptance boundary'}

    $trials=@();$hashes=0
    foreach($trial in $report.Trials) {
        $role=@('Control','Reference','Runtime')[($trial.Order-1)%3]
        Assert-H4 ($trial.Role -ceq $role -and $trial.Group -eq [Math]::Ceiling($trial.Order/3.0) -and $trial.CollectionStatus -eq 'Completed' -and -not $trial.TimedOut) 'interleaved completed trials'
        $trialDir=Resolve-H4 $Directory $trial.Directory
        $inventory=Read-H4 (Join-Path $trialDir 'evidence-files.json')
        Verify-H4Files $trialDir $inventory.Files
        Assert-H4 (@(Get-ChildItem -LiteralPath $trialDir -File -Recurse).Count -eq $inventory.Files.Count+1) 'complete returned file inventory'
        $hashes+=$inventory.Files.Count
        $native=Join-Path $trialDir 'native'
        $producer=Read-H4 (Join-Path $native 'probe.json')
        Assert-H4 ($trial.NativeOutcome -ceq $producer.Status) 'producer outcome retained'
        if($role -eq 'Runtime') {
            $review=Review-H4Native $native ($report.Tier -ne 'Legacy')
            $root=Read-H4 (Join-Path $native 'probe.json')
            Assert-H4 ($root.SourceCommit -ceq $manifest.Runtime.SourceCommit -and $trial.ExitCode -eq 0 -and $trial.Outcome -eq 'Pass') 'runtime matches candidate'
        } elseif($role -eq 'Reference') {
            $review=& (Join-Path $PSScriptRoot 'Test-HostGuardEvidence.ps1') -Directory $native
            Assert-H4 ($trial.ExitCode -eq 0 -and $trial.Outcome -eq 'Pass') 'unchanged H3 reference passed'
        } else {
            $root=Read-H4 (Join-Path $native 'probe.json')
            Assert-H4 ($root.Version -eq 1 -and (($root.Status -eq 'Pass') -eq ($trial.ExitCode -eq 0)) -and
                $trial.Outcome -ceq $root.Status) 'original rc1 outcome retained'
            $review=@{Kind='OriginalControl';Outcome=$root.Status}
        }
        $trials+=@([ordered]@{Order=$trial.Order;Role=$role;Review=$review})
    }
    Assert-H4 ((@($report.Trials.Order) -join ',') -ceq '1,2,3,4,5,6,7,8,9') 'nine distinct ordered trials'
    $result=[ordered]@{Kind='H4BundleReview';Trials=$trials;VerifiedEvidenceFiles=$hashes;Regression=$report.Regression;H4Accepted=$false}
}
if($OutputPath) {
    $path=[IO.Path]::GetFullPath($OutputPath)
    Assert-H4 (-not $path.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside original results'
    [IO.File]::WriteAllText($path,($result | ConvertTo-Json -Depth 24),[Text.UTF8Encoding]::new($false))
}
$result
