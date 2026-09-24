# Diagnostics

VT7Pty diagnostics are designed for repeatable collection on the physical
Windows 7 machines used for platform acceptance. The diagnostic transport is
local-only. Copy the candidate package to the target machine, reproduce the
failure there, and copy the resulting bundle back for analysis.

## Record format and limits

`VT7Pty.dll` and `VT7Pty-Agent.exe` emit one JSON object per record. Every
record contains:

- an ISO 8601 UTC timestamp with millisecond precision;
- severity and subsystem fields;
- package, source commit, API, and protocol identity;
- process name, process ID, and thread ID; and
- a single-line escaped message.

Records are limited to 4,096 bytes. The diagnostic client bounds both its busy
pipe wait and transaction wait at 100 milliseconds and also sends records to
`OutputDebugString`. Diagnostics therefore cannot indefinitely block a
VT7Pty session.

The native server accepts `--output PATH` and writes complete JSON lines up to
an 8 MiB default limit. `--max-bytes` can select a limit from zero through
64 MiB. Once the limit is reached, collection continues to standard output
but the file does not grow. The pipe is local-only and grants access to the
current owner, LocalSystem, and administrators. The former broad-access mode
has been removed.

## Collection on a physical machine

Open Windows PowerShell in the extracted package directory and start the
collector:

```powershell
& .\bin\VT7Pty-DebugServer.exe --output .\vt7pty-diagnostics.jsonl
```

In a second Windows PowerShell window, set the diagnostic flag and launch the
program that loads VT7Pty from that same window so the setting is inherited:

```powershell
$env:VT7PTY_DEBUG = 'trace'
# Launch the VT7 or test command that reproduces the failure here.
```

Reproduce the failure, close the tested program, and press `Ctrl+C` in the
collector window. Then create the bundle:

```powershell
& .\tools\diagnostics\New-VT7PtyDiagnosticBundle.ps1 `
    -LogPath .\vt7pty-diagnostics.jsonl `
    -PackageDirectory .
```

The bundle contains the bounded log, parsed record counts, build identities,
host version, installed hotfix inventory, package identity, and SHA-256 hashes
for shipped binaries. The script reports invalid JSON lines or mixed build
identities rather than silently treating them as a single run.

On the PowerShell 2.0 machine, add `-NoArchive` to the bundle command and copy
the resulting diagnostic directory. The acceptance runner uses this mode.

Run the collection independently on each physical target. Name the returned
archive with the machine and Windows 7 servicing state before copying it into
the validation results. The debug pipe rejects remote connections; the
diagnostic archive is the supported transfer boundary.

## Privacy

Default `trace` output excludes child command lines, environment contents,
terminal input, pipe capability names, raw handles, and application-facing
error text. It records operation outcomes, numeric Windows error codes, and
terminal geometry needed to diagnose failures.

The additional `input` flag deliberately records keyboard and mouse details
and may capture credentials or private session content:

```powershell
$env:VT7PTY_DEBUG = 'trace,input'
```

Use it only for an input-specific reproduction with non-sensitive test data.
The bundle collector detects the current input-record forms, sets
`ContainsOptInInputRecords`, and warns before the archive is shared. Always
review `vt7pty-diagnostics.jsonl` before sharing a bundle.

`input_separated_bytes` changes how input is delivered for parser testing;
it is not a passive logging option. The `force_sw_hide` and `no_sw_hide`
options also change window behavior. Do not use these flags in an ordinary
diagnostic baseline.

## Validation commands

The debug server exposes two non-collecting checks:

```powershell
& .\bin\VT7Pty-DebugServer.exe --version
& .\bin\VT7Pty-DebugServer.exe --self-test
```

`--version` prints the exact version, commit, API, and protocol identity.
`--self-test` creates the real local pipe, reads back its security descriptor,
and fails if Everyone or Anonymous access is present.
