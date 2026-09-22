# Building VT7Pty

VT7Pty's supported Visual Studio/MSBuild workflow builds, verifies, packages,
and source-debugs the inherited native boundary. It is the repository's only
active build and packaging workflow.

See the [development foundation](docs/FOUNDATION.md),
[roadmap](ROADMAP.md), [compatibility contract](docs/COMPATIBILITY.md), and
[testing strategy](docs/TESTING.md) before changing the build.

## Supported build

The maintained build uses:

- Visual Studio 2022 and MSBuild
- MSVC v143, with 14.44.35207 as the initial reference compiler
- Windows SDK 10.0.26100.0
- C++20
- the conforming MSVC preprocessor (`/Zc:preprocessor`)
- warning level 4 with warnings treated as errors (`/W4 /WX`)
- the MSVC native code analyzer for Release verification
- x64 Debug and Release configurations
- static MSVC runtime initially
- Windows 7 SP1 as the API and runtime floor
- repository-owned version and resource generation

Open `VT7Pty.sln` in Visual Studio 2022 or run the repository commands from
Windows PowerShell 5.1 or newer. Import `.vsconfig` through Visual Studio
Installer if the required native workload or SDK is missing.

```powershell
.\Build-VT7Pty.ps1
.\Analyze-VT7Pty.ps1
.\Verify-VT7Pty.ps1
.\Package-VT7Pty.ps1
```

All commands default to the configurations appropriate to their job. Build and
verify accept `-Configuration Debug`, `Release`, or `All`; package defaults to
Release. Outputs are written below ignored `artifacts`, with intermediates
below ignored `build`.

The solution contains projects for the client DLL, console-owning agent,
native debug server, protocol and modern-C++ unit tests, backend integration
test, nine controlled child-process fixtures, and two console-mode tools. The
fixture and tool purpose is described
under [tests/fixtures](tests/fixtures/README.md) and
[tools/console](tools/console/README.md). Shared compiler and linker settings
live in `Directory.Build.props`, while generated version identity is wired
through `Directory.Build.targets`.

The project does not use a hosted build or test service. Release qualification
is a local and physical-machine process described in
[Releasing](docs/RELEASING.md).

## Native identity

Roadmap Step 0.5 established these outputs:

- `VT7Pty.dll`
- `VT7Pty-Agent.exe`
- `VT7Pty-DebugServer.exe`
- `BackendSmokeTest.exe`, `ProtocolTest.exe`, and the isolated
  `ProtocolTestAgent.exe` negative-control fixture

Their public interface is [vt7pty.h](src/include/vt7pty.h), with API-version
macros in [vt7pty_version.h](src/include/vt7pty_version.h). See the
[technical naming map](docs/NAMING.md) for the intentional break from WinPTY.

`Verify-VT7Pty.ps1` runs the modern-C++ utility, protocol, backend, missing-agent,
incompatible-agent, application, and deterministic fixture checks. It also
requires x64 images, PE subsystem version 6.01, the expected direct imports,
the 19 renamed DLL exports, PDBs, generated version resources, and a clean
Release static-analysis pass. Its
development-host result is transition evidence and is not physical Windows 7
acceptance.

`Package-VT7Pty.ps1` currently creates one transition ZIP with binaries,
headers, the import library, symbols, attribution, a manifest, and a SHA-256
sidecar. Roadmap Step 0.9 will fix the final release archive split and naming.
The accepted build boundary and completed developer workflow are recorded in
the [MSBuild transition validation](docs/validation/2026-09-21-msbuild-transition.md)
and [Step 0.2 completion validation](docs/validation/2026-09-22-step-0.2-completion.md).
Step 0.3's removal and regression evidence is recorded in the
[Step 0.3 completion validation](docs/validation/2026-09-22-step-0.3-completion.md).

## Retired build paths

Roadmap Step 0.3 removed the inherited GYP, GNU Make, Cygwin/MSYS, MinGW,
Python 2 packaging, and AppVeyor paths after the maintained workflow reproduced
their required native boundary. The temporary baseline harness was also
retired after its results were preserved in the
[accepted baseline record](docs/validation/2026-09-21-upstream-baseline.md).
Git history retains every removed file.

The preserved [upstream README](docs/UPSTREAM_WINPTY_README.md) contains the
original commands for historical reference.

## Debugging

The native debugger uses `VT7Pty-DebugServer.exe`, `VT7PTY_DEBUG=trace`, and
optionally `VT7PTY_SHOW_CONSOLE=1`. Diagnostic messages use the versioned
`\\.\pipe\VT7Pty-Debug-v1` endpoint.

Open `VT7Pty.sln`, select `Debug|x64`, and use `BackendSmokeTest` as a convenient
debugging startup project. The repository also provides a repeatable command
line proof that the linked PDBs resolve both library and caller source lines:

```powershell
.\tools\debug\Verify-SourceDebugging.ps1
```

This check uses the x64 `cdb.exe` installed by **Debugging Tools for Windows**,
available as an optional Windows SDK component. Its transcript is written to
ignored `artifacts\debugging\source-debugging.txt`.

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
