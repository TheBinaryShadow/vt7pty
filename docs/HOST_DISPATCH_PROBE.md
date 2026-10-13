# Host dispatch research probe

I use H2 to verify execution inside a session's console host and its
request/acknowledgment channel. The runtime is the unchanged released 0.5.0
pair. This probe never requests Ctrl+C or cancels an application read.

The [rc1 candidate review](validation/2026-10-13-step-1.2-host-dispatch-rc1-candidate.md)
identifies the issued ZIP and three passing exact-package development repetitions.
The [Windows 7 review](validation/2026-10-13-step-1.2-host-dispatch-rc1-windows7.md)
accepts H2 on NESSY, TURTLE and LEOPARD, with three passing repetitions each.
It preserves 519 files and checks 54 host lifetimes, 225 exchanges and 45 reads.
Windows 7 emits 66-70 title events per discovery; all identify the expected
host thread with no buffer overflow. The separate [H3 prototype](HOST_CANCELLATION_PROBE.md)
now tests cancellation under the
[staged design](HOST_CANCELLATION_DESIGN.md).

## Run on Windows 7

Extract the entire research ZIP into a fresh local directory. Keep its files
together and run as the ordinary test user on the interactive default desktop:

- NESSY: RUN-HOST-DISPATCH-NONESU.cmd
- TURTLE: RUN-HOST-DISPATCH-ESU.cmd
- LEOPARD: RUN-HOST-DISPATCH-LEGACY.cmd

PowerShell 7 and KB3191566 are unnecessary. The runner supports installed
PowerShell 2.0 and 5.1. It verifies package hashes and the pinned release
binaries, records machine/OS/update/engine identity, and repeats the probe
three times. Each repetition normally takes about ten seconds, with an outer
90-second deadline. Return the complete generated results directory.

Keep failures. Do not rerun elevated to bypass a discovery or access failure.
A missing or ambiguous title event, denied hook installation or different
in-host window identity fails the experiment for that configuration.

## What runs

The package includes HostDispatchProbe.exe and the x64 HostDispatchHook.dll.
The DLL is loaded only through a thread-scoped WH_CALLWNDPROC hook after H1
identity checks and repeated title-event discovery on an owned hidden console.
No arbitrary process/thread/window can be supplied on the command line.

The callback handles only the probe's WM_NULL notifications. It performs a
bounded private-pipe exchange without waiting for connection, input or output
buffer space. It reports its actual process/thread, host creation time and
window identity. The controller independently verifies the pipe's client PID.
The callback verifies its server PID, controller image beside the exact hook
DLL, token/logon/integrity, owned descendants and their console-host association.

Every session uses a random 128-bit nonce and increasing request sequence.
The nonce binds requests to the established channel; the pipe peer and owned
process identities establish the endpoint. An atom carries the channel name;
atom secrecy is not an authentication boundary. This controlled same-user
experiment does not protect against a compromised process under that user.

## Cases and evidence

Each repetition verifies six owned host lifetimes:

- A/B hold separate pending cooked reads. A rejects duplicate, wrong-nonce,
  cross-session, stale-host, expired, schema/size, sequence-gap and rebind requests.
- A retires its channel and exits while B remains pending. Replacement C rejects
  A's old nonce and accepts its own request.
- D explicitly sends a receipt while suppressing final acknowledgment. The
  controller times out, disables dispatch and refuses a retry or fallback.
- E exits while its callback is installed; a later request is refused before
  writing to the pipe.
- A separate controller creates F, binds its channel and exits without C++
  cleanup. The parent retains and verifies controller/client/host process
  handles and requires their exit while B retains its input.
- B completes its original line intact after all neighboring sessions close.
  A through E return exactly their expected text plus CRLF through ordinary Enter.

A passing repetition contains 14 accepted requests, ten rejected requests and
one receipt-only outcome. The 13-case protocol oracle also passes. JSON records
retain discovery events, H1 observations, callback greetings, every request and
response, ambiguity/exit controls, five completed reads and the aggregate.
Five raw output streams accompany those records; F exits with its read pending.

The callback has no worker thread and retains no caller-owned buffer across a
return. Successful synchronous notification returns establish completion of
that callback invocation. Close retires its channel; controller teardown removes
the hook. Unhooking alone is not treated as a drain proof: retained host handles
must signal before the repetition passes. On failure, only created controllers
or verified owned clients may be terminated. The host is never terminated.

## Developer use

Build-VT7Pty.ps1 builds the executable and callback DLL. Verify-VT7Pty.ps1 checks
their imports, manifests, resources and the protocol oracle. The current suite
has 32 local checks
without the optional soak. Historical release package counts are unchanged.
H1 and H2 share HostResearch.h; the existing H1 workflow remains separately tested.

Package-HostDispatchProbe.ps1 requires clean source and produces an immutable
candidate containing the unchanged runtime. Run its portable
tools/research/Invoke-HostDispatchProbe.ps1 with -Tier DevelopmentHost to compare
the exact package locally. The native probe accepts an existing absolute output
directory or --self-test. Fixture/abandonment modes are internal owned-child paths.

This gate does not rerun the shell fidelity corpus or establish cancellation.
H3 must discard canceled input and preserve subsequent reads and real prompts;
H4 then qualifies a production implementation.
