# VT7Pty Compatibility Contract

Status: current claims and proposed Milestone 0 decisions. Updated: 2026-09-21.

This document separates project targets from verified support. A row marked
planned or untested is not a compatibility promise.

## Current status

| Area | Current status |
| --- | --- |
| Source baseline | WinPTY `0.4.4-dev` at upstream commit `7e59fe2` |
| Development host | Local x64 native build and debugger smoke check completed on Windows 10 build 19044 |
| Windows 7 runtime | Primary target; current VT7Pty checkout has no recorded acceptance run |
| Architectures | x64 locally exercised through a temporary harness; x86 compiler present but untested |
| Build interface | Inherited Python 2/GYP and GNU make paths remain; no supported modern VT7Pty build yet |
| API and binaries | Inherited WinPTY names and behavior |
| ConPTY compatibility | Not implemented; ConPTY is a design and behavioral reference |
| VT7 integration | Planned; no VT7Pty integration claim |
| Releases | No VT7Pty release has been published |

Historical WinPTY support statements describe upstream releases and do not
automatically qualify future VT7Pty builds.

## Primary target

The planned primary runtime target is Windows 7 SP1 x64. Before Roadmap Step
0.1 can be accepted, this document must record the exact project prerequisite
floor, including:

- Windows 7 servicing/update expectations.
- Required loader or runtime updates.
- CRT deployment model and packaged redistributables, if any.
- Architecture and subsystem target.
- Whether optional functionality needs additional OS components.
- How a package detects or reports a missing prerequisite.

The target machine must not require the modern compiler or SDK. A build-time
`_WIN32_WINNT` value, subsystem header value, successful link, or static CRT
choice alone does not establish runtime compatibility.

## Development and build hosts

Modern toolchains may run on newer Windows versions while producing Windows 7
binaries. Milestone 0 will select and record exact supported build tools. The
current proposal is CMake with MSVC, x64 Debug/Release builds, and Ninja for
scripted builds plus a Visual Studio debugging path.

Hosted CI demonstrates reproducibility on its runner. It cannot replace the
required Windows 7 execution and behavior checks.

## Runtime dependency audit

Every candidate package must be checked for:

- Direct imports from system DLLs.
- Delay-loaded and dynamically resolved APIs.
- Transitive DLL and CRT dependencies.
- API flags, structures, interface versions, and behavior that differ on
  Windows 7 even when an export exists.
- Agent/client architecture and version agreement.
- Accidental dependence on build-machine paths or tools.

Record both static inspection and execution on the target. Unsupported imports
or unexplained loader failures block acceptance.

## Application compatibility

Compatibility is application- and behavior-specific. Launching successfully is
insufficient. Claims should identify the application/version and tested input,
output, resize, cancellation, screen restoration, and shutdown behavior.

Initial required local applications are Command Prompt and Windows PowerShell.
The SSH/full-screen application path will be defined after the exact compatible
client and data path are selected. See [Testing](TESTING.md).

## ConPTY and other consumers

VT7Pty does not currently expose the ConPTY ABI, emulate
`PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`, or claim compatibility with unmodified
ConPTY clients. A ConPTY-shaped VT7 integration API and a general ConPTY shim
are distinct goals.

After VT7 readiness, the project may test a user-mode shim against named
consumers. Compatibility must cover process creation, handle ownership,
lifecycle, stream behavior, resize, and errors, not merely matching export
names. A kernel driver is not planned.

## Publishing compatibility claims

Use these terms consistently:

- **Target:** intended platform or behavior, not yet accepted.
- **Tested:** passed a named procedure on an identified configuration.
- **Supported:** included in an active release support policy.
- **Limited:** tested with documented constraints.
- **Untested:** no evidence sufficient for a claim.

Update this document, the README, security support table, and release notes
together when compatibility status changes.
