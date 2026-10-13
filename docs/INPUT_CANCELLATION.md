# Input Cancellation Investigation

Status: Step 1.2 mechanism research in progress; no production cancellation fix.
Updated: 2026-10-13.

The [rc3 Windows 7 baseline](validation/2026-10-12-step-1.1-rc3-windows7-baseline.md)
establishes the defect and the behavior that must survive a fix. CMD and native
Windows PowerShell editing retain a partially typed command after Ctrl+C.
The same command can execute after Enter. Active-child interruption works:
handled Ctrl+C arrives once, default-handler termination preserves its status,
and unprocessed Ctrl+C arrives as a key rather than a control event.
PowerShell 7.2.24 with the pinned PSReadLine configuration passes on tiers A/B.

GenerateConsoleCtrlEvent reaches a control handler but leaves the fixture's
cooked ReadConsoleW call pending. This is measured separately from the real
shells; it does not instrument their internal read calls.

## Required behavior and boundaries

A fix must discard the canceled line, expose a fresh usable prompt, preserve
the next command and leave the shell alive. It must preserve the existing
active-child, unprocessed-input and PowerShell 7 cases. One user action must
not cause duplicate control events. Keep input ordering, EOF drain, bounded
shutdown and cleanup gates.

Work remains user mode in VT7Pty. Do not change VT7, install a driver, alter
global keyboard state, send input to the user's foreground window, cancel
arbitrary application threads, terminate an application to simulate Ctrl+C,
or synthesize Enter as a successful cancellation. These are mechanism
constraints, not new OS prerequisites.

## Controlled public-API probes

Harness version 4 adds two MechanismProbe entries to the unchanged 0.5.0
baseline bundle, giving 39 entries. The 37 prior recipes and seven pending
coverage groups retain their scope. The [runner guide](FIDELITY_BASELINE.md)
defines operation, deadlines and evidence.

| Probe | Operation | Required result |
| --- | --- | --- |
| READCONSOLE_RECORD_EVENT | WriteConsoleInputW with C down/up, character 0x03 and LEFT_CTRL_PRESSED, then one GenerateConsoleCtrlEvent | Exactly one Ctrl+C event and the pending read completes with no typed text |
| READCONSOLE_OWNED_CANCEL | CancelSynchronousIo on the fixture's exact reader thread, then one GenerateConsoleCtrlEvent | Read returns no text with ERROR_OPERATION_ABORTED, exactly one control event, and a second read returns exactly NEXT_PROBE followed by CRLF |

Both establish a pending read through echoed PROBE_TYPED before requesting the
operation. The private reply records API success, its error and record count.
The existing native packet separately records read completion, BOOL result,
last error, text and event count. API success alone is insufficient.

The owned reader cannot proceed to another operation until the harness requests
the second read. The fixture joins it before resetting its storage. The second
read uses a new worker and observes the same input buffer. If either read remains
pending, its observation is preserved before a boundary Enter attempts recovery.
Recovery never changes the failed result to Pass. Completion still requires
final output, fixture exit 37 and owned-process cleanup.

Initial development-host experiments found that records retained PROBE_TYPED
and appended character 0x03. CancelSynchronousIo returned the first call with
BOOL TRUE, error 995 and zero text, but the next read did not complete after
NEXT_PROBE and Enter. Another Enter recovered only CRLF; the new line was lost.
This disqualifies that mechanism on this host despite its initially promising
first-call result. The
[Windows 7 mechanism review](validation/2026-10-13-step-1.2-mechanism-rc1-windows7.md)
now records both probes failing on NESSY, TURTLE and LEOPARD. Record injection
retains the typed line and adds character 0x03. Direct cancellation returns
FALSE with error 1168 (ERROR_NOT_FOUND) while the read stays pending. After
boundary Enter recovery, the next read returns NEXT_PROBE normally. This is
distinct from WOLF's successful first-call cancellation and lost following line.

[Microsoft's cancellation guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)
explains that cancellation depends on the underlying operation and must be
synchronized to avoid canceling a later call. The
[CancelSynchronousIo reference](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio)
does not make console-side recovery a consequence of a successful request.
Availability on Windows 7 is not proof of the required behavior.

## Window-message experiments and disposition

Test-only development experiments combined a console-window message with one
control event. WM_CHAR 0x03 retained the typed line and inserted the character.
Attaching the fixture's input queue and setting Ctrl state before WM_KEYDOWN C
or WM_CHAR 0x03 also failed cancellation. All completed through boundary Enter
with cleanup; these paths were removed from the active source.

A separate thread-hook prototype was rejected before installing a hook: the
owned console HWND reported the agent as its process/thread owner rather than
a System32 conhost.exe target. It therefore provides no evidence that executing
a hook inside conhost would work. Its DLL and project were removed. Identifying
the actual host, isolation, loading policy, failure semantics and keyboard-state
restoration would require a separate design and tests before any runtime use.

The inherited [WinPTY issue 116](https://github.com/rprichard/winpty/issues/116)
records why its window-message workaround was reverted, including dependence
on desktop keyboard state. The
[AttachThreadInput reference](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput)
also documents key-state reset and desktop restrictions. These approaches
remain research findings, not supported product capabilities.

## Next design gate

The [identified version 4 candidate](validation/2026-10-12-step-1.2-mechanism-rc1-candidate.md)
has completed its [three-tier mechanism measurements](validation/2026-10-13-step-1.2-mechanism-rc1-windows7.md).
Both measured public-API approaches are rejected for the pending cooked-read
corpus. Original child guards, PowerShell 7 prompts and target Unicode/resize
outcomes remain unchanged. This completes the research measurements, not F01.

The next recommended design investigates cancellation within one owned console
host: establish its actual identity, a user-mode request/acknowledgment path,
failure behavior, cleanup and session isolation. The earlier HWND/thread-hook
prototype provides no qualified implementation and must not be promoted as a fix.

An application-facing read shim remains a separate alternative. It needs a
specified launch/load boundary and its own console-read semantics; blindly
canceling discovered threads is not a substitute. Choosing or implementing
either mechanism requires a concrete design and new evidence.

A production candidate needs its own version/source identity, actual shell
prompt regressions, active-child and unprocessed-input checks, isolation/failure
coverage, local Debug/Release verification, and applicable Windows 7 evidence.
The unchanged 0.5.0 bundle is research evidence and cannot qualify a runtime fix.
Step 1.1's broader coverage and Step 1.2 remain open.
