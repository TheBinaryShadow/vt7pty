# Console-host cancellation design

Status: H1 and H2 accepted on all three Windows 7 tiers; H3 cancellation is next.
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
The [H2 target review](validation/2026-10-13-step-1.2-host-dispatch-rc1-windows7.md)
accepts nine repetitions covering actual host execution, session binding,
request rejection, input isolation and lifetime/failure controls.
H3 remains unimplemented. No production hook mechanism is selected.

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
H1 selects no thread and sends no host message.

A/B coexist with distinct pending cooked reads. Complete and close A, create
and complete C while B remains pending, then verify B returns BRAVO_KEEP intact.
Repeat identity observations, retain process handles across shutdown, and wait
for observed hosts to exit. Never terminate a host. Emergency termination is
limited to created agent/fixture processes and makes cleanup fail.
Enter here tests ordinary-read isolation, never successful cancellation.

This is a controlled consistency experiment, not authentication against a
malicious application. Class 49 is an internal query/set interface. Production
must retain agent ownership and validate a host endpoint independently.

## H2 implementation and acceptance

The test-only [H2 probe](HOST_DISPATCH_PROBE.md) implements the following
contract. The [rc1 candidate review](validation/2026-10-13-step-1.2-host-dispatch-rc1-candidate.md)
records three exact-package repetitions establishing the callback on WOLF.
The [Windows 7 review](validation/2026-10-13-step-1.2-host-dispatch-rc1-windows7.md)
accepts three more repetitions each on NESSY, TURTLE and LEOPARD.
H3 cancellation is not implemented.

### Discovery and loading boundary

Repeat a title change through the controlled fixture. Observe only
EVENT_OBJECT_NAMECHANGE for the verified host and its owned agent, never all
desktop processes. Record the exact console HWND, object/child IDs, event thread,
callback thread and independently queried thread owner. Both changes must
identify the same live host-owned thread; retain its query/synchronization handle.
WinEvent delivery itself occurs in the controller and is not proof of in-host
execution. Do not choose the first GUI thread or the externally reported HWND
owner.

Load the colocated x64 HostDispatchHook.dll by absolute path after the portable
runner has verified its package hash. Install WH_CALLWNDPROC only on the nonzero
discovered thread. Send a bounded WM_NULL to the owned console HWND. The callback
must report the exact host PID, creation time, discovered thread and HWND, and
its private-pipe client PID must independently match the host.

On WOLF and all 54 Windows 7 target host lifetimes, GetWindowThreadProcessId
inside the callback reports the actual host/thread; externally it reports the
agent. Require and record the in-host match. A differing future observation
fails the gate and requires investigation, never arbitrary thread selection.
The prototype requires native x64,
matching user/logon/session, medium integrity and WinSta0/Default. No elevation,
foreground/focus change, global hook, persistent installation or system patch.

### Session binding and protocol

The controller creates a first-instance, local-only user/SYSTEM pipe with a
random 128-bit channel nonce. The atom passed with WM_NULL names this channel;
it is not secret authentication. The callback opens only the exact local
research pipe pattern, never waits for a server, and sets PIPE_NOWAIT.
No callback waits for a controller response or pipe buffer space.

Both peers verify the OS-reported pipe peer PID. The callback checks the
controller image beside its own module, retained creation identity and matching
token. Binding verifies the controller -> agent -> fixture parent chain,
retained client creation times, common host query, actual executing host/thread
and console HWND. A fixed 112-byte request carries schema, operation, nonce,
sequence, bounded deadline and all session identities; the 88-byte response
reports actual callback context and the accepted sequence. Rejections preserve
the last accepted sequence. The callback cannot rebind after retirement.

This authenticates a controlled endpoint and binds sessions; it is not a
security boundary against code already compromised under the same Windows user.
Malformed pipe framing closes the channel. Supported operations are Bind, Ping,
Close and an explicit test-only receipt-without-final-acknowledgment control.
No operation cancels a read, synthesizes input or delivers a control event.

### Lifetime, failure and acceptance

States follow Unavailable -> Identified -> Ready -> InFlight -> Completed,
with Rejected, OutcomeUnknown and Closing/retired outcomes. A missing final
acknowledgment disables further dispatch; never retry it or issue a fallback
control event. Record receipt separately from final acknowledgment and read
completion. Input remains pending throughout these harmless exchanges.

The callback has no worker thread and no asynchronous I/O buffers. Completed
SendMessageTimeout calls establish return from the synchronous callback.
Close retires the channel and closes its handles; controller cleanup removes
the hook and releases only its own module reference. User32 manages the remote
hook module. A timeout does not prove the callback finished. In that case stop
dispatch, remove the hook and close owned sessions; retain observed host handles
until exit. UnhookWindowsHookEx alone is not used as a callback-drain guarantee.

The probe exercises duplicate/forged/stale/cross-session requests, successful
requests after rejection, replacement sessions, acknowledgment loss, close with
pending input, host exit and abrupt controller death. All owned clients and
observed hosts must exit, and B's original line must survive every neighboring
session. No host termination is permitted. Access failures or unfamiliar
representations fail closed and retain evidence. Broader denial/race/load
qualification remains required before production in H4.

The exact package passes all three designated Windows 7 tiers. Independent
review checks 54 actual callback contexts, 225 exchanges and 45 preserved reads.
Windows 7 yields 66-70 title events per discovery, versus two on WOLF; all
3,647 target events identify the expected host thread and stay within the
128-event buffer without overflow. No one-event-per-change assumption is made.
Broader event volume and load remain H4 qualification work.

H2 is accepted on those configurations. Proceed next to an H3 cancellation
prototype; H2 does not prove read cancellation or qualify production use.


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
- [Microsoft: SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook): scoped discovery; console events can be delivered out of context.
- [Microsoft: SendMessageTimeout](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw): bounded synchronous notification.
- [Microsoft: UnhookWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex): removal may return while a callback is still executing.
- [Microsoft: named-pipe wait modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes): PIPE_NOWAIT read/write behavior.
- [Microsoft: SetWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw): possible thread-scoped research mechanism and architecture constraints.
- VT7 reference: src/interactivity/win32/windowproc.cpp, Window::_HandleGetObject
  explains console HWND reassignment. VT7 remains reference-only.

See [probe instructions](HOST_IDENTITY_PROBE.md).
