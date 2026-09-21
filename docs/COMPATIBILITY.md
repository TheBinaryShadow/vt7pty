# VT7Pty Compatibility Contract

Status: approved target contract; Step 0.4 platform implementation qualified. Updated:
2026-09-22.

This document separates approved targets from verified support. A target is not
a compatibility claim until an identified build passes the required procedure.

## Current status

| Area | Current status |
| --- | --- |
| Source baseline | WinPTY `0.4.4-dev` at upstream commit `7e59fe2` |
| Development host | Maintained x64 Debug/Release workflow verified and source-debugged on Windows 10 build 19044 |
| Windows 7 runtime | Step 0.4 candidate accepted on Windows 7 SP1 x64; recorded evidence deviation applies |
| Architectures | x64 is the approved product architecture; inherited x64 smoke tests passed locally |
| Build interface | VS2022/MSBuild and repository PowerShell entry points; inherited build and packaging paths removed |
| API and binaries | Inherited WinPTY names and behavior remain |
| ConPTY compatibility | Not implemented; ConPTY is a design and behavioral reference |
| VT7 integration | Planned; no integration claim |
| Releases | No VT7Pty release has been published |

Historical WinPTY support statements describe upstream releases and do not
qualify future VT7Pty builds.

## Supported platform policy

The approved initial platform contract is:

- Minimum operating system: Windows 7 SP1 x64.
- Supported family: Windows 7, Windows 8, Windows 8.1, Windows 10, and Windows
  11 on x64.
- Unsupported: Windows XP, Windows Vista, and x86.
- Build API floor: `_WIN32_WINNT=0x0601` and matching subsystem/manifest
  configuration.
- Runtime model: static MSVC runtime initially.

XP/Vista compatibility code and build paths were removed in Roadmap Steps 0.3
and 0.4.
Later-Windows behavior remains in scope. An API newer than Windows 7 may be used
only through a Windows 7-safe capability path with tested fallback behavior.

The target machine does not need the compiler or Windows SDK. Compiler macros,
successful linking, subsystem headers, or static CRT selection do not by
themselves establish runtime compatibility.

## Windows 7 acceptance tiers

VT7Pty uses the same physical environment available for VT7:

| Tier | Configuration | Role |
| --- | --- | --- |
| A | Windows 7 SP1 x64, fully updated with the available non-ESU public update set | Routine target iteration and minimum-floor acceptance |
| B | Windows 7 SP1 x64, fully updated through ESU | Additional milestone and release-candidate acceptance |

Every run records the exact OS build and installed-update state. The initial
acceptance work will determine and document required loader/runtime updates.
VT7 graphics or managed-runtime prerequisites are not automatically VT7Pty
prerequisites.

Routine candidates run on Tier A. Milestone and release candidates run on both
tiers. Results from one tier ordinarily do not stand in for the other. Step
0.4 has a documented owner-approved exception for two declared-tier runs on
one installation; later milestone and release gates retain the normal rule.

## Development and build hosts

The approved reference build environment is Visual Studio 2022 17.14, MSVC
v143 14.44.35207, and Windows SDK 10.0.26100.0. The project uses C++20, x64
Debug/Release configurations, and MSBuild. Newer compatible toolchain revisions
may be adopted through an explicit reproducibility and Windows 7 validation
change.

Builds and checks run through maintained local PowerShell and native tools.
There is no hosted build or test service. Physical target evidence remains the
authority for the Windows 7 floor.

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
The SSH/full-screen application path will be fixed from measured Milestone 0
evidence rather than assumed. See [Testing](TESTING.md).

## ConPTY and other consumers

VT7Pty does not currently expose the ConPTY ABI, emulate
`PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`, or claim compatibility with unmodified
ConPTY clients. A VT7Pty API for VT7 and a general ConPTY user-mode shim are
separate goals.

After VT7 readiness, the project may test a user-mode shim against named
consumers. Compatibility must cover process creation, handle ownership,
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
