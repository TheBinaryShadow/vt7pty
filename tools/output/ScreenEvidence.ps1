# Full-screen package and result checks. Keep these compatible with PowerShell 2.0.
function Get-ScreenRuntimeRoles($Manifest) {
    'baseline'
    'accepted'
    if ($Manifest.SchemaVersion -eq 2 -or
        ($Manifest.SchemaVersion -eq 3 -and $Manifest.Runtimes.ContainsKey('candidate'))) { 'candidate' }
}

function Read-ScreenPackage([string]$Root) {
    $manifest = Read-VT7Json (Join-Path $Root 'manifest.json')
    if ($manifest.Kind -cne 'VT7PtyScreenCharacterization' -or @(1,2,3) -notcontains $manifest.SchemaVersion -or
        $manifest.Step14Accepted -or $manifest.ResizePairs -ne 4) { throw 'Wrong screen characterization package.' }
    $required = @{
        ALTERNATE = 'F06/Control'; RESIZE = 'F07/Control'
        SCREEN_CMD_EXPLICIT = 'F06/Explicit'; SCREEN_CMD_EXIT = 'F06/Implicit'; SCREEN_PS_EXIT = 'F06/Implicit'
        SCREEN_CMD_RESIZE = 'F07/Resize'; SCREEN_PS_RESIZE = 'F07/Resize'
    }
    if ($manifest.SchemaVersion -eq 3) {
        if ($manifest.ViewportResizePairs -ne 2) { throw 'Wrong viewport resize count.' }
        $required.Remove('RESIZE')
        $required.SCREEN_VIEWPORT = 'F07/Viewport'
        $required.SCREEN_FIXED_ROWS = 'F07/FixedRows'
    }
    foreach ($case in $manifest.Cases) {
        if (-not $required.ContainsKey($case.Name) -or $required[$case.Name] -cne ($case.Area + '/' + $case.Mode)) {
            throw 'Invalid or repeated screen case.'
        }
        $required.Remove($case.Name)
    }
    if ($required.Count) { throw 'Missing screen cases.' }
    Confirm-FidelityPackage $Root $manifest @(Get-ScreenRuntimeRoles $manifest)
    return $manifest
}

function Read-ScreenSnapshot([string]$Path, [int]$Bytes) {
    $snapshot = [IO.File]::ReadAllBytes($Path)
    if ($snapshot.Length -ne $Bytes -or [BitConverter]::ToUInt32($snapshot, 0) -ne 0x31465456 -or
        [BitConverter]::ToUInt32($snapshot, 4) -ne 1) { throw 'Wrong screen snapshot size or identity.' }
    return ,$snapshot
}

function Confirm-ScreenGeometry([byte[]]$Snapshot, $Console) {
    $fields = @('Columns','Rows','BufferColumns','BufferRows','WindowLeft','WindowTop','CursorX','CursorY')
    for ($i = 0; $i -lt $fields.Count; $i++) {
        if ([BitConverter]::ToUInt32($Snapshot, 8 + 4 * $i) -ne $Console[$fields[$i]]) {
            throw ('Native screen dimensions or cursor differ: ' + $fields[$i])
        }
    }
}

function Test-ScreenText([byte[]]$Snapshot, $Console, [int]$X, [int]$Y, [string]$Text, [int]$Attribute) {
    if ($Y -lt 0 -or $Y -ge $Console.Rows -or $X -lt 0 -or $X + $Text.Length -gt $Console.Columns) { return $false }
    $offset = $Snapshot.Length - 19200 + 4 * ($Y * $Console.Columns + $X)
    for ($i = 0; $i -lt $Text.Length; $i++) {
        if ([BitConverter]::ToUInt16($Snapshot, $offset + 4 * $i) -ne [int]($Text[$i]) -or
            [BitConverter]::ToUInt16($Snapshot, $offset + 4 * $i + 2) -ne $Attribute) { return $false }
    }
    return $true
}

