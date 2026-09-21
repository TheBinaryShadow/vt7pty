# Building VT7Pty

VT7Pty is at its approved Milestone 0 foundation. The repository still contains
WinPTY's inherited build and packaging systems; the supported VT7Pty MSBuild
workflow described below is the first implementation deliverable and does not
exist yet.

See the [development foundation](docs/FOUNDATION.md),
[roadmap](ROADMAP.md), [compatibility contract](docs/COMPATIBILITY.md), and
[testing strategy](docs/TESTING.md) before changing the build.

## Approved build contract

The maintained build will use:

- Visual Studio 2022 and MSBuild
- MSVC v143, with 14.44.35207 as the initial reference compiler
- Windows SDK 10.0.26100.0
- C++20
- x64 Debug and Release configurations
- static MSVC runtime initially
- Windows 7 SP1 as the API and runtime floor
- repository-owned version and resource generation

The solution will contain projects for the client DLL, console-owning agent,
native debug server, tests, and retained native fixtures and tools. Shared
settings belong in `Directory.Build.props` and `Directory.Build.targets`.

The maintained entry points will be:

```powershell
.\Build-VT7Pty.ps1
.\Verify-VT7Pty.ps1
.\Package-VT7Pty.ps1
```

They will provide repeatable local build, verification, and packaging. The
project does not use a hosted build or test service. Release qualification is a
local and physical-machine process described in
[Releasing](docs/RELEASING.md).

## Current inherited build

Until Roadmap Step 0.2 is implemented, the current native components remain:

- `winpty.dll`
- `winpty-agent.exe`
- `winpty-debugserver.exe`
- inherited tests under `src/tests`

Their public interface is [winpty.h](src/include/winpty.h).

The inherited [vcbuild.bat](vcbuild.bat) path requires Python 2, GYP, and an
older Visual Studio-compatible configuration. The root `configure` and
`Makefile` paths build Cygwin/MSYS and MinGW variants. The `ship` directory uses
Python 2 packaging. AppVeyor describes the old upstream automation.

These paths are historical inputs to the Milestone 0 inventory, not supported
VT7Pty prerequisites. Do not extend them. They will be removed only after the
new MSBuild and PowerShell workflow performs every retained responsibility and
reproduces the recorded baseline.

Roadmap Step 0.1 now has a temporary
[baseline harness](tools/baseline/README.md) and an
[accepted result record](docs/validation/2026-09-21-upstream-baseline.md). That
harness exists only to compare the inherited source before the supported build
lands; it is not a second product build system.

The preserved [upstream README](docs/UPSTREAM_WINPTY_README.md) contains the
original commands for historical reference.

## Debugging and verification

The inherited debugger uses `winpty-debugserver.exe`, `WINPTY_DEBUG=trace`, and
optionally `WINPTY_SHOW_CONSOLE=1`. Milestone 0 will rebrand and modernize this
diagnostic path while retaining source-level Visual Studio debugging.

Every build or behavior result must record:

- source commit and dirty state,
- package version and artifact hashes,
- compiler, toolset, SDK, architecture, and configuration,
- runtime dependency/import inspection,
- host and target OS and update state,
- exact verification commands and per-case results.

A pass on a development machine is not Windows 7 acceptance. Physical Windows
7 procedures and result requirements are defined in
[Testing](docs/TESTING.md).
