# Compatibility

I target Windows 7 SP1 and later on x64. The exact 0.5.0 release passed
[acceptance on all three Windows 7 tiers](validation/2026-10-02-0.5.0-release-acceptance.md).
A target platform or a successful build is not, by itself, a tested compatibility
claim.

## Platform and runtime

| Area | Policy or measured status |
| --- | --- |
| Minimum system | Windows 7 SP1 x64 |
| Later targets | Windows 8, 8.1, 10 and 11 on x64 |
| Outside scope | XP, Vista and x86 |
| Development evidence | x64 Debug/Release builds and source debugging on Windows 10 build 19044 |
| Release baseline | 0.5.0, public API 1.0, client-agent protocol 1 |
| Runtime dependencies | Static MSVC runtime; no compiler or SDK needed on the target |
| VT7 integration | Planned |
| ConPTY compatibility | Not implemented |

I removed XP/Vista code and obsolete build paths during Milestone 0.
The build uses _WIN32_WINNT=0x0601 and matching subsystem/manifest settings.
Later-Windows APIs need a Windows 7-safe capability path with tested fallback
behavior. Static CRT selection and successful linking do not prove that path works.

Historical WinPTY support statements describe upstream releases. They do not
qualify a VT7Pty build.

## My Windows 7 test machines

I use the test environment shared with VT7 and record the actual OS build,
hardware strings and installed updates with every run.

| Tier | Machine | Configuration |
| --- | --- | --- |
| A: Non-ESU | NESSY | Windows 7 SP1 x64 with the available non-ESU public update set |
| B: ESU | TURTLE | Windows 7 SP1 x64 updated through ESU |
| C: Legacy | LEOPARD | Windows 7 SP1 x64, PowerShell 2.0, no KB3191566 |

Routine candidates run on Tier A when target behavior or dependencies change.
Milestone and release candidates require all three tiers. KB3191566 and
PowerShell 5.1 are not prerequisites of the tested native runtime; Tier C also
runs the portable suite using PowerShell 2.0.

I count NESSY and LEOPARD as full acceptance machines while preserving their
reported VMware inventory. Early testing had two additional evidence exceptions:
for [Step 0.4](validation/2026-09-22-step-0.4-completion.md), I accepted two declared-tier
runs from one recorded installation; for [Step 0.5](validation/2026-09-22-step-0.5-completion.md),
I temporarily treated two runs with matching machine metadata as separate machines.
Those decisions do not turn the inventories into proof of distinct hardware or
update states. The [Step 0.9 review](validation/2026-09-25-step-0.9-completion.md)
records the later three-machine acceptance.

VT7 graphics and managed-runtime prerequisites do not automatically apply to
VT7Pty. Each candidate needs its own loader/runtime evidence.

## Build hosts

My reference build uses VS2022 17.14, MSVC v143 14.44.35207, Windows SDK
10.0.26100.0, C++20 and x64 Debug/Release. I adopt toolchain changes with
reproducible results and Windows 7 validation. See [Building](../BUILDING.md).

## Runtime dependency audit

Every candidate package must be checked for:

- direct imports from system DLLs,
- delay-loaded and dynamically resolved APIs,
- transitive DLL and CRT dependencies,
- API flags, structures, interfaces, and behavior that differ on Windows 7,
- DLL/agent architecture and version agreement,
- subsystem and manifest settings,
- accidental dependence on development-machine paths or tools.

Record both static inspection and execution on the target. Unsupported imports
or unexplained loader failures block acceptance.

## Application compatibility

Compatibility is application- and behavior-specific. Launching successfully is
insufficient. Claims identify the application and version plus tested input,
output, Unicode, resize, cancellation, screen restoration, process lifetime,
and shutdown behavior.

Command Prompt and Windows PowerShell are initial required local applications.
The [Milestone 1 fidelity plan](FIDELITY.md) adds an explicit local corpus and
tier-specific shell requirements, including PowerShell 7.2.24 on designated
tiers. These planned workflows are not qualified by the 0.5.0 suite. Console-SSH
and remote full-screen testing are deferred optional scenarios. See
[Testing](TESTING.md) for execution and evidence requirements.

## ConPTY and other consumers

VT7Pty does not currently expose the ConPTY ABI, emulate
`PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`, or claim compatibility with unmodified
ConPTY clients. A VT7Pty API for VT7 and a general ConPTY user-mode shim are
separate goals.

After VT7 readiness, I may test a user-mode shim against named consumers.
Compatibility must cover process creation, handle ownership,
lifecycle, streams, resize, and errors. No kernel driver is planned.

## Publishing compatibility claims

Use these terms consistently:

- **Target:** intended platform or behavior, not yet accepted.
- **Tested:** passed a named procedure on an identified configuration.
- **Supported:** included in an active release support policy.
- **Limited:** tested with documented constraints.
- **Untested:** no evidence sufficient for a claim.

Update this document, the README, security support table, and release notes
together when compatibility status changes.
