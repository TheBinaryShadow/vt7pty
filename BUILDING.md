# Building VT7Pty

VT7Pty is implementing Roadmap Step 0.2. The first supported Visual
Studio/MSBuild workflow now builds and verifies the inherited native boundary.
The repository still contains WinPTY's old build and packaging systems until
the new workflow covers the retained fixtures and completes the transition
evidence.

See the [development foundation](docs/FOUNDATION.md),
[roadmap](ROADMAP.md), [compatibility contract](docs/COMPATIBILITY.md), and
[testing strategy](docs/TESTING.md) before changing the build.

## Supported build

The maintained build uses:

- Visual Studio 2022 and MSBuild
- MSVC v143, with 14.44.35207 as the initial reference compiler
- Windows SDK 10.0.26100.0
- C++20
- x64 Debug and Release configurations
- static MSVC runtime initially
- Windows 7 SP1 as the API and runtime floor
- repository-owned version and resource generation

Open `VT7Pty.sln` in Visual Studio 2022 or run the repository commands from
Windows PowerShell 5.1 or newer. Import `.vsconfig` through Visual Studio
Installer if the required native workload or SDK is missing.

```powershell
.\Build-VT7Pty.ps1
.\Verify-VT7Pty.ps1
.\Package-VT7Pty.ps1
```

All commands default to the configurations appropriate to their job. Build and
verify accept `-Configuration Debug`, `Release`, or `All`; package defaults to
Release. Outputs are written below ignored `artifacts`, with intermediates
below ignored `build`.

The solution currently contains projects for the inherited client DLL,
console-owning agent, native debug server, and two inherited tests. Retained
native fixtures and tools will join it as they move out of `misc`. Shared
compiler and linker settings live in `Directory.Build.props` and generated
version identity is wired through `Directory.Build.targets`.

The project does not use a hosted build or test service. Release qualification
is a local and physical-machine process described in
[Releasing](docs/RELEASING.md).

## Current native identity

Until technical rebranding in Roadmap Step 0.3, outputs remain:

- `winpty.dll`
- `winpty-agent.exe`
- `winpty-debugserver.exe`
- inherited tests under `src/tests`

Their public interface is [winpty.h](src/include/winpty.h). This temporary
name preservation lets the verifier compare the new build against the accepted
inherited exports, imports, and process lifecycle before those names change.

`Verify-VT7Pty.ps1` runs `StringBuilderTest.exe`, `trivial_test.exe`, and the
agent version command. It also requires x64 images, PE subsystem version 6.01,
the expected direct imports, the 19 inherited DLL exports, PDBs, and generated
version resources. Its development-host result is transition evidence and is
not physical Windows 7 acceptance.

`Package-VT7Pty.ps1` currently creates one transition ZIP with binaries,
headers, the import library, symbols, attribution, a manifest, and a SHA-256
sidecar. Roadmap Step 0.9 will fix the final release archive split and naming.
The accepted development-host result for this workflow is recorded in the
[MSBuild transition validation](docs/validation/2026-09-21-msbuild-transition.md).

## Inherited build paths

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

## Debugging

The native debugger uses `winpty-debugserver.exe`, `WINPTY_DEBUG=trace`, and
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
