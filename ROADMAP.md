# VT7Pty Roadmap

Status: approved. Updated: 2026-09-22.

This file owns milestone scope and completion. It records intended work, not
implemented capability. Milestones follow technical dependencies rather than
calendar dates. The decisions that authorize Milestone 0 are recorded in the
[development foundation](docs/FOUNDATION.md).

An unchecked item (`[ ]`) is planned, in progress, or awaiting validation. A
checked item (`[x]`) has met its stated scope and has linked evidence. Partial
implementation remains unchecked. A development-host pass cannot complete an
item that requires physical Windows 7 acceptance.

Supporting documents:

- [Development foundation](docs/FOUNDATION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing strategy](docs/TESTING.md)
- [Compatibility contract](docs/COMPATIBILITY.md)
- [Versioning policy](docs/VERSIONING.md)
- [Manual release process](docs/RELEASING.md)
- [Inherited component inventory](docs/INVENTORY.md)
- [Inherited native baseline](docs/validation/2026-09-21-upstream-baseline.md)
- [MSBuild transition validation](docs/validation/2026-09-21-msbuild-transition.md)
- [Step 0.2 completion validation](docs/validation/2026-09-22-step-0.2-completion.md)
- [Step 0.3 completion validation](docs/validation/2026-09-22-step-0.3-completion.md)
- [Upstream provenance](UPSTREAM.md)

## Goal and boundaries

VT7Pty will provide a reliable user-mode console backend for Windows 7 and
later, improve on WinPTY's application-facing fidelity, and prepare a clean
integration path for VT7.

WinPTY is the implementation foundation. ConPTY is a behavioral and API
reference. VT7 is the primary intended consumer. Compatibility for other
consumers and a possible ConPTY user-mode shim are later work. A kernel driver
is outside the project direction.

Development takes place in this repository. VT7 and Microsoft's Terminal
repository are reference sources; this roadmap does not authorize changes to
either project.

## Current baseline