function Confirm-ViewportCase([string]$Directory, $Spec, $Record, $Manifest, [long]$StreamBytes, [int]$ExitCode) {
    $bytes = $Manifest.Harness.SnapshotBytes
    $previous = $Record.InitialViewport
    $initialTop = $previous.WindowTop
    $initial = Read-ScreenSnapshot (Join-Path $Directory 'console-initial.bin') $bytes
    Confirm-ScreenGeometry $initial $previous
    $stages = @($Record.ViewportResizes)
    if ($stages.Count -ne 2 * $Manifest.ViewportResizePairs -or -not $Record.OwnedProcessesCleaned) {
        throw 'Incomplete viewport resize observations.'
    }
    $last = 0
    $matched = $Record.FinalDrainMatches
    for ($i = 0; $i -lt $stages.Count; $i++) {
        $stage = $stages[$i]
        $prefix = 'console-resize-' + ($i + 1)
        $before = Read-ScreenSnapshot (Join-Path $Directory ($prefix + '-before-repaint.bin')) $bytes
        $final = Read-ScreenSnapshot (Join-Path $Directory ($prefix + '.bin')) $bytes
        Confirm-ScreenGeometry $before $stage.BeforeRepaint
        Confirm-ScreenGeometry $final $stage.Console
        $columns = if ($i % 2 -eq 0) { 100 } else { 80 }
        $rows = if ($i % 2 -eq 0) { 30 } else { 25 }
        if ($stage.Columns -ne $columns -or $stage.Rows -ne $rows -or $stage.CapturedBytesBefore -lt $last -or
            $stage.CapturedBytesBefore -gt $stage.CapturedBytes -or $stage.CapturedBytes -gt $StreamBytes) {
            throw 'Wrong viewport resize request or capture offsets.'
        }
        $last = $stage.CapturedBytes
        $size = $true
        foreach ($info in @($stage.BeforeRepaint,$stage.Console)) {
            $size = $size -and $info.Columns -eq $columns -and $info.Rows -eq $rows -and $info.BufferColumns -eq $columns
        }
        $markerRow = [int]$previous.WindowTop + 1 - [int]$stage.BeforeRepaint.WindowTop
        $fixed = Test-ScreenText $before $stage.BeforeRepaint 2 $markerRow 'STATE_READY' 7
        $console = $stage.Console
        $decoded = $stage.DecodedState
        $offset = [int]$console.WindowTop - [int]$initialTop
        $geometry = $console.WindowLeft -eq 0 -and $console.Columns -eq $decoded.Columns -and $console.Rows -eq $decoded.Rows
        $cursor = $geometry -and $console.CursorX -eq $decoded.CursorX -and $console.CursorY + $offset -eq $decoded.CursorY -and
            ([BitConverter]::ToUInt32($final, 40) -ne 0) -eq $stage.ReconstructedCursorVisible
        $overlap = if ($geometry) { [Math]::Max(0, [int]$console.Rows - [Math]::Abs($offset)) } else { 0 }
        $uncovered = 0
        if ($geometry -and $offset -ne 0) {
            $cells = $final.Length - 19200
            for ($y = 0; $y -lt $console.Rows; $y++) {
                if ($y + $offset -ge 0 -and $y + $offset -lt $decoded.Rows) { continue }
                for ($x = 0; $x -lt $console.Columns; $x++) {
                    $index = $cells + 4 * ($y * $console.Columns + $x)
                    if ([BitConverter]::ToUInt16($final, $index) -ne 32 -or [BitConverter]::ToUInt16($final, $index + 2) -ne 7) {
                        $uncovered++
                    }
                }
            }
        }
        $aligned = $geometry -and $cursor -and $overlap -gt 0 -and $stage.CellDifferences -eq 0 -and
            $stage.AttributeDifferences -eq 0 -and $uncovered -eq 0
        if ($stage.ViewportRowOffset -ne $offset -or $stage.ComparedRows -ne $overlap -or $stage.RequestedSizeMatches -ne $size -or
            $stage.FixedMarkerBeforeRepaint -ne $fixed -or $stage.ViewportGeometryMatches -ne $geometry -or
            $stage.ViewportCursorMatches -ne $cursor -or $stage.UncoveredNonblankCells -ne $uncovered -or
            $stage.AlignedScreenMatches -ne $aligned) {
            throw 'Viewport verdict disagrees with the measured origin, geometry or fixed marker.'
        }
        $matched = $matched -and $aligned -and $size -and ($Spec.Mode -cne 'FixedRows' -or $fixed)
        $previous = $console
    }
    if ($matched -ne ($ExitCode -eq 0)) { throw 'Viewport result disagrees with the recorded checkpoints.' }
}

