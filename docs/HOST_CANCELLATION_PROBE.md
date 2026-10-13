# Host cancellation research probe

I've built the first H3 prototype to test actual Ctrl+C cancellation through
the console host. Initial local runs discard cooked input, deliver one event,
preserve the next read and restore CMD and native PowerShell prompts.
The [rc1 review](validation/2026-10-13-step-1.2-host-cancellation-rc1-candidate.md)
records three passing exact-package repetitions. Windows 7 results are still required.

This is a research package using the unchanged released 0.5.0 runtime.
The callback and its controller are test programs. I have not selected a
production mechanism or accepted H3.

## Run on Windows 7

Extract the entire **VT7Pty-host-cancellation-0.5.0-rc1.zip** into a fresh local
directory. Keep the files together and run as the ordinary test user:

- NESSY: RUN-HOST-CANCELLATION-NONESU.cmd
- TURTLE: RUN-HOST-CANCELLATION-ESU.cmd
- LEOPARD: RUN-HOST-CANCELLATION-LEGACY.cmd

The runner supports installed PowerShell 2.0 and 5.1. PowerShell 7 and
KB3191566 are unnecessary for this package. It verifies every packaged file,
records the environment, and runs three repetitions. Each normally takes
less than a minute; the outer deadline is 210 seconds per repetition.
Return the complete generated results directory, including failed runs.

Do not rerun elevated to work around a failure. Creating a private desktop,
identifying the host or installing the callback can fail; those outcomes are
part of the evidence.

## What the prototype does

The root controller stays on the normal desktop. It creates two private
WinSta0 desktops with random names and a user/SYSTEM access list. Neither is
displayed. Each worker starts its own released VT7Pty agent and controlled
fixture on its assigned desktop.

H1 identity checks and H2 title-event discovery still precede callback loading.
The callback must identify the actual host, dispatch thread, window and exact
private desktop. The root independently enumerates the console HWND through
its retained desktop handle before acknowledging a session registration.
It also retains and verifies client, host and shell process lifetimes.

A request temporarily sets Ctrl state in the verified host thread's keyboard
table, verifies the exact temporary table, then synchronously sends one WM_KEYDOWN C to that same thread's console
window. A scope restores all 256 original state bytes before responding.
The host handles cancellation and event delivery together; the probe does
not append GenerateConsoleCtrlEvent or a WM_CHAR message.

I use separate desktops to contain this experiment. Windows does not allow
AttachThreadInput across desktops, and the callback rejects the active input
desktop. There are no focus changes, global key injection, queue attachments,
private function offsets or host patches. This proves behavior in the
controlled private-desktop arrangement, not suitability for ordinary-desktop
production use.

## Cases and evidence

Each repetition runs twelve cases while a neighboring session holds BRAVO_KEEP:

| Case | Required observation |
| --- | --- |
| Repeated cooked cancellation | Partial, empty and partial lines discarded; error 995; one event per request; intact next reads |
| Failure and rejection controls | Injected exception restores state without input; simulated access denial, expiry and duplicate cancellation do not add events |
| No pending read | One event; the first subsequent line is intact |
| Completed read | Its completed text survives a later cancellation; the next read works |
| Receipt without acknowledgment | Cancellation is observed independently; the controller records an unknown outcome and refuses retry or fallback |
| Close within a request | Cancellation completes, the channel retires, and the next read works |
| Default handler | The controlled application exits with STATUS_CONTROL_C_EXIT |
| Raw input | The unchanged transport delivers Ctrl+C and z as characters, with the expected modifiers and no control event |
| Fixture death | A job containing only the created test shell closes with its fixture; the shell exits without explicit termination |
| Four shell workflows | CMD and native PowerShell, empty and partial prompts: fresh prompt, absent canceled-command file, correct next-command file, live shell |

The neighboring line must remain pending without events throughout the suite,
then return intact. All hooks must be removed, registered clients and observed
hosts must exit, and both desktop handles must close. Emergency termination is
limited to created workers or independently verified owned clients and makes
cleanup fail. The fixture-death case is an explicitly recorded fault injection.
The host is never assigned to the test-shell job or terminated.

JSON records preserve pending and completed reads, handler counts, request
identities, actual callback context, keyboard snapshots, shell barriers and
lifetime registrations. Raw VT streams accompany them. Boundary Enter is used
to expose retained shell input or recover a failed fixture read; it cannot
turn a failed cancellation into a pass.

The 33-check local oracle deliberately rejects retained text, duplicate events,
normal empty reads, Ctrl+Break, missing delivery and incorrect state restoration.
The receipt-only case intentionally has no final keyboard-state acknowledgment.

## What remains before H3 can close

I still need the three Windows 7 runs and review of the exact package.
This first candidate covers a completed-read boundary, simulated denial and
retirement within the request. It does not yet cover truly concurrent
completion/close races or an actual OS-denied hook installation.

The full handled/default active-child shell matrix and PowerShell 7.2.24 with
PSReadLine also remain required for this mechanism. Private-desktop behavior
does not qualify desktop-changing production architecture or GUI applications.
Those findings must inform the production loading and isolation design before
H4 begins.

See the [design](HOST_CANCELLATION_DESIGN.md), [investigation](INPUT_CANCELLATION.md)
and [validation index](validation/README.md). Development builds include both
HostCancellationProbe.exe and HostCancellationHook.dll. Run the executable with
a fresh, empty absolute output directory, or use --self-test. Packaging requires
clean source through Package-HostCancellationProbe.ps1.

For independent development review, run tools/research/Test-HostCancellationEvidence.ps1
with -Directory pointing at one native run and -OutputPath outside that run.
This auditor requires PowerShell 5.1 or later. It checks all 150 native files,
including callback identities, keyboard tables, read semantics and shell barriers.
