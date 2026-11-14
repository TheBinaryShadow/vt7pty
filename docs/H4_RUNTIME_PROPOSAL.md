# H4 runtime contract decisions

Recorded: 2026-11-14 (Europe/Zagreb).
Status: desktop isolation and two-phase close approved; H4 qualification open.

I started H4 from accepted H3 commit
5a225364efd61ff19926389b23c50691ae14e403. I have approved opt-in isolation of the
backing console, with ordinary-desktop applications and GUI descendants, and
API 1.1 two-phase vt7pty_close during H4.

This brings a narrow lifetime operation forward from Milestone 2. It does not
freeze the final VT7-facing API. The current contract and test procedure live in
[H4 runtime qualification](H4_RUNTIME.md).

## What the initial experiments established

The development host is WOLF, Windows 10 IoT Enterprise LTSC build 19044.
The first boundary experiment verified a private agent/host with an
ordinary-desktop application and GUI-capable descendant sharing console input.

The application receives a backing-console HWND, but
GetWindowThreadProcessId(GetConsoleWindow()) returns zero from its ordinary
desktop on this host. The test therefore retains process identities and
independently enumerates the exact backing window on its private desktop.
The in-host callback still verifies its actual process, dispatch thread and HWND.
Applications that manipulate their backing console window need separate coverage.

The first integrated harness rejected every fixture because it assumed that
external window-thread observation would be nonzero. I recorded the observations
and corrected the H4-specific assumption. H3's original checks are unchanged.

The first raw-input oracle expected only C and z key-down records. The public
route also supplies a Control-key record. The H4 fixture now retains every raw
record and checks modifier/C/z ordering, no control event and exact next input.

## Why the host needs a lifetime monitor

The first abrupt-agent-death control left the application and host alive.
Normal agent destructor cleanup did not run. I retained the failure and its
source snapshot before adding a monitor.

The initial monitor used two thread-pool waits and pinned the module until host
exit. Further review found a race in its shared close flag: another request
could report success while the first was still attempting to post WM_CLOSE.
Its thread-pool desktop restoration also deserved a simpler lifetime rule.

The candidate uses one dedicated windowless cleanup thread. It assigns only its
own thread to the retained private desktop, waits for caller exit, agent exit or
a normal close event, and posts one WM_CLOSE. It never returns to a shared pool.
The context, monitor handles and pinned code remain valid until the owned host
exits. Accepted close means queued, not completed process cleanup.

There is no product job assignment or blanket breakaway. The candidate tests
preserved enclosing-job membership; the broader
[job and descendant plan](PROCESS_LIFETIME.md) remains required before 1.0.

## Evidence and limits

The [initial development record](validation/results/step-1.2/h4-prototype-2026-11-14/audit/development.json)
and [preservation inventory](validation/results/step-1.2/h4-prototype-2026-11-14/preserved-files.json)
preserve eight passing smoke cases, fifteen original protocol checks, the earlier
agent-death failure, boundary experiment, sources and binary hashes. They record
the uncommitted prototype before these decisions were approved. Their
ContractDecisionsPending field is historical, not the current decision state.

The expanded candidate adds real shell/editor, active-child, race, denial,
startup and lifetime checks. The [runtime guide](H4_RUNTIME.md) records its
scope and remaining qualification. A development pass is not a Windows 7 result.

Microsoft documents separate
[desktop assignment](https://learn.microsoft.com/en-us/windows/win32/winstation/thread-connection-to-a-desktop),
[console inheritance](https://learn.microsoft.com/en-us/windows/console/creation-of-a-console),
[asynchronous close-message posting](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew)
and [module pinning](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw).
The split-desktop arrangement is my design, supported so far by development
experiments. Windows 7 behavior and the remaining workloads still need review.