function Read-ScreenCase([string]$Directory, $Spec, [int]$ExitCode, $Manifest) {
    $native = Read-VT7Json (Join-Path $Directory 'case.json')
    $stream = Get-Item -LiteralPath (Join-Path $Directory 'stream.bin')
    if ($native.SchemaVersion -ne 1 -or $native.Case -cne $Spec.Name -or $stream.Length -lt 1 -or $stream.Length -gt 1048576 -or
        ($ExitCode -eq 0 -and ($native.Status -cne 'Pass' -or $native.Category -cne 'Observed')) -or
        ($ExitCode -eq 1 -and ($native.Status -cne 'Fail' -or $native.Category -cne 'BehaviorMismatch')) -or
        @(0,1) -notcontains $ExitCode) { throw ('Incomplete screen observation: ' + $Spec.Name + ' ' + $native.Error) }
    $bytes = $Manifest.Harness.SnapshotBytes
    if ($Spec.Mode -ceq 'Control') {
        [void](Read-ScreenSnapshot (Join-Path $Directory 'console-snapshot.bin') $bytes)
        if (-not $native.OwnedProcessesCleaned -or ($ExitCode -eq 0 -and -not $native.FinalDrainMatches)) {
            throw 'Incomplete screen control.'
        }
        return $native
    }
    if (@('Viewport','FixedRows') -ccontains $Spec.Mode) {
        Confirm-ViewportCase $Directory $Spec $native $Manifest $stream.Length $ExitCode
        return $native
    }

    $stages = @($native.ScreenCheckpoints)
    $resizes = @($native.ResizeChecks)
    $failed = $native.ContainsKey('FailedResize')
    $labels = @('primary','alternate')
    if ($Spec.Mode -ceq 'Explicit') { $labels += 'explicit-restore' }
    $count = if ($Spec.Mode -ceq 'Resize') { 2 * $Manifest.ResizePairs } else { 0 }
    if ($count -and $resizes.Count -eq $count) { $labels += 'settled-edit' }
    if (-not $failed) { $labels += 'after-exit' }
    if (($labels -join ',') -cne (($stages | ForEach-Object { $_.Name }) -join ',') -or
        $resizes.Count -gt $count -or ($failed -and (-not $count -or $native.ScreenWorkflowCompleted)) -or
        (-not $failed -and $resizes.Count -ne $count)) { throw 'Wrong screen checkpoint sequence.' }

    $match = $true
    $last = 0
    foreach ($stage in $stages) {
        $snapshot = Read-ScreenSnapshot (Join-Path $Directory ('console-' + $stage.Name + '.bin')) $bytes
        $console = $stage.Console
        Confirm-ScreenGeometry $snapshot $console
        $large = $count -and $stage.Name -ceq 'after-exit'
        $columns = if ($large) { 100 } else { 80 }
        $rows = if ($large) { 30 } else { 25 }
        if ($stage.NativeCursorVisible -ne ([BitConverter]::ToUInt32($snapshot, 40) -ne 0) -or
            $stage.CapturedBytes -lt $last -or $stage.CapturedBytes -gt $stream.Length -or
            $stage.RequestedColumns -ne $columns -or $stage.RequestedRows -ne $rows) { throw 'Wrong checkpoint cursor, size or offset.' }
        $last = $stage.CapturedBytes
        $alternate = @('alternate','settled-edit') -ccontains $stage.Name
        if ($Manifest.SchemaVersion -eq 3) {
            $text = if ($alternate) { 'SCREEN_ALTERNATE' } else { 'SCREEN_PRIMARY' }
            $attribute = if ($alternate) { 0x2f } else { 0x1e }
            $original = Test-ScreenText $snapshot $console 2 1 $text $attribute
            $reflow = $large -and (Test-ScreenText $snapshot $console 82 0 $text $attribute)
            $placement = if ($original -eq $reflow) { 'Unexpected' } elseif ($original) { 'Original' } else { 'Reflowed' }
            if ($stage.NativeMarkerPlacement -cne $placement -or $stage.NativeActiveMarker -ne ($placement -cne 'Unexpected')) {
                throw ('Native marker placement disagrees with snapshot: ' + $stage.Name)
            }
        }
        $geometry = $console.Columns -eq $columns -and $console.Rows -eq $rows -and $console.BufferColumns -eq $columns -and
            (-not $alternate -or ($console.BufferRows -eq $rows -and $console.WindowLeft -eq 0 -and $console.WindowTop -eq 0))
        $decoded = $stage.DecodedState
        $reconstructed = $console.Columns -eq $decoded.Columns -and $console.Rows -eq $decoded.Rows -and
            $console.CursorX -eq $decoded.CursorX -and $console.CursorY -eq $decoded.CursorY -and
            $stage.NativeCursorVisible -eq $stage.ReconstructedCursorVisible -and
            $decoded.RawUtf16CellDifferences -eq 0 -and $decoded.AttributeDifferences -eq 0
        if ($geometry -ne $stage.NativeGeometryMatches -or $reconstructed -ne $stage.ReconstructedMatches) {
            throw 'Screen comparisons disagree with the recorded observations.'
        }
        $match = $match -and $geometry -and $reconstructed -and $stage.NativeActiveMarker -and $stage.NativeFrameMatches
    }

    $last = $stages[1].CapturedBytes
    $draws = 0
    for ($i = 0; $i -lt $resizes.Count; $i++) {
        $resize = $resizes[$i]
        $columns = if ($i % 2 -eq 0) { 100 } else { 80 }
        $rows = if ($i % 2 -eq 0) { 30 } else { 25 }
        $child = $resize.Child
        $geometry = $child.Columns -eq $columns -and $child.Rows -eq $rows -and
            $child.BufferColumns -eq $columns -and $child.BufferRows -eq $rows -and $child.WindowLeft -eq 0 -and $child.WindowTop -eq 0
        if ($resize.Columns -ne $columns -or $resize.Rows -ne $rows -or $resize.CapturedBytesBefore -lt $last -or
            $resize.CapturedBytesBefore -gt $stream.Length -or $resize.NativeGeometryMatches -ne $geometry -or
            $resize.ContinuedDrawing -ne ($child.Draws -gt $draws)) { throw 'Resize request or child observation differs.' }
        $draws = $child.Draws
        $last = $resize.CapturedBytesBefore
        $match = $match -and $geometry -and $resize.ContinuedDrawing
    }
    if ($failed) {
        $failure = $native.FailedResize
        $columns = if ($resizes.Count % 2 -eq 0) { 100 } else { 80 }
        $rows = if ($resizes.Count % 2 -eq 0) { 30 } else { 25 }
        if ($ExitCode -ne 1 -or $failure.Columns -ne $columns -or $failure.Rows -ne $rows -or
            $failure.CapturedBytesBefore -lt $last -or $failure.CapturedBytesBefore -gt $stream.Length -or
            $failure.Error -cne $native.Error -or $failure.Error -cnotlike 'resize*') { throw 'Invalid failed resize receipt.' }
        return $native
    }
    if ($count) {
        if ($native.ResizeEventsReceived -ne ($stages[2].WindowEvents -gt 0) -or
            $native.ExitResizeCapturedBytesBefore -lt $stages[2].CapturedBytes -or
            $native.ExitResizeCapturedBytesBefore -gt $stages[3].CapturedBytes -or
            $native.ExitObservedBeforeResize -isnot [bool] -or $native.ExitObservedAfterResize -isnot [bool]) {
            throw 'Invalid resize/exit checkpoint.'
        }
        $match = $match -and $native.FullScreenEditMatches -and $native.ResizeEventsReceived
    }
    $statusPath = Join-Path $Directory 'child-exit-code.txt'
    $commandPath = Join-Path $Directory 'continued-command.txt'
    $shellStatus = if (Test-Path -LiteralPath $statusPath) { [IO.File]::ReadAllText($statusPath).Trim() } else { '' }
    $commandRan = (Test-Path -LiteralPath $commandPath) -and [IO.File]::ReadAllText($commandPath).Trim() -ceq 'SCREEN_SHELL_OK'
    if ($native.ShellReportedChildExit -cne $shellStatus -or $native.ShellReportedExitMatches -ne ($shellStatus -ceq '37') -or
        $native.FullScreenExitMatches -ne ($native.FullScreenExitCode -eq 37) -or $native.NextEditedCommandExecuted -ne $commandRan) {
        throw 'Shell resume receipts disagree.'
    }
    if ($Manifest.SchemaVersion -eq 3 -and $native.ReturnedToShellPrompt -and
        ($native.ReturnedPromptColumn -lt 0 -or $native.ReturnedPromptColumn -ge $stages[-1].RequestedColumns -or
        (-not $count -and $native.ReturnedPromptColumn -ne 0))) {
        throw 'Invalid restored prompt column.'
    }
    $match = $match -and $native.OwnedProcessesCleaned -and $native.ScreenWorkflowCompleted -and $native.InspectorExited -and
        $native.ReturnedToShellPrompt -and $native.FullScreenExitMatches -and $native.ShellReportedExitMatches -and
        $native.NextEditedCommandExecuted -and $native.ShellRepainted
    if ($match -ne ($ExitCode -eq 0)) { throw 'Full-screen result disagrees with the checkpoint and shell receipts.' }
    return $native
}
