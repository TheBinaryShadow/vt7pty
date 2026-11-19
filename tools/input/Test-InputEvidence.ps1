[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$PackageRoot='',
    [string]$ExpectedHarnessSource='',
    [string]$ExpectedRuntimeSource='54b0d23146dfde7528c9e4db1aa92d12e662f883',
    [string]$CaseDefinitionsPath='',
    [string]$OutputPath=''
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$Directory=[IO.Path]::GetFullPath($Directory).TrimEnd('\')
if(-not (Test-Path -LiteralPath (Join-Path $Directory 'probe.json')) -and $PackageRoot){
    $dispatchReport=[IO.File]::ReadAllText((Join-Path $Directory 'report.json')) | ConvertFrom-Json
    if($dispatchReport.SchemaVersion -eq 2){
        & (Join-Path $PSScriptRoot 'Test-InputCandidateEvidence.ps1') -Directory $Directory -PackageRoot $PackageRoot -OutputPath $OutputPath
        return
    }
}
$runtimeSource=$ExpectedRuntimeSource
if(-not $CaseDefinitionsPath){$CaseDefinitionsPath=Join-Path $PSScriptRoot 'cases.json'}
$definitions=([IO.File]::ReadAllText($CaseDefinitionsPath) | ConvertFrom-Json).Cases
function Need-I([bool]$Okay,[string]$Message){if(-not $Okay){throw ('Input evidence: '+$Message)}}
function Read-I([string]$Path){[IO.File]::ReadAllText($Path) | ConvertFrom-Json}
function Hash-I([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Files-I([string]$Root,$Files){
    $seen=@{}
    foreach($f in $Files){
        $p=[IO.Path]::GetFullPath((Join-Path $Root $f.Path))
        Need-I ($p.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase) -and -not $seen.ContainsKey($p)) 'contained unique inventory path'
        $seen[$p]=$true
        Need-I ((Get-Item -LiteralPath $p).Length -eq $f.Bytes -and (Hash-I $p) -ceq $f.Sha256) ('hash '+$f.Path)
    }
}
function Next-I($p){
    return ($p.ReadReturned -eq 1 -and $p.ReadSucceeded -eq 1 -and $p.ReadError -eq 0 -and
        $p.ControlCount -eq 0 -and $p.Text -ceq ('NEXT_PROBE'+[char]13+[char]10) -and $p.Trace.Overflow -eq 0)
}
function Observe-I([string]$Root,$definition){
    $recipe=Read-I (Join-Path $Root 'recipe.json')
    foreach($f in @('Case','Kind','Mode','InputHex','ExpectedText','SecondText','ChunkBytes','GapMilliseconds','LiveReplies')){
        Need-I ($recipe.$f -ceq $definition.$f) ('unaltered golden recipe '+$definition.Case+' '+$f)
    }
    foreach($f in @('IdleMilliseconds','ReplyChunkBytes')){
        if($null -ne $definition.PSObject.Properties[$f]){Need-I ($recipe.$f -eq $definition.$f) ('unaltered golden recipe '+$definition.Case+' '+$f)}
    }
    $writes=Read-I (Join-Path $Root 'writes.json')
    Need-I (($writes.ChunkHex -join '') -ceq $definition.InputHex -and $writes.GapMilliseconds -eq $definition.GapMilliseconds) 'complete recorded input writes'
    $bytes=$definition.InputHex.Length/2
    $chunk=if($definition.ChunkBytes){$definition.ChunkBytes}else{[Math]::Max(1,$bytes)}
    Need-I (@($writes.ChunkHex).Count -eq [Math]::Ceiling($bytes/$chunk)) 'declared fragmentation'
    foreach($h in $writes.ChunkHex){Need-I ($h -cmatch '^(?:[0-9a-f]{2})+$' -and $h.Length -le 2*$chunk) 'bounded hex chunk'}
    if($null -ne $definition.PSObject.Properties['IdleMilliseconds']){
        Need-I ($writes.IdleMilliseconds -eq $definition.IdleMilliseconds) 'declared idle interval'
    }
    $p=Read-I (Join-Path $Root 'input-observed.json')
    $ready=Read-I (Join-Path $Root 'capture-ready.json')
    $stopped=Read-I (Join-Path $Root 'capture-stopped.json')
    $id=Read-I (Join-Path $Root 'identity.json')
    foreach($v in @($p,$ready,$stopped)){
        Need-I ($v.SchemaVersion -eq 1 -and $v.ProcessId -eq $id.Fixture.ProcessId -and $v.Mode -eq $definition.Mode -and
            -not $v.Overflow -and @($v.Records).Count -le 1024 -and $v.Text.Length -le 2047) 'complete bounded fixture capture'
    }
    Need-I (-not $stopped.StopCanceledRead) 'capture ended without fixture read cancellation'
    $mask=1+2+4+16+64+128
    $expectedMode=if($definition.Mode -eq 1){1+2+4+128}elseif($definition.Mode -eq 2){1+128}elseif($definition.Mode -eq 3){16+128}else{128}
    Need-I (($p.InputMode -band $mask) -eq $expectedMode) 'actual input mode'
    $pass=$true;$why=@();$keys=@();$balance=@{};$mouse=@()
    foreach($r in $p.Records){
        Need-I ($r.Type -in @(1,2) -and $r.Down -in @(0,1)) 'known captured event'
        if($r.Type -eq 2){$mouse+=@($r);continue}
        Need-I ($r.VirtualKey -ge 0 -and $r.VirtualKey -lt 256 -and $r.Character -ge 0 -and $r.Character -le 65535 -and
            $r.Repeat -gt 0 -and $r.Repeat -le 1024 -and $r.ScanCode -le 65535) 'bounded complete key record'
        $vk=[string]$r.VirtualKey
        if(-not $balance.ContainsKey($vk)){$balance[$vk]=0}
        $balance[$vk]+=$(if($r.Down){$r.Repeat}else{-$r.Repeat})
        if($balance[$vk] -lt 0){$pass=$false;$why+='key-up before key-down'}
        if($r.Down -and $r.VirtualKey -notin @(16,17,18) -and $r.Character -ne 57344){
            for($n=0;$n -lt $r.Repeat;$n++){$keys+=@($r)}
        }
    }
    if(@($balance.Values | Where-Object {$_ -ne 0}).Count){$pass=$false;$why+='unreleased generated key state'}
    if($definition.Mode -ne 1){
        if(@($stopped.Records).Count -ne @($p.Records).Count){$pass=$false;$why+='late input records after completion fence'}
        if(@($p.Records | Where-Object {$_.Type -eq 1 -and $_.Character -eq 57344 -and $_.Down}).Count -ne 1 -or
            @($p.Records | Where-Object {$_.Type -eq 1 -and $_.Character -eq 57344 -and -not $_.Down}).Count -ne 1){$pass=$false;$why+='completion fence'}
        $replies=Read-I (Join-Path $Root 'terminal-replies.json')
        foreach($hex in $replies.ReplyHex){Need-I ($hex -cmatch '^(?:[0-9a-f]{2})+$') 'actual terminal reply encoding'}
        if($null -ne $definition.PSObject.Properties['ReplyChunkBytes']){
            Need-I (($replies.WriteHex -join '') -ceq ($replies.ReplyHex -join '')) 'complete terminal reply writes'
            if($definition.ReplyChunkBytes){foreach($hex in $replies.WriteHex){Need-I ($hex.Length -eq 2*$definition.ReplyChunkBytes) 'declared reply fragmentation'}}
            if($definition.LiveReplies){Need-I (@($replies.ReplyHex).Count -gt 0) 'live cursor reply exercised'}
        }
        if($p.CtrlBreak -ne 0 -or $p.CtrlC -ne $(if($definition.Kind -ceq 'processed'){1}else{0})){$pass=$false;$why+='unexpected control event'}
        if($definition.Kind -ceq 'navigation'){
            $want=@(37,39,38,40,36,35,46,45,33,34,90)
            if(($keys.VirtualKey -join ',') -cne ($want -join ',')){$pass=$false;$why+='navigation ordering'}
            for($n=0;$n -lt [Math]::Min(10,$keys.Count);$n++){
                if($keys[$n].Character -ne 0 -or $keys[$n].Modifiers -ne 256){$pass=$false;$why+='enhanced navigation'}
            }
            if($keys.Count -eq 11 -and ($keys[10].Character -ne 122 -or $keys[10].Modifiers)){$pass=$false;$why+='ordinary next key after navigation'}
        }elseif($definition.Kind -ceq 'modifiers'){
            $want=@(37,39,36,65,90);$mods=@(264,272,258,2,0);$characters=@(0,0,0,97,122)
            if(($keys.VirtualKey -join ',') -cne ($want -join ',')){$pass=$false;$why+='modifier ordering'}
            for($n=0;$n -lt [Math]::Min($want.Count,$keys.Count);$n++){
                if($keys[$n].Modifiers -ne $mods[$n] -or $keys[$n].Character -ne $characters[$n]){$pass=$false;$why+='chord state'}
            }
        }elseif($definition.Kind -ceq 'alt'){
            if($keys.Count -ne 2 -or $keys[0].Character -ne 97 -or -not ($keys[0].Modifiers -band 2) -or
                $keys[1].Character -ne 122 -or ($keys[1].Modifiers -band 31)){$pass=$false;$why+='Alt text or released next key'}
        }elseif($definition.Kind -ceq 'alt-unicode'){
            $text=New-Object Text.StringBuilder
            foreach($k in $keys){[void]$text.Append([char]$k.Character)}
            if($keys.Count -ne 2 -or $text.ToString() -cne $definition.ExpectedText -or
                -not ($keys[0].Modifiers -band 2) -or $keys[1].Character -ne 122 -or ($keys[1].Modifiers -band 31)){
                $pass=$false;$why+='Alt Unicode or released next key'
            }
        }elseif($definition.Kind -in @('mouse','x10')){
            $negotiation=Read-I (Join-Path $Root 'mouse-ready.json')
            $stream=[Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes((Join-Path $Root 'stream.bin')))
            Need-I ($negotiation.Mode1006Observed -and $stream.IndexOf(([string][char]27+'[?1006h'),[int]$negotiation.StreamStartOffset,[StringComparison]::Ordinal) -ge 0) 'observed mouse negotiation'
            $buttons=if($definition.Kind -ceq 'mouse'){@(1,1,0,7864320,4287102976,1,0)}else{@(1,0)}
            $flags=if($definition.Kind -ceq 'mouse'){@(0,1,0,4,4,0,0)}else{@(0,0)}
            $x=if($definition.Kind -ceq 'mouse'){@(9,10,10,11,11,19,19)}else{@(9,9)}
            $y=if($definition.Kind -ceq 'mouse'){@(4,5,5,6,6,7,7)}else{@(4,4)}
            if($mouse.Count -ne $buttons.Count -or $keys.Count){$pass=$false;$why+='mouse-only count'}
            for($n=0;$n -lt [Math]::Min($mouse.Count,$buttons.Count);$n++){
                if($mouse[$n].Buttons -ne $buttons[$n] -or $mouse[$n].Flags -ne $flags[$n] -or
                    $mouse[$n].X -ne $p.WindowLeft+$x[$n] -or $mouse[$n].Y -ne $p.WindowTop+$y[$n] -or
                    $mouse[$n].Modifiers -ne $(if($definition.Kind -ceq 'mouse' -and $n -eq 5){24}else{0})){$pass=$false;$why+='mouse button/position/wheel/modifier'}
            }
        }elseif($definition.Kind -ceq 'reference'){
            $r=$p.Reference
            if(-not $r.ProcessId -or $r.ProcessId -ne $r.ProcessGroupId -or -not $r.ChildExited -or
                $r.ExitCode -ne 37 -or $r.CtrlBreak -ne 1 -or $r.CtrlC -ne 0 -or $keys.Count -or
                [bool]($r.InputMode -band 1) -ne ($definition.Mode -eq 2)){$pass=$false;$why+='native Break reference group/mode/event/lifetime'}
        }else{
            $text=New-Object Text.StringBuilder
            foreach($k in $keys){[void]$text.Append([char]$k.Character)}
            if($text.ToString() -cne $definition.ExpectedText -or $mouse.Count){$pass=$false;$why+='text order/multiplicity'}
        }
    }else{
        if(-not $p.ReadStarted -or -not $p.ReadReturned -or $p.CtrlBreak){$pass=$false;$why+='cooked completion'}
        if($definition.Kind -ceq 'cancel'){
            if($p.ReadError -ne 995 -or $p.Text -cne '' -or $p.CtrlC -ne 1){$pass=$false;$why+='Unicode cancellation'}
        }elseif(-not $p.ReadSucceeded -or $p.ReadError -ne 0 -or $p.Text -cne $definition.ExpectedText -or $p.CtrlC){$pass=$false;$why+='cooked text'}
        if($definition.Kind -in @('cancel','multiline')){
            $second=Read-I (Join-Path $Root 'second-read.json')
            $want=if($definition.Kind -ceq 'cancel'){'NEXT '+[char]382+[char]13+[char]10}else{$definition.SecondText}
            if(-not $second.ReadReturned -or -not $second.ReadSucceeded -or $second.ReadError -ne 0 -or $second.Text -cne $want -or
                $second.CtrlC -ne $(if($definition.Kind -ceq 'cancel'){1}else{0}) -or $second.CtrlBreak){$pass=$false;$why+='exact second line'}
        }
    }
    if($pass){Need-I (Next-I (Read-I (Join-Path $Root 'next-read.json'))) 'usable ordinary read after capture'}
    return [ordered]@{Case=$definition.Case;Passed=$pass;Reasons=@($why | Select-Object -Unique);RawRecords=@($p.Records).Count;SignificantKeys=$keys.Count;MouseRecords=$mouse.Count}
}
function Trace-I([string]$Root,[string]$Harness){
    Need-I ((Get-Item -LiteralPath (Join-Path $Root 'collector-errors.txt')).Length -eq 0) 'empty collector errors'
    $ready=@{};$current=@{};$operations=@{}
    $reader=[IO.File]::OpenText((Join-Path $Root 'runtime-trace.jsonl'))
    try{while($null -ne ($line=$reader.ReadLine())){
        $r=$line | ConvertFrom-Json
        Need-I ($r.api -ceq '1.1' -and $r.protocol -eq 2 -and ($r.commit -ceq $runtimeSource -or
            ($r.commit -ceq $Harness -and $r.process -ceq 'RuntimeCancellationTest.exe'))) 'trace runtime/harness/API/protocol'
        if($r.message -match '^host-cancel ready host=(\d+) thread=(\d+) desktop=(VT7Pty-Console-[0-9a-f]{32})$'){
            Need-I (-not $ready.ContainsKey($Matches[3])) 'unique backing desktop'
            $v=@{Host=[int]$Matches[1];Thread=[int]$Matches[2];Desktop=$Matches[3];Agent=[int]$r.pid}
            $ready[$v.Desktop]=$v;$current[[string]$r.pid]=$v
        }elseif($r.message -match '^host-cancel result sequence=(\d+) result=(\d+) host=(\d+) thread=(\d+) captured=(\d+) applied=(\d+) sent=(\d+) restored=(\d+) equal=(\d+) error=(\d+)$'){
            $v=$current[[string]$r.pid]
            Need-I ($null -ne $v -and $Matches[2] -eq '1' -and [int]$Matches[3] -eq $v.Host -and [int]$Matches[4] -eq $v.Thread -and
                ($Matches[5..9] -join '') -ceq '11111' -and $Matches[10] -eq '0') 'one acknowledged restored callback'
            $key=$v.Desktop+':'+$Matches[1];Need-I (-not $operations.ContainsKey($key)) 'unique callback sequence'
            $operations[$key]=@{}
        }elseif($r.message -match '^host-cancel (before|during|after) sequence=(\d+) keys=([0-9a-f]{512})$'){
            $which=$Matches[1];$seq=$Matches[2];$hex=$Matches[3];$v=$current[[string]$r.pid]
            $key=$v.Desktop+':'+$seq
            Need-I ($operations.ContainsKey($key) -and -not $operations[$key].ContainsKey($which)) 'one complete snapshot stage'
            $operations[$key][$which]=$hex
        }
        Need-I (-not $r.message.Contains('outcome=unknown')) 'no ambiguous delivery'
    }}finally{$reader.Dispose()}
    foreach($o in $operations.Values){
        Need-I ($o.Count -eq 3 -and $o.before -ceq $o.after) 'all 256 keyboard bytes restored'
        $b=New-Object byte[] 256
        for($n=0;$n -lt 256;$n++){$b[$n]=[Convert]::ToByte($o.before.Substring(2*$n,2),16)}
        foreach($vk in @(16,160,161,17,162,163,18,164,165,91,92)){$b[$vk]=$b[$vk] -band 1}
        $b[17]=$b[17] -bor 128;$b[162]=$b[162] -bor 128
        Need-I ([BitConverter]::ToString($b).Replace('-','').ToLowerInvariant() -ceq $o.during) 'temporary Ctrl-only state'
    }
    $retained=@{};$seen=@{};$count=0
    foreach($file in Get-ChildItem -LiteralPath $Root -Filter identity.json -File -Recurse){
        $i=Read-I $file.FullName;$d=Read-I (Join-Path $file.DirectoryName 'desktop.json');$l=Read-I (Join-Path $file.DirectoryName 'lifetime.json')
        $pre=Read-I (Join-Path $file.DirectoryName 'preflight.json')
        Need-I ($pre.Eligible -and $pre.SourceCommit -ceq $Harness -and $pre.InputDesktopError -eq 0 -and $pre.IntegrityRid -eq 8192 -and
            $pre.CallerDesktop -ceq $pre.InputDesktop -and $pre.WindowStation -ceq 'WinSta0' -and -not $pre.Impersonated) 'actual eligible case desktop'
        Need-I ($i.IdentityEligible -and $i.SameUserLogonIntegritySession -and $i.NativeX64 -and -not $i.Error -and
            $d.ApplicationGuiOrdinary -and $d.HostWindowPrivate -and $d.ApplicationDesktop -ceq $pre.CallerDesktop -and
            $l.OwnedProcessesCleaned -and $l.HostExited -and -not $l.Error -and $l.Host -eq $i.HostProcessId -and
            $l.Agent -eq $i.Fixture.ParentId -and $l.Fixture -eq $i.Fixture.ProcessId) 'retained ordinary/private session and owned cleanup'
        $key=[string]$i.HostProcessId+':'+$i.HostCreationTime
        Need-I ($ready.ContainsKey($d.BackingDesktop) -and $ready[$d.BackingDesktop].Host -eq $i.HostProcessId -and
            $ready[$d.BackingDesktop].Agent -eq $l.Agent -and [uint64]$i.HostCreationTime -gt 0 -and
            -not $retained.ContainsKey($key) -and -not $seen.ContainsKey($d.BackingDesktop)) 'unique live host identity matches readiness'
        $retained[$key]=$true;$seen[$d.BackingDesktop]=$true;$count++
    }
    Need-I ($count -eq $definitions.Count -and $ready.Count -eq $count -and $operations.Count -eq 1) 'complete case lifetimes and cooked callback count'
    return @{HostLifetimes=$count;KeyboardOperations=$operations.Count}
}
function Native-I([string]$Root,[string]$Harness){
    $probe=Read-I (Join-Path $Root 'probe.json')
    if(-not $Harness){$Harness=$probe.SourceCommit}
    Need-I ($probe.Kind -ceq 'InputFidelityProbe' -and $probe.SchemaVersion -eq 1 -and $probe.FullInventory -and
        $probe.SourceCommit -ceq $Harness -and $probe.RuntimeSourceCommit -ceq $runtimeSource -and
        $probe.CtrlBreakTransport -ceq 'Unsupported' -and $probe.NativeBreakReferenceOnly -and -not $probe.Step12Accepted) 'explicit native source and transport limits'
    $reported=(Read-I (Join-Path $Root 'cases.json')).Cases
    Need-I (($reported.Case -join '|') -ceq ($definitions.Case -join '|') -and $probe.Total -eq $definitions.Count) 'complete case inventory'
    $cases=@()
    for($n=0;$n -lt $definitions.Count;$n++){
        $d=$definitions[$n];$c=$reported[$n]
        $measured=Observe-I (Join-Path $Root $d.Case) $d
        Need-I ($c.Passed -eq $measured.Passed -and $c.OwnedProcessesCleaned -and $c.HostExited -and
            [bool]$c.Error -eq (-not $c.Passed)) ('recomputed outcome '+$d.Case)
        $cases+=@($measured)
    }
    $passed=@($cases | Where-Object {$_.Passed}).Count
    Need-I ($probe.Passed -eq $passed -and $probe.Status -ceq $(if($passed -eq $definitions.Count){'Pass'}else{'Fail'})) 'recomputed suite outcome'
    $trace=Trace-I $Root $Harness
    return [ordered]@{Kind='InputNativeReview';CollectionStatus='Completed';Outcome=$probe.Status;SourceCommit=$Harness;RuntimeSourceCommit=$runtimeSource
        Passed=$passed;Total=$definitions.Count;FailedCases=@($cases | Where-Object {-not $_.Passed} | ForEach-Object {$_.Case})
        Cases=$cases;HostLifetimes=$trace.HostLifetimes;KeyboardOperations=$trace.KeyboardOperations;CtrlBreakTransport='Unsupported';Step12Accepted=$false}
}
if(Test-Path -LiteralPath (Join-Path $Directory 'probe.json')){
    $result=Native-I $Directory $ExpectedHarnessSource
}else{
    Need-I ([bool]$PackageRoot) 'package root required for combined evidence'
    $PackageRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
    $manifest=Read-I (Join-Path $PackageRoot 'manifest.json');$report=Read-I (Join-Path $Directory 'report.json')
    Need-I ($manifest.Kind -ceq 'VT7PtyInputCandidate' -and $report.Kind -ceq 'VT7PtyInputResults' -and
        $report.ManifestSha256 -ceq (Hash-I (Join-Path $PackageRoot 'manifest.json')) -and
        $report.Harness.SourceCommit -ceq $manifest.Harness.SourceCommit -and $manifest.Harness.SourceTreeClean -and
        $report.Runtime.SourceCommit -ceq $runtimeSource -and $manifest.Runtime.SourceCommit -ceq $runtimeSource -and
        $manifest.Runtime.UnchangedFromH4 -and $manifest.Runtime.ArchiveSha256 -ceq '910a4920b06872c4756e814d5e5851fe2c3ed9b4c802854b59e2aaa0966d16ce' -and
        $manifest.Runtime.ManifestSha256 -ceq '43a767650d720b1f9dc1bf5d8a17420d0ead7991461df65d088dcab206956a98' -and
        @($report.PreflightErrors).Count -eq 0 -and -not $report.Step12Accepted -and -not $report.MilestoneAccepted -and
        $report.CtrlBreakTransport -ceq 'Unsupported' -and $report.NativeBreakIsReferenceOnly) 'matched unchanged H4 candidate and explicit gaps'
    Need-I ($report.Tier -in @('DevelopmentHost','NonESU','ESU','Legacy') -and $report.ExpectedTrials -eq 6 -and
        $manifest.Input.Cases -eq $definitions.Count -and $manifest.Input.Repetitions -eq 3) 'declared tier and repetition contract'
    if($report.Tier -ne 'DevelopmentHost'){
        Need-I ($report.OS.Version.StartsWith('6.1.') -and $report.OS.ServicePackMajorVersion -eq 1 -and $report.OS.ProductType -eq 1) 'actual Windows 7 SP1 tier'
    }
    if($report.Tier -eq 'Legacy'){
        Need-I ($report.PowerShellVersion -ceq '2.0' -and $report.CLRVersion.StartsWith('2.0.') -and
            @($report.Updates | Where-Object {$_.Id -ceq 'KB3191566'}).Count -eq 0 -and
            -not $report.PowerShell7.Required -and -not $report.PowerShell7.Applicable) 'legacy PowerShell 2.0/CLR2/no-KB3191566 prerequisites'
    }else{
        Need-I ($report.PowerShell7.Required -and $report.PowerShell7.Applicable -and $report.PowerShell7.Version -ceq '7.2.24' -and
            $report.PowerShell7.Sha256 -cmatch '^[0-9a-f]{64}$' -and
            $report.PowerShell7.ModuleManifestSha256 -cmatch '^[0-9a-f]{64}$') 'required pinned PowerShell 7 inventory'
    }
    Files-I $PackageRoot $manifest.Files
    $files=(Read-I (Join-Path $Directory 'evidence-files.json')).Files
    Files-I $Directory $files
    Need-I (@(Get-ChildItem -LiteralPath $Directory -File -Recurse).Count -eq $files.Count+1) 'complete returned inventory'
    Need-I (($report.Trials.Order -join ',') -ceq '1,2,3,4,5,6' -and ($report.Trials.Role -join ',') -ceq 'Guard,Input,Guard,Input,Guard,Input') 'three interleaved complete repetitions'
    $reviews=@();$inputFailures=0
    foreach($t in $report.Trials){
        Need-I ($t.CollectionStatus -ceq 'Completed' -and -not $t.TimedOut -and -not $t.Error -and
            $t.Directory -ceq ($t.Order.ToString('00')+'-'+$t.Role.ToLowerInvariant())) 'completed native controller'
        $native=Join-Path $Directory ($t.Directory+'\native')
        if($t.Role -ceq 'Input'){
            $review=Native-I $native $manifest.Harness.SourceCommit
            Need-I ($t.Outcome -ceq $review.Outcome -and $t.Passed -eq $review.Passed -and $t.Total -eq $definitions.Count -and
                (($t.ExitCode -eq 0) -eq ($review.Outcome -ceq 'Pass'))) 'matching input collection outcome'
            $inputFailures+=33-$review.Passed
        }else{
            $review=& (Join-Path $PSScriptRoot '..\research\Test-H4RuntimeEvidence.ps1') -Directory $native
            Need-I ($t.Outcome -ceq 'Pass' -and $t.ExitCode -eq 0 -and $t.Total -eq $manifest.Guard.CasesByTier.($report.Tier) -and
                $t.Passed -eq $t.Total -and (Read-I (Join-Path $native 'probe.json')).SourceCommit -ceq $runtimeSource) 'unchanged accepted H4 guard outcome'
        }
        $reviews+=@(@{Order=$t.Order;Role=$t.Role;Review=$review})
    }
    $result=[ordered]@{Kind='InputBundleReview';CollectionStatus='Completed';ComputerName=$report.ComputerName;Tier=$report.Tier
        Outcome=$(if($inputFailures){'Fail'}else{'Pass'});InputCases=99;InputFailures=$inputFailures;Reviews=$reviews;CtrlBreakTransport='Unsupported';Step12Accepted=$false}
}
if($OutputPath){
    $OutputPath=[IO.Path]::GetFullPath($OutputPath)
    Need-I (-not $OutputPath.StartsWith($Directory+'\',[StringComparison]::OrdinalIgnoreCase)) 'review output outside original evidence'
    [IO.File]::WriteAllText($OutputPath,($result | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
}
$result
