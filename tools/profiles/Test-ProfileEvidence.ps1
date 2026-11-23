[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Directory,[string]$PackageRoot='',
    [string]$ExpectedHarnessSource='',[switch]$AllowSynthetic,[string]$OutputPath='')
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
$runtimeSource='0ca6e3fa78481cd4df35820b9fe3988f46e0bff4'
function Need-P([bool]$Okay,[string]$Message){if(-not $Okay){throw ('Profile evidence: '+$Message)}}
function Read-P([string]$Path){[IO.File]::ReadAllText($Path) | ConvertFrom-Json}
function Hash-P([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Files-P([string]$Root,$Files){
    $seen=@{}
    foreach($file in $Files){$path=[IO.Path]::GetFullPath((Join-Path $Root $file.Path))
        Need-P ($path.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $seen.ContainsKey($path)) 'unique contained inventory'
        $seen[$path]=$true;Need-P ((Get-Item -LiteralPath $path).Length -eq $file.Bytes -and (Hash-P $path) -ceq $file.Sha256) ('file hash '+$file.Path)
    }
}
function Snapshot-P([string]$Root,[int]$Stage,$launch){
    $s=Read-P (Join-Path $Root ('snapshot-'+$Stage+'.json'))
    Need-P ($s.Kind -ceq 'ProfileShellSnapshot' -and $s.SchemaVersion -eq 1 -and $s.ProcessId -eq $launch.ProcessId -and
        $s.Nonce -ceq $launch.Nonce -and $s.Stage -eq $Stage -and $s.HostName -ceq 'ConsoleHost' -and
        -not $s.ProfileContentCollected -and -not $s.HistoryContentCollected -and -not $s.ShellOutputCollected) 'profile snapshot identity/privacy'
    Need-P (($s.Profiles.Scope -join ',') -ceq 'AllUsersAllHosts,AllUsersCurrentHost,CurrentUserAllHosts,CurrentUserCurrentHost') 'four profile scopes'
    foreach($f in $s.Profiles){Need-P (-not $f.ReadErrorType -and $f.Bytes -ge 0 -and
        $(if($f.Present){$f.Sha256 -cmatch '^[0-9a-f]{64}$'}else{$f.Bytes -eq 0 -and $f.Sha256 -ceq ''})) 'profile file hash or absence'}
    foreach($name in @('MachinePolicy','UserPolicy','Process','CurrentUser','LocalMachine')){
        Need-P ([string]$s.Policy.$name -in @('Undefined','Restricted','AllSigned','RemoteSigned','Unrestricted','Bypass','Default')) 'complete policy metadata'
    }
    foreach($editor in $s.Editors){Need-P ($editor.Name -ieq 'PSReadLine' -and $editor.Version -cmatch '^\d+\.\d+' -and $editor.Sha256 -cmatch '^[0-9a-f]{64}$') 'editor identity hash'}
    Need-P ($s.Engine -cmatch '^(2\.0|5\.1\.|7\.2\.24)' -and $s.WindowWidth -ge 20 -and $s.WindowHeight -ge 10) 'engine and actual geometry'
    return $s
}
function StartupReference-P([string]$Root,$Launch){
    $path=Join-Path $Root 'startup-reference'
    $reference=Read-P (Join-Path $path 'reference.json')
    Need-P ($reference.Kind -ceq 'NativeProfileStartupReference' -and $reference.SchemaVersion -eq 1 -and
        $reference.NoProfile -and $reference.NativeConsole -and $reference.NaturalExit -eq 37 -and
        $reference.ProcessId -gt 0 -and $reference.Nonce -cmatch '^[0-9a-f]+$' -and
        $reference.Nonce -cne $Launch.Nonce -and $reference.Executable -ieq $Launch.Executable) 'matching native clean-shell reference'
    $snapshot=Snapshot-P $path 0 $reference
    Need-P ([IO.File]::ReadAllText((Join-Path $path 'snapshot-entered.txt')) -ceq ($reference.Nonce+'|'+$reference.ProcessId)) 'native reference reached snapshot'
    return $snapshot
}
function StartupErrors-P($Snapshot,$Reference){
    foreach($value in @($Snapshot,$Reference)){
        Need-P ($value.StartupErrorCount -ge 0 -and $value.ErrorCount -eq $value.StartupErrorCount -and
            $value.StartupErrorDigest -cmatch '^[0-9a-f]{64}$' -and $value.ErrorDigest -ceq $value.StartupErrorDigest -and
            $value.StartupErrorTypes -cmatch '^(?:[A-Za-z0-9_.+]{1,128}(?:\|[A-Za-z0-9_.+]{1,128}){0,3})?$' -and
            ($value.StartupErrorCount -eq 0) -eq ($value.StartupErrorDigest -ceq 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')) 'typed startup and setup error evidence'
    }
    Need-P ($Snapshot.Engine -ceq $Reference.Engine -and $Snapshot.CLRVersion -ceq $Reference.CLRVersion -and
        $Snapshot.CLRVersion -cmatch '^[0-9]+\.[0-9]+' -and
        ($Snapshot.Engine -cne '2.0' -or $Snapshot.CLRVersion.StartsWith('2.0.')) -and
        $Snapshot.Policy.Process -ceq $Reference.Policy.Process -and
        ($Snapshot.StartupErrorCount -eq 0 -or
            ($Snapshot.StartupErrorCount -eq $Reference.StartupErrorCount -and
             $Snapshot.StartupErrorDigest -ceq $Reference.StartupErrorDigest))) 'startup errors match native clean shell'
}
function Prediction-P([string]$Path,$Launch){
    $p=Read-P (Join-Path $Path 'prediction-support.json')
    Need-P ($p.Kind -ceq 'ProfilePredictionCapability' -and $p.SchemaVersion -eq 1 -and
        $p.Nonce -ceq $Launch.Nonce -and $p.ProcessId -eq $Launch.ProcessId -and
        $p.Requested -ceq 'History' -and $p.Before -ceq 'None' -and $p.Restored -ceq 'None' -and
        (($p.Outcome -ceq 'SettingAccepted' -and $p.Observed -ceq 'History' -and -not $p.ErrorType) -or
         ($p.Outcome -ceq 'LegacyConsoleRejected' -and $p.Observed -ceq 'None' -and
          $p.ErrorType -ceq 'System.ArgumentException'))) 'classified prediction setting and restoration'
    return $p.Outcome
}
function ManualStartup-P([string]$Root,$Launch){
    $status=Read-P (Join-Path $Root 'startup-progress.json')
    Need-P ($status.Kind -ceq 'OrdinaryProfileStartup' -and $status.SchemaVersion -eq 1 -and
        $status.ProcessId -eq $Launch.ProcessId -and $status.Nonce -ceq $Launch.Nonce -and $status.Step -ceq 'Ready' -and
        $status.InteractiveCommandAccepted -and $status.HelpersLoaded -and
        $status.SnapshotEntered -and $status.SnapshotPublished -and $status.DisplayActivated) 'complete F8 startup handshake'
    foreach($name in @('f8-entered','helpers-ready','snapshot-entered')){
        Need-P ([IO.File]::ReadAllText((Join-Path $Root ('session\'+$name+'.txt'))) -ceq
            ($Launch.Nonce+'|'+$Launch.ProcessId)) 'same-shell F8 startup acknowledgment'
    }
    Need-P ($status.InheritedWindow.Columns -gt 0 -and $status.InheritedWindow.Rows -gt 0 -and
        $status.RequestedBuffer.Columns -ge [Math]::Max(160,[Math]::Max($status.InheritedWindow.Columns,$status.InheritedBuffer.Columns)) -and
        $status.RequestedBuffer.Rows -ge [Math]::Max(60,[Math]::Max($status.InheritedWindow.Rows,$status.InheritedBuffer.Rows)) -and
        $status.ActiveWindow.Columns -ge 40 -and $status.ActiveWindow.Rows -ge 16 -and
        $status.RequestedBuffer.Columns -ge $status.ActiveWindow.Columns -and
        $status.RequestedBuffer.Rows -ge $status.ActiveWindow.Rows) 'display buffer contains inherited and active windows'
    return $status
}
function EqualState-P($first,$last){
    foreach($name in @('Engine','HostName','LanguageMode','FixtureOrder','FixtureWinner')){Need-P ($first.$name -ceq $last.$name) 'stable profile session identity'}
    foreach($name in @('MachinePolicy','UserPolicy','Process','CurrentUser','LocalMachine')){Need-P ($first.Policy.$name -ceq $last.Policy.$name) 'execution policy preserved'}
    for($i=0;$i -lt 4;$i++){foreach($name in @('Scope','Present','Bytes','Sha256','ReadErrorType')){Need-P ($first.Profiles[$i].$name -ceq $last.Profiles[$i].$name) 'profile files preserved'}}
    Need-P ((@($first.Editors | ForEach-Object {$_.Version}) -join '|') -ceq (@($last.Editors | ForEach-Object {$_.Version}) -join '|') -and (@($first.Editors | ForEach-Object {$_.Sha256}) -join '|') -ceq (@($last.Editors | ForEach-Object {$_.Sha256}) -join '|')) 'editor module preserved'
    foreach($name in @('EditMode','HistorySaveStyle','PredictionSource')){Need-P ($first.Options.$name -ceq $last.Options.$name) 'editor options preserved'}
}
function Proof-P([string]$Root,[string]$Name,$launch){
    $p=Read-P (Join-Path $Root ($Name+'.json'))
    Need-P ($p.Kind -ceq 'ProfileCommandProof' -and $p.SchemaVersion -eq 1 -and $p.ProcessId -eq $launch.ProcessId -and $p.Nonce -ceq $launch.Nonce -and
        $p.Check -ceq $Name -and $p.Matched -and $p.Invocation -gt 0 -and $p.CheckInvocation -gt 0) ('command proof '+$Name)
    return $p
}
function Child-P([string]$Root,$launch){
    $ready=Read-P (Join-Path $Root 'child-ready.json');$done=Read-P (Join-Path $Root 'child-result.json')
    Need-P ($ready.Kind -ceq 'ProfileNativeChildReady' -and $done.Kind -ceq 'ProfileNativeChildResult' -and $ready.ProcessId -gt 0 -and
        $ready.ProcessId -eq $done.ProcessId -and $ready.ParentId -eq $launch.ProcessId -and $done.ParentId -eq $launch.ProcessId -and
        $ready.Nonce -ceq $launch.Nonce -and $done.Nonce -ceq $launch.Nonce -and $done.CtrlC -eq 1 -and $done.NaturalExit -eq 37) 'native child identity/event/exit'
    $identity=Read-P (Join-Path $Root 'identity.json')
    Need-P ($ready.HostQuery.RawHostValue -ceq $identity.AgentQuery.RawHostValue -and $ready.HostQuery.NtStatus -eq 0) 'native child shares retained console'
}
function Launch-P([string]$Root){
    $l=Read-P (Join-Path $Root 'shell-launch.json');$identity=Read-P (Join-Path $Root 'identity.json');$life=Read-P (Join-Path $Root 'lifetime.json');$desk=Read-P (Join-Path $Root 'desktop.json')
    Need-P ($l.Kind -ceq 'ProfileShellLaunch' -and $l.SchemaVersion -eq 1 -and $l.ProcessId -gt 0 -and $l.Nonce -cmatch '^[0-9a-f]+$' -and
        $l.ShellParentId -eq $l.FixtureId -and $l.FixtureId -eq $identity.Fixture.ProcessId -and [uint64]$l.ShellCreationTime -gt 0 -and
        -not $l.PolicyOverride -and $identity.IdentityEligible -and $identity.SameUserLogonIntegritySession -and $identity.NativeX64 -and
        $desk.ApplicationGuiOrdinary -and $desk.HostWindowPrivate -and $life.OwnedProcessesCleaned -and $life.HostExited -and -not $life.Error) 'shell and retained session identity/cleanup'
    $result=Read-P (Join-Path $Root 'profile-result.json')
    Need-P ($result.ShellId -eq $l.ProcessId -and $result.NaturalExit -eq 37 -and $result.WorkflowsPassed) 'natural profile shell completion'
    return $l
}
function Trace-P([string]$Root,[string]$Harness,[int]$ExpectedCases){
    Need-P ((Get-Item -LiteralPath (Join-Path $Root 'collector-errors.txt')).Length -eq 0) 'empty collector errors'
    $ready=@{};$current=@{};$operations=@{}
    $reader=[IO.File]::OpenText((Join-Path $Root 'runtime-trace.jsonl'))
    try{while($null -ne ($line=$reader.ReadLine())){
        $r=$line | ConvertFrom-Json
        Need-P ($r.api -ceq '1.1' -and $r.protocol -eq 2 -and ($r.commit -ceq $runtimeSource -or
            ($r.commit -ceq $Harness -and $r.process -ceq 'RuntimeCancellationTest.exe'))) 'trace runtime/harness/API/protocol'
        if($r.message -match '^host-cancel ready host=(\d+) thread=(\d+) desktop=(VT7Pty-Console-[0-9a-f]{32})$'){
            Need-P (-not $ready.ContainsKey($Matches[3])) 'unique backing desktop'
            $v=@{Host=[int]$Matches[1];Thread=[int]$Matches[2];Desktop=$Matches[3];Agent=[int]$r.pid}
            $ready[$v.Desktop]=$v;$current[[string]$r.pid]=$v
        }elseif($r.message -match '^host-cancel result sequence=(\d+) result=(\d+) host=(\d+) thread=(\d+) captured=(\d+) applied=(\d+) sent=(\d+) restored=(\d+) equal=(\d+) error=(\d+)$'){
            $v=$current[[string]$r.pid]
            Need-P ($null -ne $v -and $Matches[2] -eq '1' -and [int]$Matches[3] -eq $v.Host -and [int]$Matches[4] -eq $v.Thread -and
                ($Matches[5..9] -join '') -ceq '11111' -and $Matches[10] -eq '0') 'one acknowledged restored callback'
            $key=$v.Desktop+':'+$Matches[1];Need-P (-not $operations.ContainsKey($key)) 'unique callback sequence'
            $operations[$key]=@{}
        }elseif($r.message -match '^host-cancel (before|during|after) sequence=(\d+) keys=([0-9a-f]{512})$'){
            $which=$Matches[1];$seq=$Matches[2];$hex=$Matches[3];$v=$current[[string]$r.pid]
            $key=$v.Desktop+':'+$seq
            Need-P ($operations.ContainsKey($key) -and -not $operations[$key].ContainsKey($which)) 'one complete snapshot stage'
            $operations[$key][$which]=$hex
        }
        Need-P (-not $r.message.Contains('outcome=unknown')) 'no ambiguous delivery'
    }}finally{$reader.Dispose()}
    foreach($o in $operations.Values){
        Need-P ($o.Count -eq 3 -and $o.before -ceq $o.after) 'all 256 keyboard bytes restored'
        $b=New-Object byte[] 256
        for($n=0;$n -lt 256;$n++){$b[$n]=[Convert]::ToByte($o.before.Substring(2*$n,2),16)}
        foreach($vk in @(16,160,161,17,162,163,18,164,165,91,92)){$b[$vk]=$b[$vk] -band 1}
        $b[17]=$b[17] -bor 128;$b[162]=$b[162] -bor 128
        Need-P ([BitConverter]::ToString($b).Replace('-','').ToLowerInvariant() -ceq $o.during) 'temporary Ctrl-only state'
    }
    $retained=@{};$seen=@{};$count=0
    foreach($file in Get-ChildItem -LiteralPath $Root -Filter identity.json -File -Recurse){
        $i=Read-P $file.FullName;$d=Read-P (Join-Path $file.DirectoryName 'desktop.json');$l=Read-P (Join-Path $file.DirectoryName 'lifetime.json')
        $pre=Read-P (Join-Path $file.DirectoryName 'preflight.json')
        Need-P ($pre.Eligible -and $pre.SourceCommit -ceq $Harness -and $pre.InputDesktopError -eq 0 -and $pre.IntegrityRid -eq 8192 -and
            $pre.CallerDesktop -ceq $pre.InputDesktop -and $pre.WindowStation -ceq 'WinSta0' -and -not $pre.Impersonated) 'actual eligible case desktop'
        Need-P ($i.IdentityEligible -and $i.SameUserLogonIntegritySession -and $i.NativeX64 -and -not $i.Error -and
            $d.ApplicationGuiOrdinary -and $d.HostWindowPrivate -and $d.ApplicationDesktop -ceq $pre.CallerDesktop -and
            $l.OwnedProcessesCleaned -and $l.HostExited -and -not $l.Error -and $l.Host -eq $i.HostProcessId -and
            $l.Agent -eq $i.Fixture.ParentId -and $l.Fixture -eq $i.Fixture.ProcessId) 'retained ordinary/private session and owned cleanup'
        $key=[string]$i.HostProcessId+':'+$i.HostCreationTime
        Need-P ($ready.ContainsKey($d.BackingDesktop) -and $ready[$d.BackingDesktop].Host -eq $i.HostProcessId -and
            $ready[$d.BackingDesktop].Agent -eq $l.Agent -and [uint64]$i.HostCreationTime -gt 0 -and
            -not $retained.ContainsKey($key) -and -not $seen.ContainsKey($d.BackingDesktop)) 'unique live host identity matches readiness'
        $retained[$key]=$true;$seen[$d.BackingDesktop]=$true;$count++
    }
    Need-P ($count -eq $ExpectedCases -and $ready.Count -eq $count -and $operations.Count -eq 2) 'complete case lifetimes and cooked callback count'
    return @{HostLifetimes=$count;KeyboardOperations=$operations.Count}
}

function Native-P([string]$Root,[string]$Harness){
    $p=Read-P (Join-Path $Root 'probe.json');if(-not $Harness){$Harness=$p.SourceCommit}
    Need-P ($p.Kind -ceq 'ProfileFixtureProbe' -and $p.SchemaVersion -eq 1 -and $p.Status -ceq 'Pass' -and
        $p.SourceCommit -ceq $Harness -and $p.RuntimeSourceCommit -ceq $runtimeSource -and $p.ControlledInlineFixtures -and
        $p.OrdinaryUserProfiles -ceq 'NotRun' -and -not $p.Step12Accepted) 'native profile identity and explicit fixture scope'
    $publicationChecks=0
    if($p.PSObject.Properties['PublicationChecks']){
        $publication=Read-P (Join-Path $Root 'publication\result.json')
        Need-P ($p.PublicationChecks -eq 3 -and $publication.Kind -ceq 'ProfilePublicationOracle' -and $publication.SchemaVersion -eq 1 -and
            $publication.Status -ceq 'Pass' -and $publication.SourceCommit -ceq $Harness -and $publication.OriginalReaderReplaceError -ne 0 -and
            $publication.SharedReaderReplacement -and $publication.PreviousHandleStable -and $publication.NextReadCurrent -and $publication.Checks -eq 3) 'held-reader publication checks'
        Need-P ((Read-P (Join-Path $Root 'publication\proof.json')).Invocation -eq 2 -and (Read-P (Join-Path $Root 'publication\proof.previous')).Invocation -eq 1) 'original and replaced publication proofs'
        $publicationChecks=3
    }
    $configurationChecks = if($p.PSObject.Properties['ConfigurationChecks']){[int]$p.ConfigurationChecks}else{0}
    Need-P ($configurationChecks -in @(0,1)) 'known controlled profile proof format'
    $reported=(Read-P (Join-Path $Root 'cases.json')).Cases
    $definitions=(Read-P (Join-Path $PSScriptRoot 'cases.json')).Cases
    Need-P ($p.Total -in @(2,4) -and $p.Passed -eq $p.Total) 'profile fixture total'
    if($p.Total -eq 2){$definitions=@($definitions | Select-Object -First 2)}
    Need-P (($reported.Case -join '|') -ceq ($definitions.Case -join '|')) 'complete fixture case inventory'
    $cases=@()
    for($i=0;$i -lt $definitions.Count;$i++){
        $definition=$definitions[$i];$row=$reported[$i];$directory=Join-Path $Root $definition.Case
        Need-P ($row.Passed -and $row.OwnedProcessesCleaned -and $row.HostExited -and -not $row.Error) 'fixture reported outcome'
        $launch=Launch-P $directory
        Need-P (-not $launch.Ordinary -and $launch.NoProfile -and -not $launch.SyntheticInputUsed -and $launch.ControlledInlineFixtures) 'controlled launch arguments'
        $first=Snapshot-P $directory 0 $launch;$last=Snapshot-P $directory 1 $launch;EqualState-P $first $last
        if($configurationChecks){
            $reference=StartupReference-P $directory $launch
            StartupErrors-P $first $reference
        }else{
            Need-P ($first.StartupErrorCount -eq 0 -and $first.ErrorCount -eq 0) 'controlled startup and workflow errors'
        }
        if($definition.Engine -ceq 'pwsh'){Need-P ($first.Engine -ceq '7.2.24' -and @($first.Editors).Count -eq 1 -and $first.Options.HistorySaveStyle -ceq 'SaveNothing') 'pinned controlled PSReadLine'}
        else{Need-P ($first.Engine -cmatch '^(2\.0|5\.1\.)' -and @($first.Editors).Count -eq 0) 'native Windows PowerShell editor'}
        $predictionExpected = $definition.PredictionSource
        if(-not $configurationChecks -and $definition.Case -ceq 'pwsh-layered-readline'){$predictionExpected='History'}
        Need-P ($first.Options.EditMode -ceq $definition.EditMode -and $first.Options.PredictionSource -ceq $predictionExpected) 'controlled profile editor options'
        if($configurationChecks){
            Need-P ($first.EditorCount -eq @($first.Editors).Count -and $last.EditorCount -eq $first.EditorCount -and
                $last.EditorVersion -ceq $first.EditorVersion -and
                $first.EditorVersion -ceq $(if($definition.Engine -ceq 'pwsh'){'2.1.0'}else{''}) -and
                $first.StartupErrorDigest -ceq $last.StartupErrorDigest -and $first.StartupErrorCount -eq $last.StartupErrorCount) 'stable and pinned controlled editor/startup identity'
        }
        $prediction = if($configurationChecks -and $definition.Case -ceq 'pwsh-layered-readline'){
            Prediction-P $directory $launch
        }else{'NotRun'}
        Need-P ($first.FixtureOrder -ceq $(if($definition.Layered){'AllUsersAllHosts,AllUsersCurrentHost,CurrentUserAllHosts,CurrentUserCurrentHost'}else{''}) -and
            $first.FixtureWinner -ceq $(if($definition.Layered){'CurrentUserCurrentHost'}else{''})) 'controlled profile precedence'
        $wanted=@('Editing','History','Completion','Multiline','Continue','Resize');if($definition.Case -ceq 'pwsh-layered-readline'){$wanted+=@('Binding')}
        $proofs=(Read-P (Join-Path $directory 'command-proofs.json')).Proofs
        Need-P (($proofs.Check -join ',') -ceq ($wanted -join ',')) 'complete workflow proof inventory'
        foreach($name in $wanted){$check=Proof-P $directory $name $launch
            Need-P ($check.CheckInvocation -eq $(if($name -ceq 'History'){2}else{1})) 'exact controlled invocation count'
            $copy=@($proofs | Where-Object {$_.Check -ceq $name})[0]
            Need-P ($copy.Matched -and $copy.Invocation -eq $check.Invocation -and $copy.CheckInvocation -eq $check.CheckInvocation) 'unaltered backend proof'
        }
        Need-P (-not (Test-Path -LiteralPath (Join-Path $directory 'Cancelled.json'))) 'canceled profile line never executes'
        $cancel=Read-P (Join-Path $directory 'cancel.json');Need-P ($cancel.PartialLineDiscarded -and $cancel.NextCommandPassed) 'profile cancellation and next command'
        Need-P ($last.WindowWidth -eq 100 -and $last.WindowHeight -eq 32) 'actual profile resize readback'
        Child-P $directory $launch
        $cases+=@(@{Case=$definition.Case;Status='Pass';Engine=$first.Engine;Editor=$definition.Editor;Layered=$definition.Layered;Shell=$launch.ProcessId;StartupErrors=$first.StartupErrorCount;PredictionCapability=$prediction})
    }
    $trace=Trace-P $Root $Harness $definitions.Count
    return [ordered]@{Kind='ProfileFixtureReview';Status='Pass';SourceCommit=$Harness;RuntimeSourceCommit=$runtimeSource;Cases=$cases
        HostLifetimes=$trace.HostLifetimes;KeyboardOperations=$trace.KeyboardOperations;PublicationChecks=$publicationChecks;ConfigurationChecks=$configurationChecks;OrdinaryUserProfiles='NotRun';Step12Accepted=$false}
}
function Manual-P([string]$Root,[string]$Harness){
    $p=Read-P (Join-Path $Root 'probe.json');if(-not $Harness){$Harness=$p.SourceCommit}
    Need-P ($p.Kind -ceq 'OrdinaryProfileProbe' -and $p.Status -ceq 'Pass' -and $p.SourceCommit -ceq $Harness -and
        $p.RuntimeSourceCommit -ceq $runtimeSource -and $p.Stages -eq 8 -and $p.OwnedProcessesCleaned -and $p.HostExited -and
        -not $p.Error -and -not $p.ProfileContentCollected -and -not $p.ShellOutputCollected -and -not $p.HistoryContentCollected -and -not $p.Step12Accepted) 'ordinary profile completion/privacy'
    Need-P (($AllowSynthetic -or -not $p.SyntheticInputUsed) -and ($p.OrdinaryProfilesLoaded -eq (-not $p.SyntheticInputUsed))) 'physical ordinary source, never synthetic acceptance'
    $directory=Join-Path $Root 'session';$launch=Launch-P $directory
    Need-P ($launch.Ordinary -and ($launch.NoProfile -eq [bool]$p.SyntheticInputUsed) -and
        $launch.SyntheticInputUsed -eq $p.SyntheticInputUsed -and -not $launch.ControlledInlineFixtures) 'ordinary startup preserves profiles'
    foreach($name in @('runtime-trace.jsonl','stream.bin','ui-transcript.txt','output-trace.json')){
        Need-P (-not (Test-Path -LiteralPath (Join-Path $Root $name)) -and -not (Test-Path -LiteralPath (Join-Path $directory $name))) 'no ordinary output/transcript collection'
    }
    $first=Snapshot-P $directory 0 $launch
    $handshake = if($p.PSObject.Properties['StartupHandshake']){[int]$p.StartupHandshake}else{0}
    Need-P ($handshake -in @(0,1)) 'known ordinary startup proof format'
    $display=$null
    if($handshake){
        $display=ManualStartup-P $Root $launch
        $reference=StartupReference-P $Root $launch
        StartupErrors-P $first $reference
    }else{
        Need-P ($first.StartupErrorCount -eq 0) 'ordinary startup clean before test helpers'
    }
    foreach($stage in 1..7){$snapshot=Snapshot-P $directory $stage $launch;EqualState-P $first $snapshot}
    foreach($name in @('Editing','History','Completion','Multiline','Continue','Resize')){$null=Proof-P $directory $name $launch}
    Need-P ((Proof-P $directory 'History' $launch).CheckInvocation -ge 2) 'ordinary history actually recalled'
    Need-P (-not (Test-Path -LiteralPath (Join-Path $directory 'Cancelled.json'))) 'ordinary canceled line absent'
    Child-P $directory $launch
    $physical=Read-P (Join-Path $Root 'physical-checks.json')
    Need-P (($physical.Stages.Stage -join ',') -ceq '0,1,2,3,4,5,6,7' -and @($physical.KeyCountsByStage).Count -eq 8 -and -not $physical.PrintableTextCollected -and @($physical.Resizes).Count -gt 0) 'complete manual checkpoint and resize inventory'
    foreach($stage in $physical.Stages){Need-P ($stage.Confirmed -and $stage.Source -ceq $(if($p.SyntheticInputUsed){'OwnedConsoleSyntheticSmoke'}else{'PhysicalConsoleInput'})) 'manual checkpoint source'}
    foreach($pair in @(@(1,'8'),@(1,'37'),@(1,'35'),@(2,'38'),@(3,'9'))){Need-P ($physical.KeyCountsByStage[$pair[0]].($pair[1]) -ge 1) 'physical editing/history/completion key'}
    if(-not $p.SyntheticInputUsed){foreach($stage in @(5,6)){Need-P ($physical.KeyCountsByStage[$stage].'67' -ge 1) 'physical Ctrl+C observed'}}
    return [ordered]@{Kind='OrdinaryProfileReview';Status='Pass';SourceCommit=$Harness;RuntimeSourceCommit=$runtimeSource;Engine=$first.Engine
        SyntheticSmoke=[bool]$p.SyntheticInputUsed;OrdinaryProfilesQualified=(-not $p.SyntheticInputUsed);ProfileFilesPresent=@($first.Profiles | Where-Object {$_.Present}).Count
        EditorVersions=@($first.Editors | ForEach-Object {$_.Version});Stages=8;StartupHandshake=$handshake;Display=$display;HostLifetimes=1;ProfileContentCollected=$false;Step12Accepted=$false}
}
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')){
    $p=Read-P (Join-Path $Directory 'probe.json')
    if($p.Kind -ceq 'ProfileFixtureProbe'){$result=Native-P $Directory $ExpectedHarnessSource}else{$result=Manual-P $Directory $ExpectedHarnessSource}
}else{
    Need-P ([bool]$PackageRoot) 'package root required for combined profile review'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $m=Read-P (Join-Path $PackageRoot 'manifest.json');$r=Read-P (Join-Path $Directory 'report.json')
    Need-P ($m.Kind -ceq 'VT7PtyProfileCandidate' -and $m.SchemaVersion -eq 1 -and $r.Kind -ceq 'VT7PtyProfileResults' -and
        $r.ManifestSha256 -ceq (Hash-P (Join-Path $PackageRoot 'manifest.json')) -and $m.Runtime.SourceCommit -ceq $runtimeSource -and
        $m.Runtime.UnchangedFromInputRc2 -and $m.Runtime.ArchiveSha256 -ceq '93d1a1b290d796124b143d490181a861d6224bc61f36ca1cbe46113585a57fb5' -and
        $m.Runtime.ManifestSha256 -ceq 'e66b761b06c4c99fb885dcfe1862537cd9fb3dd577365569668639ccd09f7517' -and
        $r.Runtime.SourceCommit -ceq $runtimeSource -and $r.Runtime.UnchangedFromInputRc2 -and $m.Runtime.ApiVersion -ceq '1.1' -and
        $m.Runtime.ProtocolVersion -eq 2 -and $r.Harness.SourceCommit -ceq $m.Harness.SourceCommit -and $m.Harness.SourceTreeClean -and
        $m.Harness.SourceCommit -cmatch '^[0-9a-f]{40}$' -and -not $r.Step12Accepted -and -not $r.MilestoneAccepted -and
        -not $m.Step12Accepted -and $m.CtrlBreakTransport -ceq 'Unsupported') 'matched profile candidate and unchanged runtime'
    Need-P ($r.Mode -in @('Automated','Manual') -and $r.Tier -in @('DevelopmentHost','NonESU','ESU','Legacy') -and @($r.PreflightErrors).Count -eq 0 -and
        -not $r.PolicyOverrideInherited -and -not $r.ProfileContentCollected -and -not $r.HistoryContentCollected -and -not $r.ShellOutputCollectedInManual) 'profile report prerequisites/privacy'
    Need-P ($r.OrdinaryUserProfiles -ceq $(if($r.Mode -ceq 'Manual'){'PendingReview'}else{'NotRun'})) 'ordinary profiles remain explicitly unqualified before review'
    if($r.Tier -ne 'DevelopmentHost'){Need-P ($r.OS.Version.StartsWith('6.1.') -and $r.OS.ServicePackMajorVersion -eq 1 -and $r.OS.ProductType -eq 1) 'Windows 7 SP1 tier'}
    if($r.Tier -ceq 'Legacy'){
        Need-P ($r.PowerShellVersion -ceq '2.0' -and $r.CLRVersion.StartsWith('2.0.') -and -not $r.PowerShell7.Required -and
            @($r.Updates | Where-Object {$_.Id -ceq 'KB3191566'}).Count -eq 0) 'legacy installed PowerShell 2.0/CLR2 prerequisites'
    }else{Need-P ($r.PowerShell7.Required -and $r.PowerShell7.Version -ceq '7.2.24' -and $r.PowerShell7.Sha256 -cmatch '^[0-9a-f]{64}$' -and
        $r.PowerShell7.ModuleManifestSha256 -cmatch '^[0-9a-f]{64}$') 'pinned profile PowerShell 7 inventory'}
    Files-P $PackageRoot $m.Files
    $rc3 = $m.Profiles.PSObject.Properties['ConfigurationChecks'] -and $m.Profiles.ConfigurationChecks -eq 1
    $bundleCaseFile=Join-Path $PackageRoot 'tools\profiles\cases.json'
    $bundleProbeFile=Join-Path $PackageRoot 'tools\profiles\ProfileProbe.ps1'
    if($rc3){
        $sourceCases=Join-Path $PSScriptRoot 'cases.json'
        $sourceProbe=Join-Path $PSScriptRoot 'ProfileProbe.ps1'
        Need-P ($m.Manual.StartupHandshake -eq 1 -and
            @($m.Files | Where-Object {$_.Path -ceq 'tools/profiles/ProfileJsonWriter.ps1'}).Count -eq 1) 'package includes startup handshake and PS2 JSON writer'
    }else{
        $historic=Join-Path $PSScriptRoot '..\..\docs\validation\results\step-1.2\profiles-rc2-candidate-2026-11-20'
        $sourceCases=Join-Path $historic 'cases.json.txt'
        $sourceProbe=Join-Path $historic 'ProfileProbe.ps1.txt'
    }
    Need-P ((Hash-P (Join-Path $PackageRoot 'reference\manifest.json')) -ceq $m.Runtime.ManifestSha256 -and
        (Hash-P $bundleCaseFile) -ceq (Hash-P $sourceCases) -and (Hash-P $bundleProbeFile) -ceq (Hash-P $sourceProbe)) 'frozen reference and reviewed fixture source'
    $original=Read-P (Join-Path $PackageRoot 'reference\manifest.json')
    foreach($f in $m.Reference.Files){
        $o=@($original.Files | Where-Object {$_.Path -ceq ('runtime/bin/'+$f.Name)})
        Need-P ($o.Count -eq 1 -and $o[0].Sha256 -ceq $f.Sha256 -and $o[0].Bytes -eq $f.Bytes -and
            (Hash-P (Join-Path $PackageRoot ('reference\bin\'+$f.Name))) -ceq $f.Sha256) 'unchanged reference artifact'
        if($f.Name -cmatch '^VT7Pty(?:-Agent|-Host|-DebugServer)?\.(?:exe|dll|pdb)$'){
            Need-P ((Hash-P (Join-Path $PackageRoot ('profiles\bin\'+$f.Name))) -ceq $f.Sha256) 'unchanged profile runtime artifact'
        }
    }
    $files=(Read-P (Join-Path $Directory 'evidence-files.json')).Files;Files-P $Directory $files
    Need-P (@(Get-ChildItem -LiteralPath $Directory -File -Recurse).Count -eq $files.Count+1) 'complete profile evidence inventory'
    $expectedRoles=@(if($r.Mode -ceq 'Manual'){$m.Manual.EnginesByTier.($r.Tier)}else{'Guard','Profiles','Input','Profiles','Profiles'})
    Need-P (($r.Trials.Role -join '|') -ceq ($expectedRoles -join '|') -and @($r.Trials).Count -eq $expectedRoles.Count) 'complete ordered profile trial inventory'
    $reviews=@()
    foreach($t in $r.Trials){
        Need-P ($t.Order -eq $reviews.Count+1 -and $t.Directory -ceq ($t.Order.ToString('00')+'-'+$t.Role.ToLowerInvariant()) -and
            $t.CollectionStatus -ceq 'Completed' -and $t.Outcome -ceq 'Pass' -and $t.ExitCode -eq 0 -and -not $t.TimedOut -and -not $t.Error) 'completed profile controller'
        $native=Join-Path $Directory ($t.Directory+'\native')
        if($r.Mode -ceq 'Manual'){
            if($rc3){
                $issuedProbe=Read-P (Join-Path $native 'probe.json')
                Need-P ($issuedProbe.PSObject.Properties['StartupHandshake'] -and $issuedProbe.StartupHandshake -eq 1) 'required ordinary startup handshake'
            }
            $review=Manual-P $native $m.Harness.SourceCommit
            Need-P ($review.OrdinaryProfilesQualified -and -not $review.SyntheticSmoke) 'real ordinary profile qualification'
            $snap=Read-P (Join-Path $native 'session\snapshot-0.json')
        }elseif($t.Role -ceq 'Guard'){
            $review=& (Join-Path $PSScriptRoot '..\research\Test-H4RuntimeEvidence.ps1') -Directory $native
            Need-P ($review.Cases -eq $m.Reference.GuardCasesByTier.($r.Tier) -and (Read-P (Join-Path $native 'probe.json')).SourceCommit -ceq $runtimeSource) 'frozen H4 guard inventory/source'
        }elseif($t.Role -ceq 'Input'){
            $review=& (Join-Path $PSScriptRoot '..\input\Test-InputEvidence.ps1') -Directory $native -ExpectedRuntimeSource $runtimeSource -ExpectedHarnessSource $runtimeSource -CaseDefinitionsPath (Join-Path $PackageRoot 'reference\input-cases.json')
            Need-P ($review.Outcome -ceq 'Pass' -and $review.Passed -eq 43 -and $review.Total -eq 43) 'accepted frozen input matrix'
        }else{
            if($rc3){
                $issuedProbe=Read-P (Join-Path $native 'probe.json')
                Need-P ($issuedProbe.PSObject.Properties['ConfigurationChecks'] -and $issuedProbe.ConfigurationChecks -eq 1) 'required controlled profile configuration checks'
            }
            $review=Native-P $native $m.Harness.SourceCommit
            Need-P (@($review.Cases).Count -eq $m.Profiles.CasesByTier.($r.Tier)) 'tier-specific fixture inventory'
            if($m.Profiles.PSObject.Properties['PublicationChecks']){Need-P ($m.Profiles.PublicationChecks -eq 3 -and $review.PublicationChecks -eq 3) 'required publication checks in each profile trial'}
            $snap=Read-P (Join-Path $native 'wps-clean-native\snapshot-0.json')
        }
        if($r.Mode -ceq 'Manual' -or $t.Role -ceq 'Profiles'){
            Need-P ($snap.Policy.Process -ceq $(if($r.OriginalProcessPolicy.Present){$r.OriginalProcessPolicy.Value}else{'Undefined'})) 'original process policy restored before shell startup'
        }
        $reviews+=@(@{Order=$t.Order;Role=$t.Role;Review=$review})
    }
    $linkedReview=$null
    if($r.Mode -ceq 'Manual'){
        Need-P ($r.AutomatedRunId -cmatch '^(developmenthost|nonesu|esu|legacy)-profiles-automated-\d{8}-\d{6}-[0-9a-f]{8}$') 'linked automated run identity'
        $linked=Join-Path (Split-Path -Parent $Directory) $r.AutomatedRunId
        $linkedReport=Read-P (Join-Path $linked 'report.json')
        Need-P ($linkedReport.Mode -ceq 'Automated' -and $linkedReport.ComputerName -ceq $r.ComputerName -and $linkedReport.Tier -ceq $r.Tier -and
            $linkedReport.ManifestSha256 -ceq $r.ManifestSha256) 'same-machine linked automated collection'
        $linkedReview=& $MyInvocation.MyCommand.Path -Directory $linked -PackageRoot $PackageRoot
        Need-P ($linkedReview.Status -ceq 'Pass') 'linked automated phase passed independently'
    }
    $result=[ordered]@{Kind='ProfileBundleReview';Status='Pass';Mode=$r.Mode;ComputerName=$r.ComputerName;Tier=$r.Tier;SourceCommit=$m.Harness.SourceCommit
        RuntimeSourceCommit=$runtimeSource;RuntimeUnchanged=$true;Reviews=$reviews;AutomatedRunId=$r.AutomatedRunId
        OrdinaryProfilesQualified=($r.Mode -ceq 'Manual');OrdinaryUserProfiles=$(if($r.Mode -ceq 'Manual'){'QualifiedWithinReturnedConfiguration'}else{'NotRun'})
        CtrlBreakTransport='Unsupported';Step12Accepted=$false}

}
if($OutputPath){
    $OutputPath=[IO.Path]::GetFullPath($OutputPath);Need-P (-not $OutputPath.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside original evidence'
    [IO.File]::WriteAllText($OutputPath,($result | ConvertTo-Json -Depth 25),[Text.UTF8Encoding]::new($false))
}
$result
