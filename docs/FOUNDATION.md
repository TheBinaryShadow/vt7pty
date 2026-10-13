# Development foundation

I settled these decisions on 2026-09-21 before starting Milestone 0.
They explain the direction of VT7Pty. The [roadmap](../ROADMAP.md) tracks current
work, and the [completed checklist](MILESTONE_0.md) records how I established
the 0.5.0 baseline.

## Why I'm building it

I want a user-mode console backend that improves WinPTY's application-facing
fidelity and reliability for VT7 on Windows 7. WinPTY is the implementation
foundation; ConPTY and Microsoft Terminal are references for behavior and design.

I work on VT7Pty in this repository. VT7 integration is a separate task after
the backend and API are ready. General ConPTY compatibility is later work.
I am not planning a kernel driver.

## Engineering decisions

| Area | My choice |
| --- | --- |
| Minimum OS and architecture | Windows 7 SP1 x64 |
| Later target systems | Windows 8, 8.1, 10 and 11 on x64 |
| Outside scope | XP, Vista and x86 |
| Build | Visual Studio solution and MSBuild projects |
| Reference toolchain | VS2022 17.14, MSVC v143 14.44.35207, Windows SDK 10.0.26100.0 |
| Language and runtime | C++20, initially the static MSVC runtime |
| Backend | Separate client DLL and console-owning agent |
| Identity | VT7Pty names with no WinPTY API or binary aliases |
| Verification | Local PowerShell commands and native tests |
| Releases | Manual GitHub Releases after local and Windows 7 testing |
| Initial version | 0.5.0-dev during development, then separately tested 0.5.0 |

A newer toolchain can be adopted with reproducible results and Windows 7
validation. SDK declarations alone do not establish runtime compatibility.
Later APIs need a Windows 7-safe capability path and a tested fallback.

## How I modernize the code

I keep inherited code because it serves the backend, not simply because it was
there. The [inventory](INVENTORY.md) records what I retained, modernized, replaced,
archived or removed.

Milestone 0 replaced the build before removing its predecessors. It removed
the Unix adapter, GYP, GNU Make, Python 2 packaging, Cygwin/MSYS/MinGW paths,
old compiler workarounds, AppVeyor and pre-Windows 7 runtime branches.
I keep baseline and regression evidence before changing input parsing, screen
scraping, resize ordering or lifecycle behavior.

## How I test and release

The local entry points are Build-VT7Pty.ps1, Verify-VT7Pty.ps1 and
Package-VT7Pty.ps1. I verify Debug and Release locally, then test the exact
candidate on Windows 7. There is no hosted CI service.

I started with non-ESU and ESU machines. Step 0.9 added the legacy machine with
PowerShell 2.0 and without KB3191566. The current
[compatibility policy](COMPATIBILITY.md) records all three, including machine
inventory and the exceptions I accepted during early testing.

I review the package, checksums, symbols, notices, changelog and target results
before publishing through the [release process](RELEASING.md).
Passing tests and publishing a release are separate decisions.

## What comes after the baseline

Milestone 0 established the native foundation. It did not promise complete
terminal fidelity, VT7 integration, a stable long-term 0.5.x ABI, an installer
or code signing. Backend behavior comes next, followed by the VT7-facing API
and a versioned handoff.

The milestone-0-start tag names the original planning marker, not a binary
release. [Upstream provenance](../UPSTREAM.md) and dated
[validation records](validation/README.md) remain the history behind later work.
