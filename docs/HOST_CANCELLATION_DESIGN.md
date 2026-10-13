# Console-host cancellation design

Status: H1 accepted on all three designated Windows 7 tiers; H2 dispatch next.
Updated: 2026-10-13.

This design follows the [cancellation investigation](INPUT_CANCELLATION.md).
The goal is correct line cancellation while preserving active-child and raw-input
policies. The production runtime remains 0.5.0.

## Decisions and gates

| Gate | Deliverable | Acceptance |
| --- | --- | --- |
| H1 | Query-only identity/isolation probe | Consistent agent/fixture host associations, validated host identity, distinct simultaneous hosts, surviving input preserved, cleanup |
| H2 | Test-only host callback and request/acknowledgment | Actual dispatch context verified; stale, duplicate and cross-session requests rejected; no cancellation yet |
| H3 | Cooked-read cancellation prototype | Old text discarded, appropriate read completion, one event, intact next read and passing real shell prompts |
| H4 | Production integration and candidate | Input/child/PS7 guards, concurrency, failure, cleanup and Windows 7 regression qualification pass |

The [H1 target review](validation/2026-10-13-step-1.2-host-identity-rc1-windows7.md)
accepts three repetitions each on NESSY, TURTLE and LEOPARD: 54 consistent
host observations, 27 ordinary reads, session isolation and cleanup. This
completes the prerequisite for H2 on those configurations. A failed later gate
blocks dependent work for that configuration; retain the evidence.
H2/H3 remain unimplemented hypotheses. No production hook mechanism is selected.

## H1 implementation

HostIdentityProbe.exe creates its own VT7Pty agent/fixture sessions. It accepts
no arbitrary target PID. Private pipes use the existing user/SYSTEM ACL,
remote-client rejection, first-instance protection and peer PID checks. The
fixture image and its agent parent are checked against retained handles.

The fixture dynamically resolves NtQueryInformationProcess from the already
loaded ntdll.dll and queries class 49, ProcessConsoleHostProcess. The controller
also queries its owned fixture and agent. All three raw values must agree.
Preserve NTSTATUS, returned length, raw value, low two bits and normalized PID.
Require successful status, eight-byte result and a nonzero DWORD-sized PID
after clearing the low two bits; reject other representations.

The first local experiment rejected bit 0 in the returned host value.
[libuv 1.44.2's Windows terminal source](https://github.com/libuv/libuv/blob/v1.44.2/src/win/tty.c)
also clears these two bits before using this query's result as a host PID.
The revised oracle checks all four flag combinations, failure status, wrong
length, zero, overflow, flags without a PID and different hosts. This references
a representation rule; it imports no libuv source or dependency.

Open the candidate host only for query/synchronization. Require a live retained
handle, exact System32 conhost.exe image path, native x64 architecture, matching
user SID/authentication LUID/integrity/Windows session across all participants,
and host creation no later than fixture creation. Recheck associations.
The controlled console must contain exactly the expected agent and fixture.

Record the HWND's reported PID/thread as observations only. Enumerate threads
belonging to the verified host and record GetGUIThreadInfo results. A GUI-capable
thread is not proven to be the dispatch thread: all 54 Windows 7 observations
expose two such threads, and the development host also exposes multiple threads.
No thread is selected and no host message is sent.

A/B coexist with distinct pending cooked reads. Complete and close A, create
and complete C while B remains pending, then verify B returns BRAVO_KEEP intact.
Repeat identity observations, retain process handles across shutdown, and wait
for observed hosts to exit. Never terminate a host. Emergency termination is
limited to created agent/fixture processes and makes cleanup fail.
Enter here tests ordinary-read isolation, never successful cancellation.

This is a controlled consistency experiment, not authentication against a
malicious application. Class 49 is an internal query/set interface. Production
must retain agent ownership and validate a host endpoint independently.

## H2 contract, not implemented

Discover the actual dispatch context through evidence inside the verified host.
Do not choose the first thread, the HWND's reported thread or every GUI thread.
Start with a callback reporting actual PID/thread and a challenge response.
Any experimental thread hook must use a nonzero, explicitly verified target.
No global hook or input to the user's foreground window is allowed.

Specify the x64 module's absolute path, exact binary/source identity, loading
boundary, desktop/user policy and bounded lifetime before loading it.
No persistent installation or system-binary modification. H1 includes no DLL.

Use a restricted, fixed-size/schema request channel with an unpredictable
session nonce, controller/host creation identities, console identity and
increasing sequence number. Independently check actual execution context and
peer ownership. Reject stale, forged, duplicate and cross-session requests.
The host callback cannot block waiting for the controller or pipe connection.

States: Unavailable -> Identified -> Ready -> InFlight -> Completed, plus
Rejected, OutcomeUnknown and Closing. Record receipt, acknowledgment and
observed read outcome separately. Never retry a timed-out request or send a
fallback control event blindly: the first operation may already have happened.
Disable experimental dispatch until ambiguous state is reconciled.

Closing stops new requests, retires the session generation, drains callbacks
and releases resources. Unhooking alone does not prove no callback is executing.
Define handle/mapping/module ownership and host/agent/controller death behavior.

## H3 contract, not implemented

After H2, evaluate the host's normal input-processing path. Define cancellation
and event delivery as one operation; first measure whether the host already
delivers the event. Do not unconditionally append GenerateConsoleCtrlEvent.

No private function offsets, byte patches, arbitrary-thread cancellation,
global keyboard changes, focus changes, synthetic Enter or termination as Ctrl+C.
Temporary thread-local keyboard state would require proof of queue isolation,
complete restoration and an independently verified failure path.

Establish pending input before requesting cancellation. Record read and handler
outcomes separately. Require no old text, appropriate aborted-read semantics,
one event and an intact second read. Then require fresh CMD/native PowerShell
prompts, no canceled-command marker, a next-command marker and live shells.

Cover repeated requests, empty/partial lines, no pending read, completion races,
active children, default handlers, raw keys, PS7, two sessions, timeout after
receipt, denied access and close during a request. Only then consider H4.

## If a gate fails

Record the exact failure and reject that approach. Do not relax isolation or
ownership checks. The separate application-read-shim alternative must specify
launch/loading, descendant coverage, APIs, encoding, modes, read ownership and
cancellation semantics. Blind cancellation of discovered threads is insufficient.

## References and limits

- [Microsoft: NtQueryInformationProcess](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess): internal and subject to change; resolve dynamically.
- [System Informer phnt](https://github.com/winsiderss/phnt/blob/master/ntpsapi.h): class 49 and pointer-sized representation; reference only.
- [Microsoft: GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo): GUI state observation.
- [Microsoft: SetWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw): possible thread-scoped research mechanism and architecture constraints.
- VT7 reference: src/interactivity/win32/windowproc.cpp, Window::_HandleGetObject
  explains console HWND reassignment. VT7 remains reference-only.

See [probe instructions](HOST_IDENTITY_PROBE.md).
