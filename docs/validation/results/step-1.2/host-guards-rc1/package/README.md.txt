# H3 guard qualification

I've added the remaining research guards to the coordinated cancellation
prototype. This package tests the request boundary, controlled native children
and the pinned PowerShell 7 editor configurations. Windows 7 qualification is
pending. H3 and production integration remain open.

## Run the candidate

Extract **VT7Pty-host-guards-0.5.0-rc1.zip** into a fresh local folder, such as
C:\VT7Pty-tests\guards-rc1. Keep the complete directory tree. Run as the ordinary
test user from the normal desktop, without elevation.

| Machine | Launcher | PowerShell 7 |
| --- | --- | --- |
| NESSY | RUN-HOST-GUARDS-NONESU.cmd | 7.2.24 x64 required |
| TURTLE | RUN-HOST-GUARDS-ESU.cmd | 7.2.24 x64 required |
| LEOPARD | RUN-HOST-GUARDS-LEGACY.cmd | Eight application cases explicitly NotApplicable |

The default PowerShell 7 path is C:\Program Files\PowerShell\7\pwsh.exe.
A different location can be passed to the launcher with -PowerShell7Path
"C:\path\to\pwsh.exe". The runner checks the actual engine version and records
the executable and bundled PSReadLine manifest hashes. A missing required
application is NotRun and blocks qualification.

Installed x64 Windows PowerShell launches the suite. LEOPARD continues to use
PowerShell 2.0 and CLR 2.0 without KB3191566. Neither Windows Management Framework
5.1 nor PowerShell 7 becomes a native VT7Pty requirement.

Run once on each machine and retain the entire results directory. Do not repeat
failed trials just to obtain a passing outcome. Return all three result folders.

## What runs

Each of three planned groups contains these trials:

| Role | Protocol | Cases per trial |
| --- | ---: | ---: |
| Original H3 rc1 control, unchanged | 1 | 12 |
| Issued coordinated prototype, unchanged | 4 | 23 |
| New guard prototype | 5 | 34 on NESSY/TURTLE, 26 on LEOPARD |

This is nine trials and 207 case records on each designated PowerShell 7
machine, or 183 on LEOPARD. The previous package remains complete, including
its older nested controls. The version-4 reference still runs the rotated
selection/delay contrasts; its deliberate delivery failures remain Fail.

The new experiment runs fourteen baseline/expiry cases, four completion-race
checks, three concurrent-close checks, a real access-denied case, four or eight
active-child cases, and four additional PowerShell 7 prompt cases when required.
Its selection characterization remains in the unchanged reference, rather than
being repeated in the guard suite.

CollectionStatus is separate from native Pass/Fail. The overall runner can
return 1 because an unchanged control or reference fails even when all guard
trials pass. Keep those outcomes and all raw files.

## Completion and close

Completion races release an armed input writer while a single cancellation
request is in flight. Enter-first, simultaneous-start and cancellation-first
schedules record both operations and the fixture's read/handler timeline.
A fourth check requires both completed and canceled reads across those trials.
Concurrent controller requests do not claim simultaneous execution inside Windows.

A completed read must retain its full completed line. A canceled read must
discard its old text with the required event. A recorded input fence consumes
only the planned Enter and its own marker, with unchanged console mode and no
flush. Foreign key-downs or duplicate input fail the check. The next cooked
read must still contain exactly the expected new text.

A second thread requests channel close after publication or after keyboard-scope
completion but before response consumption. New requests stop, while the active
request retains its handles, module reference and lease until completion.
The receipt-only variant still lacks its final acknowledgment and refuses retries.
No wait is added inside the host callback. These are research-channel close
checks; production session destruction and API lifetime policy belong to H4.

## Actual access denial

The controller temporarily impersonates its own user with an additional
restricting SID and attempts the normal retained host-thread open. Windows must
deny it with ERROR_ACCESS_DENIED. The test verifies ordinary access before and
afterward, restores its credentials and preserves the pending input.

The host's security descriptor stays unchanged. No hook or cancellation request
is created on this denied path. This qualifies an actual kernel access-denied
setup path, rather than the existing injected protocol rejection. Cross-integrity
and elevated-host policy still need their production qualification.

Microsoft documents the two access checks for
[restricted tokens](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken)
and the requirements for
[impersonation](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-impersonateloggedonuser).

## Children and editors

The native child opts into Ctrl+C handling explicitly. Its process, parent,
creation time, image and console association are independently retained by the
root controller. Both input routes require exactly one event, the expected
child status, the parent reporting status 37 and a usable next parent command.

The public ETX route uses the established event-only child contract: its
application records the event and exits normally. It does not claim that the
old public route cancels a pending cooked ReadConsole. That limitation remains
in the [baseline](https://github.com/TheBinaryShadow/vt7pty/blob/1054a9a75e25bd04eb08f100f442d05e03c5eaad/docs/FIDELITY_BASELINE.md). The host route additionally requires an
aborted pending cooked read, discarded old text and an intact second child read.

On designated machines, PowerShell 7.2.24 runs with native ConsoleHost editing
and with its bundled PSReadLine module. Native editing uses the coordinated
host path; PSReadLine prompt cancellation keeps ordinary public ETX input.
The latter must preserve raw input behavior without a host cancellation request.
Windows edit mode, disabled prediction and SaveNothing history are local test
settings. Global profiles and console settings are unchanged.

## Evidence and boundary

The guard probe, callback and agent have separate identities. Keep every bin
directory intact. The new agent admits the exact guard controller; it uses
the existing gate protocol 1 and the same scraping/resize exclusion mechanism.
The original protocol-1 control and the complete version-4 package retain their
issued bytes. The accepted 0.5.0 runtime DLL and public API/protocol are unchanged.

[Test-HostGuardEvidence.ps1](https://github.com/TheBinaryShadow/vt7pty/blob/1054a9a75e25bd04eb08f100f442d05e03c5eaad/tools/research/Test-HostGuardEvidence.ps1)
checks package/result identities, every evidence hash, original outcomes,
coordination scopes and the guard-specific records. Developer review requires
PowerShell 5.1 or later; target collection supports PowerShell 2.0.

My [console-host design](HOST_CANCELLATION_DESIGN.md) keeps H3 acceptance separate
from H4 integration. Ordinary-desktop, keyboard-layout, production cleanup and
wider input/regression qualification remain necessary before runtime integration.
