# The lifecycle collector and reviewer also run on Windows PowerShell 2.0.
function Get-LifecycleRegressionNames {
    'REPEAT'
    'RESIZE'
    'SHUTDOWN'
    'SPAWN_FAILURE'
    'CONCURRENT'
}

function Get-LifecycleCaseSchedule($Manifest) {
    $acceptedFirst = $false
    foreach ($spec in $Manifest.Cases) {
        $order = @('baseline','accepted')
        if ($spec.Area -ceq 'F09') {
            if ($acceptedFirst) { $order = @('accepted','baseline') }
            $acceptedFirst = -not $acceptedFirst
        }
        foreach ($role in $order) { @{Runtime=$role; Spec=$spec} }
    }
}

function Read-LifecyclePackage([string]$Root) {
    $manifest = Read-VT7Json (Join-Path $Root 'manifest.json')
    $extended = $manifest.SchemaVersion -eq 2
    $idle = if ($extended) { 15000 } else { 1000 }
    if ($manifest.Kind -cne 'VT7PtyLifecycleCharacterization' -or @(1,2) -notcontains $manifest.SchemaVersion -or
        $manifest.Step15Accepted -or $manifest.OutputRows -ne 200 -or $manifest.Samples -ne 16 -or
        $manifest.IdleMilliseconds -ne $idle -or $manifest.ShutdownMilliseconds -ne 10000 -or
        ($extended -and ($manifest.ActiveMilliseconds -ne 20000 -or $manifest.ActiveStepMilliseconds -ne 25)) -or
        ($manifest.Regressions -join ',') -cne ((Get-LifecycleRegressionNames) -join ',')) {
        throw 'Wrong lifecycle characterization recipe.'
    }
    $cases = @{
        LIFECYCLE_CMD_DRAIN = 'F08'; LIFECYCLE_PS_DRAIN = 'F08'; LIFECYCLE_NEGATIVE_DRAIN = 'F08'
        LIFECYCLE_RECOVER = 'F08'; LIFECYCLE_CLOSE = 'F08'
        LIFECYCLE_MEASURE_1 = 'F09'; LIFECYCLE_MEASURE_2 = 'F09'; LIFECYCLE_MEASURE_3 = 'F09'
    }
    if ($extended) { $cases['LIFECYCLE_MEASURE_4'] = 'F09' }
    foreach ($spec in $manifest.Cases) {
        if (-not $cases.ContainsKey($spec.Name) -or $cases[$spec.Name] -cne $spec.Area) {
            throw ('Wrong or repeated lifecycle case: ' + $spec.Name)
        }
        $cases.Remove($spec.Name)
    }
    if ($cases.Count) { throw 'Incomplete lifecycle case inventory.' }
    Confirm-FidelityPackage $Root $manifest
    $files = @{}
    foreach ($file in $manifest.Files) { $files[$file.Path] = $file.Sha256 }
    foreach ($name in @('SessionContractTest.exe','SessionFixture.exe')) {
        if (-not $files['baseline/bin/' + $name] -or
            $files['baseline/bin/' + $name] -cne $files['accepted/bin/' + $name]) {
            throw ('Lifecycle regression builds differ: ' + $name)
        }
    }
    return $manifest
}

function Test-LifecycleRegression($Entry) {
    $controller = $Entry.Controller
    return $null -ne $controller -and -not $controller.TimedOut -and $controller.ExitCode -eq 0 -and
        $controller.StandardOutput -ceq ('Session contract ' + $Entry.Name + ' passed.')
}

function Get-LifecycleOutcome($Record) {
    if ($Record.Status -ceq 'Pass') { return 'Match' }
    if ($Record.Case -ceq 'LIFECYCLE_NEGATIVE_DRAIN' -and $Record.OwnedProcessesCleaned -and
        $Record.Error -ceq 'final marker missing from drained output') { return 'ControlDetected' }
    return 'Difference'
}

