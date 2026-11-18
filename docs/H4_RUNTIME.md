# H4 runtime qualification

I am qualifying the H3 cancellation mechanism in the real runtime. H4,
Milestone 1 and VT7 integration remain open. The published runtime is 0.5.0;
this test runtime is 0.5.1-dev, public API 1.1 and client-agent protocol 2.
The DLL and agent must be a matched pair.

The [completed rc2 review](validation/2026-11-16-step-1.2-h4-runtime-rc2-turtle-rerun.md)
records 345/345 runtime cases across NESSY, TURTLE and LEOPARD. The rc4 candidate
adds the remaining qualification experiments. I preserve rc2 and its results.

The [rc4 local review](validation/2026-11-17-step-1.2-h4-runtime-rc4-candidate.md)
records 123/123 runtime cases, all seven qualification cases and the full
120-minute opt-in soak. The [Windows 7 rc4 review](validation/2026-11-18-step-1.2-h4-runtime-rc4-windows7.md)
now verifies the automated matrix and elevated-caller checks on all three machines.
Keyboard and lock/unlock checks remain open because the manual UI hides typing.

## Runtime contract

VT7PTY_FLAG_HOST_CANCELLATION enables the new path. It requires colocated
VT7Pty-Host.dll and an unimpersonated, medium-integrity interactive caller on the
current input desktop. Without the flag, the existing input route remains.

Each flagged session has a private backing desktop for its agent and console.
The application and GUI-capable descendants use the caller's ordinary desktop.
I do not switch the user's desktop or change foreground focus. A backing-window
query from an application may return no window thread; applications that
manipulate their console HWND still need explicit compatibility tests.

Processed cooked ETX uses one verified host-thread C dispatch. Raw editor input
keeps the public input-record route. Dispatch is serialized with scraping,
resize and retirement. An ambiguous result disables dispatch and closes the
session, with no retry or fallback Ctrl+C.

The host module stays pinned until its owned conhost exits. A windowless cleanup
thread on the private desktop waits for agent exit, caller exit or normal close,
then posts one WM_CLOSE. It never cancels a read or terminates a host. Normal close
accepts a queued request; it does not prove that every client has exited.
Enclosing-job behavior and broader descendant policies are tracked in the
[process lifetime plan](PROCESS_LIFETIME.md).

API 1.1 adds vt7pty_close. It retires the session under the control-RPC mutex and
retains the allocation, borrowed agent handle and immutable pipe names.
Later RPCs fail with LOST_CONNECTION. Before vt7pty_free, callers stop and join
all users and close their independently opened pipes. Close does not promise
output draining or allow concurrent free.

The [contract record](H4_RUNTIME_PROPOSAL.md) preserves the approved design.

## Run on the three Windows 7 machines

Extract the complete rc4 ZIP to a fresh local directory. Close other VT7Pty debug
collectors. Keep the ordinary desktop active for the entire automated run:
disable automatic locking or screensaver activation for this run if needed.
I check desktop eligibility before every trial and soak cycle. An interrupted
collection is retained and fails; a new invocation creates a fresh directory.

| Machine | Automated launcher | Installed shell |
| --- | --- | --- |
| NESSY, NonESU | RUN-H4-NONESU.cmd | Windows PowerShell 5.1 |
| TURTLE, ESU | RUN-H4-ESU.cmd | Windows PowerShell 5.1 |
| LEOPARD, Legacy | RUN-H4-LEGACY.cmd | Windows PowerShell 2.0 / CLR 2 |

NESSY and TURTLE also require PowerShell 7.2.24 x64 and its bundled PSReadLine at
C:\Program Files\PowerShell\7\pwsh.exe. LEOPARD retains its no-KB3191566
configuration; PowerShell 7 cases are not applicable.

Allow **more than four hours** per machine for the automated collection:

1. Three interleaved groups of unchanged original controls, frozen H3 guard
   references and runtime cases. Runtime inventory stays 41 cases per group,
   or 33 on LEOPARD. Original control failures remain visible.
2. Seven focused qualification cases: inactive-desktop startup, eight live
   sessions and resources, two enclosing-job policies, timeout before dispatch,
   timeout during the callback, and agent death during the callback.
3. A **120-minute opt-in soak**, with persistent active and pending-neighbor
   sessions, repeated cancellation and exact next reads, resize, periodic session
   recreation, keyboard snapshots and resource measurements.
4. The existing 30-case regression suite and its separate **120-minute baseline
   soak**, against the matched runtime. This phase uses the existing input route.

Then follow the [manual checks](H4_MANUAL_CHECKS.md): RUN-H4-MANUAL.cmd as the
ordinary user, followed by RUN-H4-ACCESS.cmd with Run as administrator. The
package also includes these instructions as MANUAL-CHECKS.md.

Return all new directories under results from each machine, including partial
or failed runs. Test completion does not automatically accept H4.

## Qualification experiments

Missing/wrong-helper checks now require eligible desktops before and after
startup and the exact agent-side module failure. A desktop refusal cannot stand
in for that evidence.

H4TestGate.dll is a test-only external, thread-scoped hook on the retained owned
console host. It pauses before dispatch or during the real C callback, without
writing keyboard state or adding product fault switches. A pause ends on release,
controller exit or a five-second bound. I verify the real timeout decision,
all 256 restored keyboard bytes, no retry, retired RPCs and natural host exit.
The live timeout monitor starts at a recorded log byte offset before the current
request. It ignores timeouts from earlier uses of an agent PID. An agent can remain
alive until a paused callback unwinds; that observation is
retained without requiring premature process exit.

The opt-in soak records cold-start, post-warmup and final resources. The unchanged
post-warmup budgets are eight caller handles, two USER objects, two GDI objects
and 64 MiB private bytes. Fixture observations retain overlapping windows of the
event timeline; review requires continuous sequence coverage.

The resource case keeps eight hosts and pending clients live together, then
sequences cancellation/resize work across them. This preserves complete evidence
on the bounded diagnostic channel; it does not claim eight simultaneous callback
invocations.

The job cases observe explicit-breakaway-allowed and silent-breakaway policies
from an enclosing native job. They do not introduce product job ownership or
request explicit breakaway. Full descendant-tree containment remains pre-1.0
work in the process lifetime plan.

## Build and independent review

Use the normal [local build commands](../BUILDING.md).
RuntimeCancellationTest --self-test runs 27 runtime checks and four keyboard
bridge checks. --run NEW_DIRECTORY runs the core inventory; --qualify NEW_DIRECTORY
runs the seven focused cases. --soak 120 NEW_DIRECTORY runs the opt-in soak.
VT7PTY_H3_PWSH selects the pinned engine; VT7PTY_H4_LEGACY=1 selects 33 core cases.

Package-H4Runtime.ps1 requires clean committed source and preserves the exact
accepted H3 archive. The test gate is present only in this research candidate,
not the general runtime archive.

Test-H4RuntimeEvidence.ps1 reviews native, combined and manual collections.
Supply -PackageRoot for combined/manual results. The reviewers recompute reads,
identities, desktop placement, failure causes, keyboard bytes, fault timing,
resources and continuous trace coverage, and verify complete file inventories.
Historical rc2 collections remain reviewable.

The automated Windows 7 rc4 matrix and elevated-caller checks pass. Physical
keyboard and lock/unlock evidence remain pending a corrected manual runner.
Passing this candidate does not complete the broader application corpus, freeze the Milestone 2 API
or authorize VT7 changes.
