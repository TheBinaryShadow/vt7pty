# Shared package and result checks. Keep this compatible with Windows PowerShell 2.0.
function Read-OutputPackage([string]$Root) {
    $manifest = Read-VT7Json (Join-Path $Root 'manifest.json')
    if ($manifest.Kind -cne 'VT7PtyOutputCharacterization' -or $manifest.SchemaVersion -ne 1 -or
        -not $manifest.Harness.SourceTreeClean -or $manifest.Harness.SourceCommit -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.Harness.SnapshotBytes -lt 8000 -or $manifest.Harness.SnapshotBytes -gt 65536 -or
        $manifest.SessionFlags -ne 0 -or $manifest.CheckpointsPerCase -ne 2 -or $manifest.Step13Accepted) {
        throw 'Wrong output characterization package.'
    }
    $files = @{}
    foreach ($entry in $manifest.Files) {
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
    $seen = @{}
    foreach ($entry in $manifest.Cases) {
        if ($entry.Name -cnotmatch '^OUTPUT_[A-Z0-9_]+$' -or @('F04','F05') -cnotcontains $entry.Area -or
            $seen.ContainsKey($entry.Name)) { throw 'Invalid or repeated output case.' }
        $seen[$entry.Name] = $true
    }
    if ($seen.Count -ne 13) { throw 'Wrong output case count.' }

    foreach ($role in @('baseline','accepted')) {
        $runtime = $manifest.Runtimes[$role]
        $sourcePath = $role + '/source-manifest.json'
        if (-not $files.ContainsKey($sourcePath) -or $files[$sourcePath].Sha256 -cne $runtime.ManifestSha256) {
            throw ('Original runtime manifest does not match: ' + $role)
        }
        $original = Read-VT7Json (Join-Path $Root $sourcePath)
        $originalIdentity = if ($role -ceq 'baseline') { $original } else { $original.Runtime }
        if ($originalIdentity.SourceCommit -cne $runtime.SourceCommit) { throw 'Runtime source identity differs.' }
        foreach ($required in @('VT7Pty.dll','VT7Pty-Agent.exe')) {
            if (-not $runtime.Binaries.ContainsKey($required)) { throw ('Missing runtime binary: ' + $required) }
        }
        foreach ($name in $runtime.Binaries.Keys) {
            $installedPath = $role + '/bin/' + $name
            $originalPath = $runtime.Binaries[$name]
            $reference = @($original.Files | Where-Object { $_.Path -ceq $originalPath })
            if ($reference.Count -ne 1 -or -not $files.ContainsKey($installedPath) -or
                $reference[0].Sha256 -cne $files[$installedPath].Sha256 -or
                $reference[0].Bytes -ne $files[$installedPath].Bytes) {
                throw ('Extracted runtime differs from its source archive: ' + $installedPath)
            }
        }
    }
    foreach ($name in @('FidelityTest.exe','FidelityFixture.exe')) {
        $base = $files['baseline/bin/' + $name]
        $accepted = $files['accepted/bin/' + $name]
        if (-not $base -or -not $accepted -or $base.Sha256 -cne $accepted.Sha256) {
            throw ('Both runtimes must use the same output harness: ' + $name)
        }
    }
    return $manifest
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
    return $record
}
