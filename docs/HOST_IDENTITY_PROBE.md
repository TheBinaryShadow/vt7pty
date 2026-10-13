# Host identity research probe

I use H1 to identify each session's console host and check that ordinary reads
stay isolated. It uses the unchanged released 0.5.0 binaries, with no hook DLL.
A pass establishes neither cancellation nor the actual dispatch thread.

The [rc1 candidate record](validation/2026-10-13-step-1.2-host-identity-rc1-candidate.md)
identifies the issued ZIP and passing development evidence. The
[Windows 7 review](validation/2026-10-13-step-1.2-host-identity-rc1-windows7.md)
accepts H1 on all three designated tiers, with three successful repetitions
per machine, including PowerShell 2.0 on LEOPARD. The separate
[H2 dispatch prototype](HOST_DISPATCH_PROBE.md) also has an accepted
[three-tier Windows 7 review](validation/2026-10-13-step-1.2-host-dispatch-rc1-windows7.md).
The separate [H3 prototype](HOST_CANCELLATION_PROBE.md) now tests cooked-read cancellation; target evidence remains pending.

## Run on Windows 7

Extract the whole ZIP into a fresh local directory, keeping all files together.
Run as the ordinary test user, without elevation:

- NESSY: RUN-HOST-IDENTITY-NONESU.cmd
- TURTLE: RUN-HOST-IDENTITY-ESU.cmd
- LEOPARD: RUN-HOST-IDENTITY-LEGACY.cmd

PowerShell 7 is unnecessary. Installed PowerShell 2.0/5.1 is supported and
KB3191566 is not required. Each run repeats the probe three times, normally
taking seconds; the outer deadline is 90 seconds per repetition.
Return the entire generated results directory, including failures.

The runner verifies packaged file hashes and pinned release binaries, then
records machine/OS/update/PowerShell/CLR identity, manifest/source identity,
exit codes, timings, stdout/stderr and native observations.
Only controlled fixture text is collected.

Each repetition creates A/B sessions, checks host identity before/during reads,
closes A, creates/closes C, and verifies B's original typed input survives.
It saves six identity observations, three read results, a closed-client record,
three streams and probe.json. An oracle checks ten query representations.

Hosts are opened only for query/synchronization and threads only for query.
GUI-capable threads are recorded, never selected. Both DispatchThreadVerified
and CancellationImplemented remain false. Unsupported queries or identity
failures must be retained; do not rerun elevated to bypass them.

## Developer use

Build-VT7Pty.ps1 includes HostIdentityProbe in the solution. The verifier
inspects its imports and runs its oracle. For a native run, create an output
directory first and pass its absolute path to HostIdentityProbe.exe.

Package-HostIdentityProbe.ps1 requires clean source and creates an immutable,
versioned research ZIP. Run the portable tools/research/Invoke-HostIdentityProbe.ps1
with -Tier DevelopmentHost and -PackageRoot pointing to the extracted package.

See [the staged cancellation design](HOST_CANCELLATION_DESIGN.md).
