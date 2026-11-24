# Shared package and result checks. Keep this compatible with Windows PowerShell 2.0.
function Confirm-FidelityPackage([string]$Root, $Manifest, [string[]]$Roles = @('baseline','accepted')) {
    if (-not $Manifest.Harness.SourceTreeClean -or $Manifest.Harness.SourceCommit -cnotmatch '^[0-9a-f]{40}$' -or
        $Manifest.Harness.SnapshotBytes -lt 8000 -or $Manifest.Harness.SnapshotBytes -gt 65536 -or
        $Manifest.SessionFlags -ne 0 -or $Manifest.Runtimes.Count -ne $Roles.Count) { throw 'Wrong fidelity harness or runtime count.' }
    $files = @{}
    foreach ($entry in $Manifest.Files) {
        if ($entry.Path -notmatch '^[a-zA-Z0-9_.-]+(/[a-zA-Z0-9_.-]+)*$' -or
            $entry.Path -match '(^|/)\.\.?(/|$)' -or $files.ContainsKey($entry.Path)) {
            throw 'Duplicate or invalid package inventory path.'
        }
        $file = Join-Path $Root $entry.Path
        if ((Get-Item -LiteralPath $file).Length -ne $entry.Bytes -or (Get-VT7Sha256 $file) -cne $entry.Sha256) {
            throw ('Package file changed: ' + $entry.Path)
        }
        $files[$entry.Path] = $entry
    }
    foreach ($role in $Roles) {
        $runtime = $Manifest.Runtimes[$role]
        $built = $role -ceq 'candidate'
        $sourcePath = if ($built) { $role + '/source-verification.json' } else { $role + '/source-manifest.json' }
        if (-not $files.ContainsKey($sourcePath) -or $files[$sourcePath].Sha256 -cne $runtime.ManifestSha256) {
            throw ('Original runtime manifest does not match: ' + $role)
        }
        $original = Read-VT7Json (Join-Path $Root $sourcePath)
        $originalIdentity = if ($role -ceq 'accepted') { $original.Runtime } else { $original }
        if ($originalIdentity.SourceCommit -cne $runtime.SourceCommit) { throw 'Runtime source identity differs.' }
        $requiredBinaries = @('VT7Pty.dll','VT7Pty-Agent.exe')
        if ($built) {
            if ($runtime.SourceCommit -cne $Manifest.Harness.SourceCommit -or
                $runtime.ManifestSha256 -cne $Manifest.Harness.VerificationSha256 -or
                -not $original.SourceTreeClean -or $original.Configuration -cne 'Release' -or
                $original.Architecture -cne 'x64') { throw 'Wrong candidate build verification.' }
            $requiredBinaries += 'VT7Pty-Host.dll','VT7Pty-DebugServer.exe'
        }
        foreach ($required in $requiredBinaries) {
            if (-not $runtime.Binaries.ContainsKey($required)) { throw ('Missing runtime binary: ' + $required) }
        }
        $referenceFiles = if ($built) { $original.Artifacts } else { $original.Files }
        $referenceField = if ($built) { 'Name' } else { 'Path' }
        foreach ($name in $runtime.Binaries.Keys) {
            $installedPath = $role + '/bin/' + $name
            $originalPath = $runtime.Binaries[$name]
            $reference = @($referenceFiles | Where-Object { $_[$referenceField] -ceq $originalPath })
            if ($reference.Count -ne 1 -or -not $files.ContainsKey($installedPath) -or
                $reference[0].Sha256 -cne $files[$installedPath].Sha256 -or
                $reference[0].Bytes -ne $files[$installedPath].Bytes) {
                throw ('Runtime binary differs from its recorded source: ' + $installedPath)
            }
        }
        foreach ($name in @('FidelityTest.exe','FidelityFixture.exe')) {
            $base = $files['baseline/bin/' + $name]
            $harness = $files[$role + '/bin/' + $name]
            if (-not $base -or -not $harness -or $base.Sha256 -cne $harness.Sha256) {
                throw ('All runtimes must use the same fidelity harness: ' + $name)
            }
        }
    }
}

function Read-OutputPackage([string]$Root) {
    $manifest = Read-VT7Json (Join-Path $Root 'manifest.json')
    if ($manifest.Kind -cne 'VT7PtyOutputCharacterization' -or @(1,2) -notcontains $manifest.SchemaVersion -or
        $manifest.CheckpointsPerCase -ne 2 -or $manifest.Step13Accepted) { throw 'Wrong output characterization package.' }
    $seen = @{}
    $widthCases = 0
    foreach ($entry in $manifest.Cases) {
        if ($entry.Name -cnotmatch '^OUTPUT_[A-Z0-9_]+$' -or @('F04','F05') -cnotcontains $entry.Area -or
            $seen.ContainsKey($entry.Name)) { throw 'Invalid or repeated output case.' }
        $seen[$entry.Name] = $true
        if ($manifest.SchemaVersion -eq 2) {
            if ($entry.WidthProbe -isnot [bool]) { throw 'Missing width probe recipe field.' }
            if ($entry.WidthProbe) { $widthCases++ }
        }
    }
    $expectedCases = if ($manifest.SchemaVersion -eq 1) { 13 } else { 21 }
    if ($seen.Count -ne $expectedCases -or ($manifest.SchemaVersion -eq 2 -and $widthCases -ne 10)) {
        throw 'Wrong output case count.'
    }
    Confirm-FidelityPackage $Root $manifest
    return $manifest
}