- The inherited source is WinPTY `0.4.4-dev` at
  [`7e59fe2`](https://github.com/rprichard/winpty/commit/7e59fe2d09adf0fa2aa606492e7ca98efbc5184e).
- Active runtime names, public APIs, endpoints, diagnostics, and version
  metadata use the VT7Pty `0.5.0-dev` identity. The Unix adapter and superseded
  build, package, and compiler-compatibility paths have been removed.
- The maintained VS2022/MSBuild workflow builds, verifies, packages, and
  source-debugs the renamed x64 native boundary. This is development-host
  evidence, not Windows 7 acceptance.

Completed project foundation:

- [x] Establish the VT7Pty name, purpose, and independent-project identity.
- [x] Record the WinPTY baseline and preserve upstream license and attribution.
- [x] Add contribution, conduct, security, build, release, and maintainer guidance.
- [x] Approve Windows 7 SP1 x64 as the minimum platform and x64 as the supported
  architecture for the initial product.
- [x] Approve Visual Studio/MSBuild, C++20, the static CRT, local verification,
  physical Windows 7 acceptance, and manual GitHub Releases.
- [x] Approve removal of the Unix adapter, XP/Vista support, old toolchains, and
  superseded build and packaging systems after their retained duties are replaced.
- [x] Approve a clean VT7Pty technical identity and the `0.5.0-dev` line.
- [x] Establish this roadmap and its supporting documentation as the Milestone 0
  development foundation.

These decisions authorize the work. Implementation status and evidence are
recorded separately by the checked items below.

## Milestone overview

| Milestone | Outcome | Status |
| --- | --- | --- |
| 0 | Fully modernized, native VT7Pty 0.5.x foundation | In progress; Steps 0.1-0.7 accepted, Step 0.8 candidate prepared |
| 1 | Measured backend fidelity and reliability improvements | Direction approved; detailed scope follows Milestone 0 evidence |
| 2 | Documented VT7Pty API and standalone integration host | Direction approved; detailed scope follows Milestone 1 |
| 3 | Versioned backend handoff ready for separately authorized VT7 integration | Direction approved; detailed scope follows Milestone 2 |
| Later | Other consumers and optional ConPTY user-mode compatibility | Deferred |

## Milestone 0: Establish the modern VT7Pty foundation

Testing begins with the baseline and grows throughout the milestone. Build
replacement precedes deletion. Mechanical modernization and rebranding remain
separate from intentional backend behavior changes so regressions can be
attributed and corrected.

### Step 0.1: Record the upstream behavioral baseline

Outcome: later work can be compared with an identifiable, reproducible record
of the inherited native backend.

- [x] Inventory every source component, build path, generated file, dependency,
  diagnostic tool, package responsibility, and test.
- [x] Classify every retained file as keep, modernize, replace, archive as
  historical material, or remove.
- [x] Record the exact upstream revision and clean/dirty source state.
- [x] Produce x64 Debug and Release baseline artifacts with symbols.
- [x] Record artifact names, hashes, imports, exports, runtime dependencies, and
  compiler and SDK identity.
- [x] Run inherited automated tests and representative console sessions.
- [x] Record current development-host results, failures, limitations, and
  untested behavior without treating them as Windows 7 acceptance.

Exit criterion met by the [component inventory](docs/INVENTORY.md) and
[accepted development-host baseline](docs/validation/2026-09-21-upstream-baseline.md).
Physical Windows 7 execution remains a later step and is not implied here.

### Step 0.2: Replace build and developer infrastructure

Outcome: one maintained Visual Studio/MSBuild workflow builds, verifies,
packages, and debugs the native project from a clean checkout.

- [x] Add a Visual Studio solution and MSBuild C++ projects for the client DLL,
  agent, native debug server, and inherited tests.
- [x] Add maintained project membership for the initial retained fixture and
  tool set as it moves from `misc` into permanent locations.
- [x] Centralize configuration in `Directory.Build.props` and
  `Directory.Build.targets`.
- [x] Target MSVC v143, C++20, Windows SDK 10.0.26100.0, Unicode, and x64 Debug
  and Release configurations.
- [x] Use Windows 7 SP1 as the API floor and the static CRT for self-contained
  initial deployment.
- [x] Establish explicit conformance, warning, optimization, symbol, manifest,
  version-resource, and Windows 7-compatible security settings.
- [x] Add `.vsconfig`, `.editorconfig`, and formatting configuration.
- [x] Add `Build-VT7Pty.ps1`, `Verify-VT7Pty.ps1`, and
  `Package-VT7Pty.ps1` as the maintained local entry points.
- [x] Keep target-side scripts compatible with Windows PowerShell 5.1 where
  they must run on Windows 7.
- [x] Generate version and artifact identity from repository-owned sources.
- [x] Verify source-level debugging and inspect imports and runtime dependencies.
- [x] Reproduce the inherited baseline before removing any old build route.

The clean-commit [transition record](docs/validation/2026-09-21-msbuild-transition.md)
reproduces the inherited tests, imports, exports, and artifact boundary. The
[Step 0.2 completion record](docs/validation/2026-09-22-step-0.2-completion.md)
adds the maintained fixture/tool projects, verifies their binaries, and proves
source and caller line resolution under CDB.

The project will not add hosted build or test automation. Release qualification
uses the maintained local commands and physical acceptance machines.

Exit criterion met: clean commit `a43f319` builds, verifies, packages, and
debugs through MSBuild and maintained PowerShell entry points without relying
on the inherited build system. Remaining `misc` classification and obsolete
infrastructure removal belong to Step 0.3.

### Step 0.3: Remove obsolete infrastructure and Unix-derived components

Outcome: the active tree contains no technology retained solely for the Unix
adapter, retired compilers, or superseded packaging.

- [x] Remove `src/unix-adapter` and the Cygwin/MSYS `winpty.exe` frontend.
- [x] Remove `configure`, GNU Makefiles, `subdir.mk`, GYP files, `vcbuild.bat`,
  and their generated-file conventions.
- [x] Remove Cygwin, MSYS, MinGW, GCC, old MSVC, and Node/GYP conditionals that
  no longer serve the native backend.
- [x] Remove Python 2 packaging and replace its retained responsibilities with
  `Package-VT7Pty.ps1`.
- [x] Remove AppVeyor configuration and obsolete shell build scripts.
- [x] Classify `misc` individually: move useful console probes into tests, move
  maintained developer utilities under `tools`, preserve valuable research as
  documentation, and remove obsolete experiments.
- [x] Rename and retain shared control-character behavior currently carrying a
  Unix-oriented filename when it remains part of the Windows input backend.
- [x] Refresh repository ignores, attributes, build documentation, and package
  documentation for the single supported workflow.
- [x] Rebuild and rerun baseline checks after each coherent removal group.

Exit criterion met by the
[Step 0.3 completion record](docs/validation/2026-09-22-step-0.3-completion.md):
the active build and source tree has no operational dependency
on the Unix adapter, GYP, Python 2, GNU Make, Cygwin, MSYS, MinGW, AppVeyor, or
obsolete compiler compatibility code.

### Step 0.4: Establish the Windows 7+ platform implementation

Outcome: the maintained runtime has one explicit Windows platform floor and no
XP/Vista implementation burden.

- [x] Set `_WIN32_WINNT` and the associated platform configuration to Windows 7.
- [x] Remove Windows XP and Vista build, runtime, font, console, desktop, and
  diagnostic compatibility paths.
- [x] Remove XP toolset logic and SDK workarounds.
- [x] Evaluate the background-desktop workaround and remove it and its public
  flag if baseline and Windows 7 validation confirm it serves only pre-Windows 7.
- [x] Simplify console-font behavior around APIs available on Windows 7 SP1.
- [x] Replace obsolete OS-version checks with correct version or capability
  detection.
- [x] Preserve later-Windows behavior branches that remain useful.
- [x] Audit subsystem versions, manifests, imports, delay loading, dynamically
  resolved APIs, structures, flags, and runtime dependencies.
- [x] Verify startup and representative sessions on physical non-ESU and ESU
  Windows 7 SP1 x64 machines.

Development-host verification and the exact portable candidate are recorded in
the [Step 0.4 platform-candidate evidence](docs/validation/2026-09-22-step-0.4-candidate.md).
The accepted physical results and evidence deviation are recorded in the
[Step 0.4 completion evidence](docs/validation/2026-09-22-step-0.4-completion.md).

Exit criterion: Windows 7 SP1 x64 is the demonstrable minimum, newer x64 Windows
versions remain in scope, and no active XP/Vista compatibility path remains.

### Step 0.5: Complete technical rebranding and 0.5.x identity

Outcome: active artifacts, APIs, diagnostics, and protocols have a consistent
VT7Pty identity while historical attribution retains the WinPTY name.

- [x] Publish an old-to-new naming map.
- [x] Rename the native artifacts to `VT7Pty.dll`, `VT7Pty-Agent.exe`, and
  `VT7Pty-DebugServer.exe`.
- [x] Rename active headers, exports, C API symbols, constants, source/build
  identifiers, diagnostics, agent lookup, environment variables, IPC/debug
  endpoints, package metadata, tests, and documentation.
- [x] Use `vt7pty_*` for the inherited public C surface and `VT7PTY_*` for its
  constants and macros, without WinPTY compatibility aliases.
- [x] Change the development version to `0.5.0-dev` using one authoritative
  package-version source.
- [x] Introduce independent API and client-agent protocol versions where the
  approved versioning policy requires them.
- [x] Add consistent Windows file and product version resources.
- [x] Reject missing, malformed, or incompatible agents with actionable errors.
- [x] Verify that remaining WinPTY names occur only in attribution, provenance,
  migration material, historical documentation, and inherited Git history.
- [x] Rebuild, package, inspect, and rerun the baseline:
  - [x] Development host, Debug and Release.
  - [x] Physical non-ESU Windows 7 SP1 x64.
  - [x] Physical ESU Windows 7 SP1 x64.

This is an intentional source and binary compatibility break. It does not claim
interchangeability with WinPTY or ConPTY.

Exit criterion: every active project-owned identity follows the approved map;
package, API, protocol, and source identities are unambiguous; and the DLL and
agent cannot silently form an incompatible pair.

The implementation, development-host verification, and exact portable package
are recorded in the [Step 0.5 technical identity candidate](docs/validation/2026-09-22-step-0.5-candidate.md).
The accepted physical results and evidence deviation are recorded in the
[Step 0.5 completion evidence](docs/validation/2026-09-22-step-0.5-completion.md).

### Step 0.6: Modernize the native codebase

Outcome: the retained implementation uses current C++ and Windows engineering
practice without gratuitously rewriting proven console algorithms.

- [x] Replace the custom old-MinGW mutex layer with standard C++ synchronization
  (completed and verified during Step 0.3).
- [x] Replace compiler compatibility macros with C++20 language and library
  facilities where their Windows 7 behavior is verified.
- [x] Remove legacy formatting, architecture, exception, integer, and header
  shims that no longer serve the selected toolchain.
- [x] Strengthen handle, process, thread, pipe, and allocation ownership through
  focused RAII and move semantics.
- [x] Modernize buffer and string handling, bounds checks, typed constants,
  scoped enums, null handling, and error propagation.
- [x] Remove obsolete conditional compilation and introduce consistent internal
  namespaces and source organization.
- [x] Reorganize retained code under clear `include`, `src`, `tests`, `tools`,
  and `docs` boundaries using Git-aware moves.
- [x] Clean the project to `/W4`; enable warnings as errors only after inherited
  warning debt is resolved.
- [x] Add repeatable local static analysis to release verification.
- [x] Avoid new third-party dependencies unless they provide a reviewed,
  documented benefit and retain Windows 7 compatibility.
- [x] Keep any bulk formatting change separate from functional changes.
- [x] Add focused regression evidence before refactoring input parsing, screen
  scraping, resize ordering, or lifecycle algorithms.

Exit criterion: no obsolete compiler compatibility layer remains, maintained
targets build cleanly under the approved toolchain, and tests show that source
modernization preserved the accepted backend behavior.

The implementation, development-host verification, and exact portable package
are recorded in the
[Step 0.6 native modernization candidate](docs/validation/2026-09-22-step-0.6-candidate.md).
The accepted physical results and owner-approved environment disposition are
recorded in the
[Step 0.6 completion evidence](docs/validation/2026-09-23-step-0.6-completion.md).

### Step 0.7: Modernize diagnostics and security boundaries

Outcome: physical-machine failures are diagnosable and native trust boundaries
have explicit validation.

- [x] Retain and rebrand the native debug server while removing obsolete Python
  debug helpers.
- [x] Add structured severity and subsystem categories, timestamps, process and
  thread IDs, version identity, and bounded debugger/file output.
- [x] Document diagnostic collection for remote physical Windows 7 machines.
- [x] Audit named-pipe permissions and peer validation.
- [x] Audit handle inheritance, process creation, command-line quoting,
  executable and DLL lookup, temporary paths, and environment parsing.
- [x] Audit IPC message sizes, integer conversions, buffer boundaries, shutdown
  races, and malformed-client behavior.
- [x] Verify that default logs exclude credentials and private session content.

Exit criterion: diagnostic bundles identify the exact build and failure, and
the application-facing and client-agent boundaries have a documented security
review with resolved blockers or explicit dispositions.

The implementation, security dispositions, development-host verification,
and exact portable package are recorded in the
[Step 0.7 diagnostics and security candidate](docs/validation/2026-09-24-step-0.7-candidate.md).
Both physical Windows 7 tiers passed the exact candidate; the results and
accepted environment disposition are recorded in the
[Step 0.7 completion evidence](docs/validation/2026-09-24-step-0.7-completion.md).

### Step 0.8: Establish the permanent test system

Outcome: one local command verifies the project, and a portable bundle runs the
same accepted cases on either physical Windows 7 tier.

- [x] Implement standalone unit, component, and integration test executables.
- [x] Add controlled child-process and console behavior fixtures.
- [x] Cover build/package identity, startup, spawn, input, output, Unicode,
  resize, lifecycle, interactive applications, diagnostics, and failures.
- [ ] Cover handle/process leaks, output drain, agent/client failure, shutdown
  races, repeated sessions, resize under load, and bounded long-running use.
- [x] Add negative controls that prove incorrect output, status, ordering,
  timeout, incompatible agents, and leaked state are detected.
- [x] Produce machine-readable results and a concise human-readable summary.
- [x] Build a portable Windows 7 acceptance bundle with instructions and all
  required fixtures.
- [ ] Record exact OS/update state, architecture, package identity, per-case
  result, timing, logs, and crash artifacts for every physical run.
- [ ] Run routine candidates on non-ESU Windows 7 and milestone/release
  candidates on both non-ESU and ESU Windows 7.
- [x] Record a defined SSH/full-screen baseline or the exact unresolved
  dependency for Milestone 1.

Exit criterion: all required cases report pass, fail, skipped, or not run; no
unexplained crash, hang, data loss, orphan, or material resource regression
remains; and both Windows 7 tiers accept the milestone candidate.

The implementation and development-host checks are recorded in the
[Step 0.8 permanent test system candidate](docs/validation/2026-09-24-step-0.8-candidate.md).
Physical acceptance on both Windows 7 tiers and the full soak remain pending.

### Step 0.9: Establish the manual release process

Outcome: an identified, reviewed release can be reproduced locally and
published without retired upstream infrastructure or hosted automation.

- [ ] Make `Package-VT7Pty.ps1` produce runtime, development, symbols, and test
  archives from an accepted Release build.
- [ ] Include headers, import libraries, PDBs, licenses, notices, build/component
  manifest, source revision, toolchain identity, and SHA-256 checksums.
- [ ] Require a clean local build and verification result before packaging.
- [ ] Require non-ESU and ESU physical acceptance records for a release.
- [ ] Document changelog, version, tag, archive, checksum, and GitHub Release
  preparation and review.
- [ ] Keep installers and code signing outside the initial release process until
  distribution needs justify them.

Exit criterion: VT7Pty `0.5.0` can be built, tested, packaged, reviewed, and
published manually with complete identity and acceptance evidence.

### Milestone 0 acceptance

- [x] Accept Step 0.1 with linked baseline evidence.
- [x] Accept Step 0.2 with linked build and debugging evidence.
- [x] Accept Step 0.3 with linked removal and regression evidence.
- [x] Accept Step 0.4 with linked platform and physical-machine evidence.
- [x] Accept Step 0.5 with linked naming, version, protocol, and package evidence.
- [x] Accept Step 0.6 with linked code-quality and regression evidence.
- [x] Accept Step 0.7 with linked diagnostics and security evidence.
- [ ] Accept Step 0.8 with linked automated and physical test results.
- [ ] Accept Step 0.9 with a reviewed release-candidate package.

`0.5.0-dev` remains a development identity until acceptance. Creating a
`0.5.0` tag or GitHub Release is a separate publication decision. Milestone 0
does not claim complete terminal fidelity, VT7 integration, or ConPTY
compatibility.

## Milestone 1: Improve backend fidelity and reliability

- [ ] Define the application corpus and measurable fidelity criteria from the
  Milestone 0 baseline, including local shells and the identified SSH path.
- [ ] Prioritize and improve input semantics, output/state reconstruction,
  Unicode preservation, resize, screen restoration, and lifecycle behavior.
- [ ] Add regression coverage and Windows 7 evidence with each accepted change.
- [ ] Where screen scraping cannot preserve required information, prove an
  adaptation or record an explicit scope decision.
- [ ] Publish tested capabilities, limitations, stability, and performance.

Exit criterion: agreed application workflows meet their documented Windows 7+
fidelity and reliability criteria with no unexplained baseline regression.

## Milestone 2: Define the VT7 integration API

- [ ] Review VT7's connection requirements as reference material without
  modifying the VT7 repository.
- [ ] Specify handle ownership, stream encoding, errors, threading, cancellation,
  resize/close races, output draining, process lifetime, version negotiation,
  and distribution requirements before freezing an ABI.
- [ ] Implement documented headers and the API over the proven backend.
- [ ] Build a standalone consumer and contract tests in this repository.
- [ ] Validate the contract on Windows 7 and publish examples and migration
  guidance from the renamed inherited interface.

Exit criterion: documented API behavior, contract tests, examples, and the
standalone consumer pass against the Windows 7 backend.

## Milestone 3: Prepare the VT7 handoff

- [ ] Assemble identifiable binaries, symbols, headers, licenses, provenance,
  compatibility claims, and test evidence.
- [ ] Validate the intended consumer boundary through the standalone host.
- [ ] Document integration steps, remaining risks, and a fallback strategy.

Exit criterion: the backend is ready for a separately authorized VT7
integration task. This milestone does not claim that integration has happened.

## Later work

After VT7 readiness, evaluate other consumers and an optional user-mode shim
for specific ConPTY clients. Process creation and attachment semantics are part
of that work, not just export names. Each supported client needs an explicit
contract and tests. No kernel driver is planned.

## Maintaining this roadmap

Check an item only with linked implementation and validation evidence. Keep
plans separate from results, record scope changes explicitly, and preserve
failed or partial historical evidence. Detailed procedures and measurements
belong in the supporting documents rather than accumulating here.
