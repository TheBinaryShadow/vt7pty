# Lifecycle and resource tests

I use lifecycle rc4 to trace the slow resize calls seen on NESSY and
LEOPARD. Released 0.5.0 and the exact screen rc3 runtime are the controls.
All trials use the same fixture and session flags 0.

## Run it on Windows 7

Extract the entire ZIP to a local directory such as `C:\VT7Lifecycle`.
Start the launcher for your machine from an unlocked desktop. The suite
sends its own input. Keep the desktop unlocked until collection finishes.

| Machine | Launcher |
| --- | --- |
| NESSY | `RUN-LIFECYCLE-NONESU.cmd` |
| TURTLE | `RUN-LIFECYCLE-ESU.cmd` |
| LEOPARD | `RUN-LIFECYCLE-LEGACY.cmd` |

LEOPARD uses installed PowerShell 2.0 without KB3191566. Copy the complete
new `results\<run-id>` directory to `K:\VT7_work\results`, including any
failures. I expect 22 case observations, 10 session contract checks and
four files named `resize-trace.json`. The idle and active windows take
at least seven minutes per machine before the contract checks and setup.

## What I measure

- **Drain and exit.** CMD and native Windows PowerShell each write 200
  numbered rows and a final marker, then exit with status 37. I start
  reading after observing child exit, check the ordered output through
  EOF, and check that the agent exits. The missing-marker control must
  be detected once per runtime.
- **Close and recovery.** I verify the native error and handle result for
  a missing executable, then launch a working command in the same
  session. I close an active session beside a second live session and
  verify the neighbor still renders and drains. The released runtime uses
  free. The accepted runtime also exercises idempotent close and a
  rejected resize after retirement.
- **Response.** I run four paired trials, alternating which runtime goes
  first. Each trial records 16 input receipts, backend resize RPCs, reader
  resizes and repaint barriers with a monotonic clock. The barrier waits
  for a new marker in the decoded stream after a native snapshot.
- **Resize trace.** After each control pair I repeat the timing workload
  with a diagnostic copy of screen rc3. Only its agent is rebuilt with
  tracing. I record the caller's 16 RPC intervals and the agent's dispatch,
  resize and reply timing on the same clock. Within the agent I time
  selection, font, buffer size, window placement and size-event work.
  I also record input and scrape work that can delay dispatch. Trace
  records are collected in memory during the timing window and written
  after the agent exits. I use the controls to check whether a slow tail
  also occurs without tracing.
  [Windows performance counter](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
- **Resources.** I measure 15 seconds of idle time and at least 20 seconds
  of repeated input, resize and repaint work per trial. The active window
  starts after the latency measurements. I record caller, agent and
  fixture CPU cycles, CPU time, private bytes, working set and handle
  counts at both ends of each window. I preserve the completed operation
  count and the final active snapshot.
  [Windows process cycle counter](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryprocesscycletime) ·
  [Windows process memory counters](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex)
- **Existing gates.** I run the 500-session handle check, 100-resize load,
  repeated shutdown, failed-spawn and concurrent-session checks against
  both packaged runtimes. I run the full soak during milestone qualification.

I use the repeated Windows 7 baseline samples to set latency and resource
budgets. I compare idle cycle deltas and active cycles per completed
operation between runtimes on the same machine. I keep CPU time as a
separate measurement. Shutdown has a 10-second limit. The close and
measurement cases record enclosing-job membership.
[Process lifetime plan](PROCESS_LIFETIME.md) ·
[Rc1 Windows 7 measurements](validation/2026-11-25-step-1.5-lifecycle-rc1-windows7.md#timing-and-resources) ·
[Rc2 Windows 7 measurements](validation/2026-11-26-step-1.5-lifecycle-rc2-windows7.md#resize-timing) ·
[Rc3 Windows 7 measurements](validation/2026-11-27-step-1.5-lifecycle-rc3-windows7.md#cycle-measurements)

## Build and review

From a clean, committed checkout, I build and package with
`Package-OutputFidelity.ps1 -Suite Lifecycle -TraceResize`, using the pinned 0.5.0
runtime and screen rc3 archives. I review the returned run with
`tools/output/Test-LifecycleEvidence.ps1 -Directory <run-directory> -PackageRoot <extracted-zip>`.
The reviewer checks the package and evidence hashes, run order, raw drain
sequence, native measurements and regression receipts. It checks that
cycle totals stay monotonic for each process. For rc4 it checks the
diagnostic agent identity and matches every trace interval to its resize
sample. I can review archived rc1 through rc3 results with the same script.
