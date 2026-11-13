# H3 cancellation diagnostics

I'm collecting evidence for the intermittent
[rc1 Windows 7 failures](validation/2026-11-13-step-1.2-host-cancellation-rc1-windows7.md).
This package alternates the exact issued rc1 controls with a separate traced
build. It is a diagnostic experiment; a traced pass does not replace a failed
control or accept H3.

The [Windows 7 review](validation/2026-11-13-step-1.2-host-cancellation-diagnostics-rc1-windows7.md)
records all 18 completed trials, including the PowerShell 2.0 run. Both traced
native delivery failures sampled an active selection before dispatch. H3 remains
open for a controlled scraper/cancellation experiment.

## Run on Windows 7

Extract **VT7Pty-host-cancellation-diagnostics-0.5.0-rc1.zip** into a fresh local
folder and keep the full directory tree together. Run as the ordinary test user
from the normal desktop:

| Machine | Launcher |
| --- | --- |
| NESSY | RUN-HOST-CANCELLATION-DIAGNOSTICS-NONESU.cmd |
| TURTLE | RUN-HOST-CANCELLATION-DIAGNOSTICS-ESU.cmd |
| LEOPARD | RUN-HOST-CANCELLATION-DIAGNOSTICS-LEGACY.cmd |

The launchers use x64 installed Windows PowerShell. PowerShell 2.0 is supported;
PowerShell 7 and KB3191566 are unnecessary. Do not elevate after a failure.

The runner executes **control 1, trace 1, control 2, trace 2, control 3, trace 3**.
Allow several minutes. The outer limits are 210 seconds for a control and
300 seconds for a traced trial. Keep the whole results directory, including
failed and partial trials. Keep the first run; do not repeat failed trials just
to obtain green results.

A trial has an original Pass/Fail outcome and a separate collection status.
Completed means its native result records were collected; it does not mean
cancellation succeeded or every diagnostic observation was available.
The runner returns failure if a trial fails or collection is incomplete.

## What stays in each comparison

The control directory contains all 18 files from the exact issued rc1 ZIP.
Its manifest and every original component are checked independently, in addition
to the outer package manifest. Controls are extracted from the issued archive,
not rebuilt from current source.

Both directories use the accepted 0.5.0 DLL/agent pair. The traced executable
and DLL have separate names, research protocol version 2 and their own source identity.
The Ctrl+C operation remains one synchronous WM_KEYDOWN C in the verified
host thread, with the same temporary Ctrl table and complete restoration.
There is no retry or appended GenerateConsoleCtrlEvent.

Instrumentation changes timing and some test preparation. The trace has an
additional raw-key case, public console observations and prompt identifiers.
This is why I retain separate, interleaved controls instead of treating the
two builds as interchangeable acceptance results.

## What the trace records

- Fixed 16-entry host traces: capture, apply, pre-send, nested key entry,
  nested key return, send return and restoration. Each sample keeps QPC/tick,
  PID/thread/HWND, keyboard state, layout, message arguments and send context.
- A fixed 64-entry fixture timeline: read entry/return and handler entry/exit,
  with published sequence numbers, thread IDs, read indices and timestamps.
- Public input mode, pending-record count and selection observations, including
  API errors. Snapshot intervals are recorded; the fields are not atomic.
- Controller request intervals and fixture observations before/after cancellation.
- Up to 512 output frames with byte offsets, cursor rows and observed prompt
  epochs. PowerShell also emits its own prompt counter; CMD uses a distinct
  preparation token. Neither establishes that a shell is blocked in ReadConsole.
- A separate host-key-record case that invokes the H3 operation with processed
  input disabled. It preserves the actual virtual key, scan code, modifiers
  and character value. Character zero is recorded faithfully.

The raw-input guard from rc1 still exercises the original input transport.
The new host-key case answers a different question about the window-message path.

Failed deadline snapshots retain their original result. A bounded five-second
follow-up can observe later read/event state before recovery. A predicate being
true during that follow-up does not prove it first became true there; the
timeline must establish when delivery occurred. Recovery Enter or z only allows
cleanup after a recorded failure and cannot produce a passing result.

The receipt-without-final-acknowledgment control intentionally has no final
host trace in its receipt. Further dispatch remains disabled. Fixture/read
observations are retained; the runner does not retry to fetch the missing trace.
Default-handler exit can prevent a final fixture observation, which is recorded
as an expected limit.

## Containment and review

Each native trial uses private desktops that are never displayed, owned
agent/fixture sessions, verified callbacks and a neighboring pending read.
There are no foreground or global input operations, queue attachments, private
offsets or host patches. The hooks use bounded storage without blocking IPC
or file I/O inside key tracing. Console hosts are never terminated.

The [job and descendant-cleanup plan](PROCESS_LIFETIME.md) remains open.
An inherited job can prevent shell-job assignment on Windows 7. Keep those
failures for separate diagnosis.

The report identifies both builds and the exact package. Each trial keeps
stdout/stderr, raw native records and a SHA-256 inventory. The
[development auditor](../tools/research/Test-HostCancellationDiagnosticEvidence.ps1)
reviews a native trace or a combined result directory. It requires PowerShell
5.1 or later; the portable runner does not.

For a combined review, supply -Directory with the returned run directory,
-PackageRoot with the extracted package, and -OutputPath outside the raw results.
Coverage gaps and original failures remain visible in the review.

The [investigation](validation/2026-11-13-step-1.2-host-cancellation-rc1-investigation.md)
defines the questions this experiment must answer before a fix decision.
H3, H4 and runtime integration remain open.
