# Coordinated cancellation research

I've extended the [verified scraper coordination](validation/2026-11-13-step-1.2-host-freeze-diagnostics-rc1-windows7.md)
to the H3 research prototype's native and shell cancellation paths. The
[Windows 7 review](validation/2026-11-13-step-1.2-host-coordination-rc1-windows7.md)
passes all 126 baseline/expiry cases and 54 protected focused phases with no
prototype trace loss. Remaining race, access-denied, active-child and PowerShell 7
guards keep H3 open. The production runtime DLL and public API are unchanged.

## Run on the three Windows 7 machines

Extract **VT7Pty-host-coordination-0.5.0-rc1.zip** into a fresh local folder,
such as C:\\VT7Pty-tests\\coordination-rc1. Keep its directory tree intact and
run as the ordinary test user from the normal desktop.

| Machine | Launcher |
| --- | --- |
| NESSY | RUN-HOST-COORDINATION-NONESU.cmd |
| TURTLE | RUN-HOST-COORDINATION-ESU.cmd |
| LEOPARD | RUN-HOST-COORDINATION-LEGACY.cmd |

Installed x64 Windows PowerShell is sufficient, including PowerShell 2.0 on
LEOPARD without KB3191566. PowerShell 7 is unnecessary for this experiment.
Run once on each machine and keep the entire results directory. Allow several
minutes; do not repeat failed trials just to obtain a green outcome.

## What runs

Each of three groups contains these trials, in this order:

1. Original H3 rc1 control, protocol 1, twelve cases, unchanged.
2. Previous freeze experiment, protocol 3, twenty-two cases, unchanged.
3. Coordinated prototype, protocol 4, twenty-three cases.

That is **nine trials and 171 case records per machine**. Both research builds
rotate their focused condition order across the three groups. The original
first traced package is retained inside the previous experiment's complete
archive. The current comparison executes its version-3 experiment directly.

The new prototype coordinates all thirteen baseline cases that dispatch through
the host, while preserving the existing ordinary raw-input guard and lifecycle
checks. It adds a live expiry control to the existing focused contrasts.
Explicit selected and delay-control leases retain their intended behavior.
Selected challenges can still fail cancellation, and their bracket stays failed
when its middle operation fails. The runner preserves these outcomes and returns
1 for any failed trial. CollectionStatus is separate from Pass/Fail and acceptance.

Limits are 210 seconds for an original control and 480 for either research trial.
The optional development-only neighbor soak is disabled by the portable runner.

## Coordination boundary

The controller obtains an unfrozen lease between complete agent operations,
before entering the host callback. The agent excludes scraping and resizing
until release or bounded expiry. Every mutation records its exact lease, host
request, acquisition, dispatch boundary, completion and release.

A private event, bound to the same random channel name and user/SYSTEM access
policy, marks the end of the callback's keyboard scope. The callback only signals
it; no wait, agent IPC or file I/O is added there. The controller waits outside
the callback and resumes work after that marker. A marker confirms scope
completion, not event delivery or a final protocol acknowledgment. The receipt-only
control still withholds its final acknowledgment, disables further dispatch and
refuses retries.

[SendMessageTimeout](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
provides bounded dispatch, but [ReplyMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-replymessage)
can let a sender continue before receiver processing ends. The explicit marker
keeps coordination tied to our keyboard scope. Receiver/window exit is also
reported as a dispatch failure. No extra C or fallback control event is sent.

## Expiry and collection checks

The new coordination-expiry case requests a one-second lease, queues a resize,
then lets the lease expire before trying cancellation. It must refuse before
writing a host request, disable retries, preserve the pending read and original
text, apply the deferred resize, restore dimensions and clean up. A stale lease
is never silently renewed to make that request pass.

The neighboring controller drains host and agent recorders while waiting for
parent commands, using a nominal 250 ms interval. Storage remains 4,096 events
per ring, paged in chunks of 32. Actual drain counts and gaps are recorded;
overwritten records remain explicit failures of coverage.

The development reviewer is [Test-HostCoordinationEvidence.ps1](../tools/research/Test-HostCoordinationEvidence.ps1).
It requires PowerShell 5.1 or later and writes outside raw results. It checks
both original outcomes and each permit, completion boundary, expiry refusal,
resumption and trace coverage. The portable target runner supports PowerShell 2.0.

## Identity and remaining work

The original control and prior experiment keep every issued file byte for byte.
Their manifests are independently pinned in addition to the outer file inventory.
The coordinated probe, hook and agent have distinct identities. Its agent is
packaged as VT7Pty-Agent.exe for the accepted DLL's existing resolver; its original
filename remains VT7Pty-CoordinatedAgent.exe. Keep each bin directory intact.

The public runtime API/protocol and release version remain unchanged. Private
desktops, retained process identities, owned cleanup, full modifier restoration
and original cancellation deadlines remain required. A fresh prompt does not
prove a pending ReadConsole. True completion/close races, actual access denial,
active-child and PowerShell 7 qualification remain before production integration.