function Confirm-LifecycleSnapshot([string]$Path, [int]$Ordinal, $Manifest) {
    $snapshot = [IO.File]::ReadAllBytes($Path)
    $index = ($Ordinal - 1) % $Manifest.Samples
    $columns = if ($index % 2 -eq 0) { 100 } else { 80 }
    $rows = if ($index % 2 -eq 0) { 30 } else { 25 }
    $key = 148 + 16 * $index
    if ($snapshot.Length -ne $Manifest.Harness.SnapshotBytes -or
        [BitConverter]::ToUInt32($snapshot,0) -ne 0x31465456 -or [BitConverter]::ToUInt32($snapshot,4) -ne 1 -or
        [BitConverter]::ToUInt32($snapshot,8) -ne $columns -or [BitConverter]::ToUInt32($snapshot,12) -ne $rows -or
        [BitConverter]::ToUInt32($snapshot,16) -ne $columns -or [BitConverter]::ToUInt32($snapshot,64) -ne ($index + 1) -or
        [BitConverter]::ToUInt32($snapshot,$key) -ne (97 + $index) -or [BitConverter]::ToUInt32($snapshot,$key + 12) -ne 1) {
        throw ('Invalid resize or key evidence at measurement ' + $Ordinal)
    }
    $offset = $snapshot.Length - 19200 + 4 * (10 * $columns + 2)
    $marker = 'VT7_BARRIER_' + $Ordinal + '_END'
    for ($j = 0; $j -lt $marker.Length; $j++) {
        if ([BitConverter]::ToUInt16($snapshot,$offset + 4 * $j) -ne [int]($marker[$j])) {
            throw ('Wrong native barrier marker at measurement ' + $Ordinal)
        }
    }
}

function Confirm-LifecycleMeasurements([string]$Directory, $Record, $Manifest, [int]$StreamBytes) {
    $extended = $Manifest.SchemaVersion -eq 2
    $phases = @('InputUs','ResizeUs','BarrierUs')
    $timingUs = $Record.ActiveUs
    $windows = @('IdleBefore','IdleAfter','ActiveAfter')
    if ($extended) {
        $phases = @('InputUs','ResizeRpcUs','ReaderResizeUs','BarrierUs')
        $timingUs = $Record.TimingUs
        $windows = @('IdleBefore','IdleAfter','ActiveBefore','ActiveAfter')
        $stepLimit = 1 + $Manifest.ActiveMilliseconds / $Manifest.ActiveStepMilliseconds
        if (($Record.ActiveSteps -isnot [int] -and $Record.ActiveSteps -isnot [long]) -or
            $Record.ActiveUs -lt 1000 * $Manifest.ActiveMilliseconds -or
            $Record.ActiveSteps -lt $Manifest.Samples -or $Record.ActiveSteps -gt $stepLimit -or
            $Record.ActiveUs -lt 1000 * $Manifest.ActiveStepMilliseconds * $Record.ActiveSteps) {
            throw 'Incomplete active lifecycle window.'
        }
    }
    if ($Record.Measurements.Count -ne $Manifest.Samples -or
        $Record.IdleUs -lt 1000 * $Manifest.IdleMilliseconds -or $timingUs -le 0) {
        throw 'Incomplete lifecycle measurements.'
    }
    foreach ($role in @('Caller','Agent','Child')) {
        $previous = $Record.IdleBefore[$role]
        foreach ($window in $windows) {
            $sample = $Record[$window][$role]
            if ($sample.CreationTime -le 0 -or $sample.CreationTime -ne $previous.CreationTime -or
                $sample.Cpu100ns -lt $previous.Cpu100ns -or $sample.Handles -le 0 -or
                $sample.PrivateBytes -le 0 -or $sample.WorkingSetBytes -lt 0) {
                throw ('Invalid process resource sample: ' + $role)
            }
            $previous = $sample
        }
    }
    $last = 0
    $measuredUs = 0L
    for ($i = 0; $i -lt $Record.Measurements.Count; $i++) {
        $sample = $Record.Measurements[$i]
        foreach ($phase in $phases) {
            $duration = $sample[$phase]
            if (($duration -isnot [int] -and $duration -isnot [long]) -or $duration -lt 0 -or $duration -gt $timingUs) {
                throw ('Invalid ' + $phase + ' at measurement ' + ($i + 1))
            }
            $measuredUs += $duration
        }
        if ($measuredUs -gt $timingUs -or $sample.CapturedBytes -le $last -or $sample.CapturedBytes -gt $StreamBytes) {
            throw ('Invalid timing window or stream offset at measurement ' + ($i + 1))
        }
        Confirm-LifecycleSnapshot (Join-Path $Directory ('console-' + ($i + 1) + '.bin')) ($i + 1) $Manifest
        $last = $sample.CapturedBytes
    }
    if ($extended) {
        if ($Record.ActiveCapturedBytes -le $last -or $Record.ActiveCapturedBytes -gt $StreamBytes) {
            throw 'Wrong active lifecycle stream offset.'
        }
        Confirm-LifecycleSnapshot (Join-Path $Directory 'console-active-final.bin') ($Manifest.Samples + $Record.ActiveSteps) $Manifest
    }
}

