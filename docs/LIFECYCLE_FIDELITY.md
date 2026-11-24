# Lifecycle and resource tests

I use lifecycle rc1 for the first paired Step 1.5 runs. It tests the released
0.5.0 runtime and the exact runtime accepted in screen rc3 with the same
fixture and session flags 0.

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
failures. I expect 16 case observations and 10 session contract checks.

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
- **Response and resources.** Each of three trials per runtime records
  16 input receipts, resize RPCs and repaint barriers with a monotonic
  clock. The barrier waits for a new marker in the decoded stream after
  a native snapshot. I record caller, agent and fixture CPU time, private
  bytes, working set and handle counts across the idle and active windows.
  [Windows process memory counters](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex)
- **Existing gates.** I run the 500-session handle check, 100-resize load,
  repeated shutdown, failed-spawn and concurrent-session checks against
  both packaged runtimes. I run the full soak during milestone qualification.

I use the repeated Windows 7 baseline samples to set latency and resource
budgets. Each sample includes harness and fixture overhead. Shutdown has a
10-second limit. The close and measurement cases record enclosing-job
membership. The [process lifetime plan](PROCESS_LIFETIME.md) tracks the
job-policy, abrupt-exit and descendant cases.

## Build and review

From a clean, committed checkout, I build and package with
`Package-OutputFidelity.ps1 -Suite Lifecycle`, using the pinned 0.5.0
runtime and screen rc3 archives. I review the returned run with
`tools/output/Test-LifecycleEvidence.ps1 -Directory <run-directory> -PackageRoot <extracted-zip>`.
The reviewer checks the package and evidence hashes, raw drain sequence,
native measurements and regression receipts.
