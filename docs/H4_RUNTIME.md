# H4 runtime candidate

I am integrating the qualified H3 cancellation mechanism into the real runtime.
This is the first opt-in H4 test candidate. H4, Milestone 1 and VT7 integration
remain open. The published runtime is still 0.5.0.

## What changes

The development identity is 0.5.1-dev, public API 1.1 and client-agent protocol 2.
The DLL and agent must be a matched pair.

VT7PTY_FLAG_HOST_CANCELLATION enables the new path. It requires the colocated
VT7Pty-Host.dll and an unimpersonated, medium-integrity interactive caller on the
current input desktop. Without the flag, the existing input route remains.

Each flagged session has a private backing desktop for its agent and console.
The application and GUI-capable descendants use the caller's ordinary desktop.
I do not switch the user's desktop or change foreground focus. A backing-window
query from the application may return no window thread; applications that
manipulate their console HWND need explicit compatibility tests.

Processed cooked ETX uses one verified host-thread C dispatch. Raw editor input
keeps the public input-record route. The agent serializes dispatch with scraping,
resize and retirement. An ambiguous result disables dispatch and closes the
session; it never triggers a retry or fallback Ctrl+C.

The host module is pinned until its owned conhost exits. A dedicated, windowless
cleanup thread on the private desktop waits for agent exit, caller exit or normal
close, then posts one WM_CLOSE. It does not cancel reads or terminate a host.
Normal close accepts a queued request, not proof that every client has exited.
Existing enclosing-job membership is preserved; broader job and descendant
policy remains in the [process lifetime plan](PROCESS_LIFETIME.md).

API 1.1 adds vt7pty_close. It retires a session under the control-RPC mutex and
leaves the allocation, borrowed agent handle and immutable pipe names valid.
Later RPCs fail with LOST_CONNECTION. Callers stop and join all users before
vt7pty_free; callers also close their independently opened pipe handles.
Close does not promise output draining or permit concurrent free.

The [contract record](H4_RUNTIME_PROPOSAL.md) preserves the design decisions and
initial experiments, including the agent-death failure that led to the monitor.

## Run on Windows 7

Extract the entire H4 ZIP to a new local directory. Keep its directory structure.
Close any development VT7Pty debug collector before starting; the runtime suite
starts its own collector and refuses to use another collector's channel.

Run the launcher for the machine:

| Machine | Launcher |
| --- | --- |
| NESSY, non-ESU | RUN-H4-NONESU.cmd |
| TURTLE, ESU | RUN-H4-ESU.cmd |
| LEOPARD, PowerShell 2.0 without KB3191566 | RUN-H4-LEGACY.cmd |

NESSY and TURTLE require PowerShell 7.2.24 x64 with its bundled PSReadLine module,
normally at C:\Program Files\PowerShell\7\pwsh.exe. Supply a different path as
-PowerShell7Path "X:\path\pwsh.exe". LEOPARD records these eight cases as
NotApplicable and still runs installed Windows PowerShell 2.0.

The runner uses the installed x64 Windows PowerShell. It needs no build tools,
newer PowerShell, .NET upgrade or KB3191566 on LEOPARD.

There are three interleaved groups. Each runs:

1. The original H3 rc1 controls, unchanged, with their failures retained.
2. The accepted H3 guard reference, unchanged, for comparison.
3. The new runtime suite through VT7Pty.dll, the ordinary application desktop and
   the real input pipe: 41 cases on NESSY/TURTLE, 33 on LEOPARD.

The runtime cases cover cooked/raw routing, repeated cancellation, exact next
input, Enter races, concurrent RPC/input/close, agent and caller death, an enclosing
job, neighbor isolation, session recreation, impersonation denial, missing/wrong
host modules, shell prompts and active children. Native oracle checks also run
during source verification. Trace review checks all 256 keyboard-state bytes.

The final phase runs the existing 30-case regression acceptance suite, including
its full 120-minute soak, against the matched development runtime. That regression
suite uses the existing input route. It does not establish sustained operation
of the new opt-in path. Allow more than two hours for the complete collection.

Keep the entire new results directory. The runner reports collection separately
from test outcome and never promotes the original control failures to passes.
Return all three machines' results, including partial or failed runs.

## Development and review

Build and verify with the normal [local commands](../BUILDING.md).
RuntimeCancellationTest --self-test runs the protocol and keyboard oracles.
For the integrated suite, set VT7PTY_H3_PWSH to the pinned engine path, then run
RuntimeCancellationTest --run NEW_DIRECTORY. VT7PTY_H4_LEGACY=1 selects the
33-case inventory. These variables configure the harness, not the product.

Package from clean committed source with Package-H4Runtime.ps1. It includes the
exact accepted H3 ZIP's files and a matched release-test archive. Original
reference files are verified and never rebuilt.

Use tools/research/Test-H4RuntimeEvidence.ps1 to review a native directory, or
supply -PackageRoot for an entire returned collection. The review checks file
hashes, case inventories, retained identities, desktop placement, keyboard tables,
reads, shell recovery, child status and original outcomes.

## What still needs qualification

I will review the Windows 7 results before accepting this path. US/Croatian
keyboard workflows, sustained opt-in workloads, desktop resource limits under
larger concurrent workloads, lock/session transitions and broader access/job
policies remain open. The current runtime has no test-only fault or acknowledgment
suppression switches. H3 retains its fault controls; forced production callback
timeout and rundown qualification still need their own concrete experiment.

Passing this candidate does not complete the broader fidelity corpus, freeze the
Milestone 2 API or authorize VT7 changes.
