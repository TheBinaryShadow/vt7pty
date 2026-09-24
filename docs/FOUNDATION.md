# VT7Pty Development Foundation

Status: approved. Approved: 2026-09-21.

This document records the decisions from which VT7Pty development begins. It
is the authority for Milestone 0 unless a later reviewed change updates this
document and the [roadmap](../ROADMAP.md) together.

## Product direction

VT7Pty is a user-mode console backend derived from WinPTY. Its first purpose is
to improve application-facing fidelity and reliability for future use by VT7
on Windows 7. ConPTY and Microsoft Terminal are behavioral and architectural
references. General compatibility with ConPTY clients is later work. No kernel
driver is planned.

Only the VT7Pty repository is modified by this work. VT7 and Microsoft Terminal
remain reference repositories unless separately authorized.

## Approved technical decisions

| Area | Decision |
| --- | --- |
| Minimum OS | Windows 7 SP1 x64 |
| Supported OS family | Windows 7, 8, 8.1, 10, and 11 on x64 |
| Unsupported legacy platforms | Windows XP and Vista |
| Architecture | x64 only for the initial product |
| Build system | Visual Studio solution and MSBuild projects |
| Reference toolchain | Visual Studio 2022 17.14, MSVC v143 14.44.35207, Windows SDK 10.0.26100.0 |
| Language mode | C++20 |
| Runtime model | Static MSVC runtime initially |
| Automation | Repository-owned PowerShell and native executables run locally |
| Release model | Manual GitHub Releases after local and physical-machine acceptance |
| First VT7Pty version | `0.5.0-dev`, becoming `0.5.0` only after Milestone 0 acceptance |
| Compatibility identity | Clean VT7Pty names; no WinPTY API or binary aliases |
| Runtime architecture | Separate client DLL and console-owning agent retained |
| Diagnostics | Native debug server retained and modernized during Milestone 0 |
| Driver policy | User mode only; no kernel driver |

The selected compiler and SDK are the initial reproducible reference, not a
claim that only that exact patch version can ever build the project. A toolchain
change must preserve the documented target and reproducibility requirements.

## Windows compatibility contract

The implementation targets Windows 7 with `_WIN32_WINNT=0x0601`. APIs added by
later Windows versions may be used only through a path that preserves Windows 7
startup and behavior, normally explicit capability detection and a tested
fallback.

Windows 7 acceptance uses two physical-machine tiers already used for VT7:

- Windows 7 SP1 x64 with the complete non-ESU public update set available to
  the test machine.
- Windows 7 SP1 x64 with the complete ESU update set available to the test
  machine.

Every result records the actual OS build and installed-update state. The first
acceptance work will establish the precise runtime prerequisite floor instead
of inferring it from compiler settings.

Step 0.9 later added a third Windows 7 SP1 x64 acceptance machine with
PowerShell 2.0 and without KB3191566. The current candidate matrix is in
[Compatibility](COMPATIBILITY.md) and [Testing](TESTING.md).

Windows XP and Vista compatibility code, toolsets, workarounds, public flags,
and claims will be removed after the baseline is captured. Later Windows
versions remain supported targets, and useful version-specific behavior is
retained and tested rather than flattened into Windows 7 behavior.

## Modernization rule

Nothing remains active merely because upstream used it. Every inherited file
is classified and either retained for a current purpose, modernized, replaced,
archived as historical material, or removed.

Milestone 0 removes the Unix adapter and the infrastructure that existed to
build or distribute it. It also removes GYP, Python 2, GNU Make, Cygwin, MSYS,
MinGW, obsolete MSVC compatibility, AppVeyor, and pre-Windows 7 runtime paths
after the supported MSBuild and PowerShell replacements perform all retained
duties.

Core input, screen-scraping, resize, and lifecycle algorithms are changed only
with baseline and regression evidence. Modernization is comprehensive, but
source churn alone is not evidence of an improvement.

## Validation and release rule

The project has no hosted continuous build or test service. Reproducibility
comes from checked-in local commands:

- `Build-VT7Pty.ps1`
- `Verify-VT7Pty.ps1`
- `Package-VT7Pty.ps1`

These scripts are Milestone 0 deliverables and do not exist at this foundation
marker. Local x64 Debug and Release verification precedes every candidate.
Routine candidates run on the non-ESU machine tier. Milestone and release
candidates run on both physical Windows 7 tiers. Results are attached to exact
source and package identities.

The Step 0.9 release gate supersedes this original two-tier plan and requires
the legacy third machine as well.

Publishing a release follows the [manual release process](RELEASING.md) and
remains a deliberate maintainer action after the package,
checksums, symbols, notices, changelog, and physical acceptance records have
been reviewed.

## Scope boundaries

Milestone 0 creates a modern, tested VT7Pty foundation. It does not promise:

- complete ConPTY compatibility,
- integration changes in VT7,
- a stable long-term ABI during the 0.5.x line,
- complete terminal fidelity,
- x86, XP, or Vista support,
- an installer or code signing,
- a published `0.5.0` release before milestone acceptance.

Backend fidelity improvements follow in Milestone 1. The VT7-facing API follows
in Milestone 2. A separately authorized handoff to VT7 follows in Milestone 3.

## Development start marker

The foundation becomes the implementation starting point when its documentation
commit is present on the default branch and marked with the annotated Git tag
`milestone-0-start`. The tag identifies planning approval, not a binary release
or a claim that Milestone 0 is complete.

Before the marker is published:

1. Validate documentation links and consistency.
2. Confirm the commit contains documentation changes only.
3. Push the approved commit to the default branch.
4. Create and push the annotated `milestone-0-start` tag on that exact commit.

All implementation evidence after that marker belongs to the roadmap item it
advances and must preserve the upstream provenance recorded in
[UPSTREAM.md](../UPSTREAM.md).
