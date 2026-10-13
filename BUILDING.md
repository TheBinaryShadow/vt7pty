# Building VT7Pty

I use Visual Studio 2022 and MSBuild for the native x64 build. Local commands
cover compilation, verification, packaging and source debugging. Windows 7
machines run portable test packages and do not need the compiler or SDK.

## Set up the build host

Install the native C++ workload and import [.vsconfig](.vsconfig) through
Visual Studio Installer if components are missing. The reference toolchain is:

| Component | Version or setting |
| --- | --- |
| Visual Studio | 2022, with 17.14 as the reference |
| MSVC | v143, reference 14.44.35207 |
| Windows SDK | 10.0.26100.0 |
| Language and architecture | C++20, x64 Debug and Release |
| Runtime | Static MSVC CRT |
| Platform floor | Windows 7 SP1, _WIN32_WINNT=0x0601 |

Shared settings live in [Directory.Build.props](Directory.Build.props) and
[Directory.Build.targets](Directory.Build.targets). Builds use the conforming
preprocessor, /W4 /WX, version resources and Windows 7-compatible platform
settings. Release verification also runs MSVC static analysis.

## Build and verify

Open [VT7Pty.sln](VT7Pty.sln), or run these commands from the repository root
in 64-bit Windows PowerShell 5.1:

~~~powershell
.\Build-VT7Pty.ps1
.\Verify-VT7Pty.ps1
~~~

Both commands default to Debug and Release. Use -Configuration Debug or
-Configuration Release for one configuration. To run static analysis separately:

~~~powershell
.\Analyze-VT7Pty.ps1
~~~

Outputs go under ignored artifacts; intermediate files go under ignored build.
[Testing](docs/TESTING.md) explains the checks, result files and optional soak.
I keep local results separate from [Windows 7 acceptance](docs/WINDOWS7_ACCEPTANCE.md).

## Package a candidate

From a clean source tree:

~~~powershell
.\Package-VT7Pty.ps1 -Configuration Release
~~~

Packaging rebuilds Release, runs static analysis and verification, then creates
runtime, development, symbols and test archives. It checks the extracted files
against their manifests and writes a release-set manifest and SHA-256 file.
It refuses a dirty tree or an existing candidate asset.

The [release guide](docs/RELEASING.md) explains which archive to use and how I
review the exact package on the Windows 7 machines. I publish releases manually;
there is no hosted CI service.

## Find the code

The runtime consists of VT7Pty.dll and VT7Pty-Agent.exe, with
VT7Pty-DebugServer.exe for diagnostics. The public API is in
[vt7pty.h](src/include/vt7pty.h); its version is in
[vt7pty_version.h](src/include/vt7pty_version.h).

The solution also builds unit and integration tests, controlled
[fixtures](tests/fixtures/README.md), [console tools](tools/console/README.md)
and fidelity research probes. See the [architecture](docs/ARCHITECTURE.md)
and [documentation index](docs/README.md) for those components.

## Debug

Select Debug|x64 in Visual Studio and use BackendSmokeTest as a starting
project. The diagnostic tools support VT7PTY_DEBUG=trace and
VT7PTY_SHOW_CONSOLE=1; see [Diagnostics](docs/DIAGNOSTICS.md) for collection
commands and the restricted debug pipe.

For a repeatable source and caller-line check:

~~~powershell
.\tools\debug\Verify-SourceDebugging.ps1
~~~

This uses x64 cdb.exe from the Windows SDK's optional **Debugging Tools for
Windows** component. It breaks at vt7pty_config_new and checks source locations
in vt7pty.cc and BackendSmokeTest.cc. The transcript is written to
artifacts\debugging\source-debugging.txt.

I record the source commit, dirty state, toolchain, package identity and test
environment with each result. The [validation index](docs/validation/README.md)
preserves the build transition and later acceptance records. Retired build
commands remain in the [original WinPTY README](docs/UPSTREAM_WINPTY_README.md)
for historical reference.