function Test-OutputWidthReplay($Record, [string]$StreamPath) {
    $stages = @($Record.OutputCheckpoints)
    $action = $stages[1]
    $replay = $Record.WidthReplay
    if ($action.CapabilityUnavailable) {
        if ($replay.Status -cne 'CapabilityUnavailable') { throw 'Width replay ignored the applied code page.' }
        return
    }
    if (@($stages | Where-Object { -not $_.BarrierSeen -or -not $_.Settled -or $_.ReaderError }).Count) {
        if ($replay.Status -cne 'FrameUnavailable') { throw 'Width replay requires both settled frames.' }
        return
    }
    if (-not $action.NativeTextMatchesIntent -or $action.NativeTrailingBarColumn -lt 0) {
        if ($replay.Status -cne 'NativeSampleChanged') { throw 'Width replay requires the original native sample.' }
        return
    }
    if ($replay.Status -ceq 'PayloadUnavailable') { return }
    if ($replay.Status -cne 'ReplayComplete' -or $replay.NativeBarColumn -ne $action.NativeTrailingBarColumn -or
        $replay.InsertionOffset -lt $stages[0].CapturedBytes -or $replay.InsertionOffset -ge $action.CapturedBytes) {
        throw 'Width replay receipt or insertion offset differs.'
    }
    $stream = [IO.File]::ReadAllBytes($StreamPath)
    $escape = [string][char]27 + '[' + ($replay.NativeBarColumn + 1) + 'G'
    if ($stream[$replay.InsertionOffset] -ne 124 -or $replay.InsertedSequence -cne $escape) {
        throw 'Unexpected width replay byte or column command.'
    }
    if (($replay.Reference.Text -join ',') -cne ($action.ReconstructedText -join ',') -or
        $replay.Reference.BarColumn -ne $action.ReconstructedTrailingBarColumn -or
        $replay.Reference.CursorMatchesNative -ne $action.ReconstructedCursorMatchesNative) {
        throw 'Reference replay differs from the captured reconstruction.'
    }
    $nativeText = $action.NativeText -join ','
    $nonblank = ($action.NativeText | Where-Object { $_ -ne 32 }) -join ','
    foreach ($variant in @($replay.Reference,$replay.OneCellSamples,$replay.Anchored)) {
        $points = $variant.Text -join ','
        $visiblePoints = ($variant.Text | Where-Object { $_ -ne 32 }) -join ','
        if ($variant.TextMatchesNative -ne ($points -ceq $nativeText) -or
            $variant.NonblankTextMatchesNative -ne ($visiblePoints -ceq $nonblank) -or
            $variant.BarMatchesNative -ne ($variant.BarColumn -eq $replay.NativeBarColumn)) {
            throw 'Width replay verdict disagrees with its recorded row.'
        }
    }
}

function Read-OutputCase([string]$Directory, $Spec, [int]$ExitCode, [int]$SnapshotBytes) {
    $record = Read-VT7Json (Join-Path $Directory 'case.json')
    $stages = @($record.OutputCheckpoints)
    if ($record.SchemaVersion -ne 1 -or $record.Case -cne $Spec.Name -or -not $record.OwnedProcessesCleaned -or
        $stages.Count -ne 2 -or (($stages | ForEach-Object { $_.Phase }) -join ',') -cne '1,2' -or
        ($ExitCode -eq 0 -and $record.Status -cne 'Pass') -or
        ($ExitCode -eq 1 -and ($record.Status -cne 'Fail' -or $record.Category -cne 'BehaviorMismatch')) -or
        @(0,1) -notcontains $ExitCode) {
        throw ('Incomplete output observation: ' + $Spec.Name)
    }
    $stream = Get-Item -LiteralPath (Join-Path $Directory 'stream.bin')
    if ($stream.Length -lt 1 -or $stream.Length -gt 1048576 -or
        $stages[0].CapturedBytes -gt $stages[1].CapturedBytes -or $stages[1].CapturedBytes -gt $stream.Length) {
        throw 'Invalid raw stream boundaries.'
    }
    foreach ($stage in $stages) {
        $snapshot = Get-Item -LiteralPath (Join-Path $Directory ('console-' + $stage.Phase + '.bin'))
        if ($snapshot.Length -ne $SnapshotBytes -or $stage.Console.Columns -ne 80 -or $stage.Console.Rows -ne 25) {
            throw 'Missing or malformed native snapshot.'
        }
        $streamAtCheckpoint = $stage.CapturedBytes
        if ($streamAtCheckpoint -lt 0 -or $streamAtCheckpoint -gt $stream.Length) { throw 'Invalid checkpoint extent.' }
    }
    $action = $stages[1]
    $requestedMode = if ($Spec.Name -ceq 'OUTPUT_RAW_VT') { 7 } else { 0 }
    if ($action.RequestedOutputCP -ne $Spec.RequestedOutputCP -or $action.RequestedOutputMode -ne $requestedMode) {
        throw 'Output capability request differs from the compiled recipe.'
    }
    if ($action.RequestedOutputCP -or $action.RequestedOutputMode) {
        $enabled = $action.SetupSucceeded -and
            (-not $action.RequestedOutputCP -or $action.Console.OutputCP -eq $action.RequestedOutputCP) -and
            (-not $action.RequestedOutputMode -or
                ($action.Console.OutputMode -band $action.RequestedOutputMode) -eq $action.RequestedOutputMode)
        if ($action.WriteAttempted -ne $enabled -or $action.CapabilityUnavailable -eq $enabled) {
            throw 'Write and applied code page or output mode disagree.'
        }
    }
    if ($ExitCode -eq 0 -and ($record.BackingConsoleDiffers -or $record.ReconstructionDiffers -or
        -not $record.CompleteStreamValidUtf8)) { throw 'Output pass conflicts with native evidence.' }
    if ($Spec.ContainsKey('WidthProbe') -and $Spec.WidthProbe) {
        Test-OutputWidthReplay $record $stream.FullName
    }
    return $record
}