function Read-LifecycleCase([string]$Directory, $Spec, [int]$ExitCode, $Manifest, [string]$Role) {
    $record = Read-VT7Json (Join-Path $Directory 'case.json')
    $path = Join-Path $Directory 'stream.bin'
    $bytes = (Get-Item -LiteralPath $path).Length
    if ($record.SchemaVersion -ne 1 -or $record.Case -cne $Spec.Name -or $bytes -gt 1048576 -or
        ($ExitCode -eq 0 -and ($record.Status -cne 'Pass' -or $record.Category -cne 'Observed' -or
            -not $record.OwnedProcessesCleaned -or $bytes -le 0)) -or
        ($ExitCode -eq 1 -and ($record.Status -cne 'Fail' -or $record.Category -cne 'BehaviorMismatch')) -or
        @(0,1) -notcontains $ExitCode) { throw ('Incomplete lifecycle observation: ' + $record.Error) }

    $ascii = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($path))
    if (@('LIFECYCLE_CMD_DRAIN','LIFECYCLE_PS_DRAIN','LIFECYCLE_NEGATIVE_DRAIN') -ccontains $Spec.Name -and
        $record.ContainsKey('RowsMatched')) {
        $tokens = [regex]::Matches($ascii,'VT7ROW_([1-9][0-9]*)_END')
        $position = 0
        $matched = 0
        for ($i = 0; $i -lt $tokens.Count -and $i -lt $Manifest.OutputRows; $i++) {
            if ($tokens[$i].Groups[1].Value -cne [string]($i + 1)) { break }
            $position = $tokens[$i].Index + $tokens[$i].Length
            $matched++
        }
        $ordered = $matched -eq $Manifest.OutputRows -and [regex]::Matches($ascii,'VT7ROW_').Count -eq $matched
        $tail = [regex]::Matches($ascii,'VT7DRAIN_END')
        $final = $tail.Count -eq 1 -and $tail[0].Index -ge $position
        $drain = $record.ChildExitedBeforeReader -and $record.OutputEof -and
            $record.ChildExitCode -eq 37 -and $record.AgentExitedAfterDrain -and $ordered
        if ($ordered -ne $record.RowsMatched -or $final -ne $record.FinalMarkerMatched -or
            ($ExitCode -eq 0 -and (-not $drain -or -not $final -or $Spec.Name -ceq 'LIFECYCLE_NEGATIVE_DRAIN')) -or
            ($record.Error -ceq 'final marker missing from drained output' -and
                (-not $drain -or $tail.Count -ne 0 -or $Spec.Name -cne 'LIFECYCLE_NEGATIVE_DRAIN'))) {
            throw 'Drain verdict disagrees with recorded rows, tail or shutdown.'
        }
    } elseif ($ExitCode -eq 0) {
        if ($Spec.Name -ceq 'LIFECYCLE_RECOVER') {
            if ($record.UnexpectedStart -or $record.UnexpectedChildHandle -or $record.SpawnApiError -ne 2 -or
                $record.SpawnWin32Error -ne 2 -or -not $record.Recovered -or -not $ascii.Contains('VT7_RECOVERED')) {
                throw 'Wrong spawn failure or retry evidence.'
            }
        } elseif ($Spec.Name -ceq 'LIFECYCLE_CLOSE') {
            if ($record.ExplicitCloseAvailable -ne ($Role -ceq 'accepted') -or -not $record.CloseContract -or
                -not $record.SelectedExited -or -not $record.NeighborAlive -or -not $record.NeighborFinalDrain -or
                -not $ascii.Contains('FINAL_DRAIN') -or $record.ShutdownMicroseconds -lt 0 -or
                $record.ShutdownMicroseconds -gt 1000 * $Manifest.ShutdownMilliseconds) {
                throw 'Wrong close or neighbor evidence.'
            }
        } elseif ($Spec.Area -ceq 'F09') {
            Confirm-LifecycleMeasurements $Directory $record $Manifest $bytes
            if (-not $record.FinalMarkerMatched -or -not $ascii.Contains('FINAL_DRAIN')) { throw 'Measurement final marker missing.' }
        } else { throw 'Missing delayed-reader evidence.' }
    }
    return $record
}
