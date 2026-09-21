# VT7Pty Roadmap

Status: draft for discussion. Updated: 2026-09-21.

This file owns milestone scope and completion. It records intended work, not
implemented capability. Milestones follow technical dependencies rather than
calendar dates.

An unchecked item (`[ ]`) is planned, in progress, or awaiting validation. A
checked item (`[x]`) has met its stated scope and has linked evidence. Partial
implementation remains unchecked. A local pass cannot complete an item that
requires Windows 7 acceptance.

Detailed design and evidence live in the linked documents:

- [Architecture](docs/ARCHITECTURE.md)
- [Testing strategy](docs/TESTING.md)
- [Compatibility contract](docs/COMPATIBILITY.md)
- [Versioning policy](docs/VERSIONING.md)
- [Upstream provenance](UPSTREAM.md)

## Goal and boundaries

VT7Pty will provide a reliable user-mode console backend for Windows 7, improve
on WinPTY's application-facing fidelity, and prepare a clean integration path
for VT7.

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
- Runtime names, public APIs, version metadata, build scripts, and components
  remain inherited from WinPTY, including the Unix adapter.
- A temporary local harness has built and debugged the x64 native components
  and passed the existing process/output and StringBuilder smoke tests. This is
  development-host evidence, not a supported build path or Windows 7 acceptance.

Completed repository foundation:

- [x] Establish the VT7Pty name, purpose, and independent-project identity.
- [x] Record the WinPTY baseline and preserve upstream license and attribution.
- [x] Add contribution, conduct, security, build, and maintainer guidance.
- [x] Establish this roadmap and its supporting documentation structure.

These items do not complete the build, technical rebranding, or testing work.

## Milestone overview

| Milestone | Outcome | Status |
| --- | --- | --- |
| 0 | Modern build, native VT7Pty identity, and tested 0.5.x foundation | Proposed |
| 1 | Measured backend fidelity and reliability improvements | Proposed |
| 2 | Documented VT7Pty API and standalone integration host | Proposed |
| 3 | Versioned backend handoff ready for separately authorized VT7 integration | Proposed |
| Later | Other consumers and optional ConPTY user-mode compatibility | Deferred |

Milestone 0 is specified below. Later milestones remain directional until its
baseline and test results identify the actual engineering priorities.

## Milestone 0 prerequisites

Resolve these decisions before implementation changes begin:

- [ ] Approve the CMake/MSVC build direction and choose minimum tool versions.
- [ ] Define the Windows 7 update floor, CRT deployment model, and available
  target test environment in [Compatibility](docs/COMPATIBILITY.md).
- [ ] Approve the keep/remove/replace inventory for inherited components.
- [ ] Approve the old-to-new naming map and the package/API/protocol versioning
  rules in [Versioning](docs/VERSIONING.md).
- [ ] Approve the Milestone 0 test matrix, workloads, budgets, and inherited
  failure register in [Testing](docs/TESTING.md).

## Milestone 0: Establish the modern VT7Pty foundation

The order is deliberate: create a supported build and baseline checks, remove
obsolete infrastructure, complete technical rebranding, then run comprehensive
validation. Cleanup, renaming, and versioning are separate checkpoints so a
regression can be attributed to one class of change.

### Step 0.1: Modernize build and auxiliary infrastructure

Outcome: a fresh checkout can build, test, package, and debug the retained
native components through documented, repeatable commands.

- [ ] Inventory source components, build paths, generated files, dependencies,
  diagnostics, packaging responsibilities, and existing tests.
- [ ] Preserve an identifiable upstream-baseline artifact and result record.
- [ ] Add CMake as the supported native build with checked-in configure, build,
  and test presets.
- [ ] Build x64 Debug and Release artifacts with symbols and a usable Visual
  Studio debugging workflow. Preserve x86 source paths without claiming support.
- [ ] Replace Python 2/GYP as the supported native build route and provide
  repository-owned version generation.
- [ ] Add fork-owned CI that uses the documented commands and clearly separates
  hosted-runner evidence from Windows 7 evidence.
- [ ] Produce an identifiable portable test package with binaries, symbols,
  notices, manifest, source revision, toolchain details, and hashes.
- [ ] Automate the inherited process/output and suitable unit smoke tests before
  any component removal or runtime renaming.
- [ ] Verify source-level debugging and inspect imports and runtime dependencies.
- [ ] Obtain a Windows 7 startup/session smoke result for the selected runtime
  configuration and record inherited failures and untested behavior.

Keep runtime behavior, names, and components unchanged during this step. Modern
build tools may run on a newer host, but emitted binaries and target-side test
tools must meet the approved Windows 7 contract.

Exit criterion: the supported build works from a fresh checkout, baseline tests
are automated, artifacts are identifiable, debugging works, and the selected
runtime configuration has a recorded Windows 7 smoke result.

### Step 0.2A: Remove audited obsolete components

Outcome: the repository retains the native backend and useful engineering tools
without carrying unsupported adapter-only infrastructure.

- [ ] Approve and publish the keep/remove/replace inventory.
- [ ] Remove `src/unix-adapter` and the Cygwin/MSYS `winpty.exe` frontend.
- [ ] Remove adapter-only wrappers, toolchain configuration, and packaging paths.
- [ ] Remove superseded Python 2/GYP, CI, build, and helper paths only after
  their retained responsibilities have replacements from Step 0.1.
- [ ] Audit diagnostics and experiments individually; retain useful native
  tests and investigation tools regardless of age or origin.
- [ ] Build, package, and rerun baseline checks on the development host and
  Windows 7, with no unexplained behavior or dependency changes.

Exit criterion: every removal is justified by the inventory, retained targets
build and run without adapter dependencies, attribution remains intact, and
baseline behavior has a recorded comparison.

### Step 0.2B: Complete technical rebranding

Outcome: retained artifacts and active interfaces have an internally consistent
VT7Pty identity while historical attribution keeps the WinPTY name.

- [ ] Finalize and publish the old-to-new naming map.
- [ ] Rename the proposed native artifacts to `VT7Pty.dll`,
  `VT7Pty-Agent.exe`, and `VT7Pty-DebugServer.exe`.
- [ ] Update active headers, exports, source/build identifiers, diagnostics,
  agent lookup, environment variables, IPC/debug endpoints, package metadata,
  tests, and documentation consistently.
- [ ] Document the source and binary compatibility break. Do not advertise
  interchangeability with WinPTY.
- [ ] Verify historical names remain in copyright notices, credits, upstream
  links, preserved documentation, and provenance records.
- [ ] Build and package all retained targets and verify exports, agent discovery,
  dependencies, failures for missing/wrong agents, and notices.
- [ ] Rerun baseline checks on the development host and Windows 7.

This step renames the inherited API without redesigning its behavior. The
public integration API is Milestone 2 work.

Exit criterion: every active project-owned identity follows the approved map,
the DLL and agent agree on names and endpoints, packaging is complete, and no
unexplained baseline regression remains.

### Step 0.2C: Establish the VT7Pty 0.5.x development line

Outcome: version metadata clearly distinguishes VT7Pty from upstream WinPTY.

- [ ] Change the development version to `0.5.0-dev` using one authoritative
  version source and consistent generated metadata.
- [ ] Add independent API and agent-protocol version fields where required by
  the approved [versioning policy](docs/VERSIONING.md).
- [ ] Start VT7Pty history in [CHANGELOG.md](CHANGELOG.md) without rewriting
  upstream tags or [WinPTY release history](RELEASES.md).
- [ ] Verify file metadata, diagnostics, package manifests, and test reports
  identify both the VT7Pty version and source revision.

Exit criterion: no active artifact reports itself as an upstream WinPTY release,
and package, API, protocol, and source identities are unambiguous.

### Step 0.3: Implement and run the testing system

Outcome: repeatable evidence that modernization, cleanup, and rebranding
preserved a solid backend, plus a regression suite for future fidelity work.

- [ ] Finalize expected outcomes, timeouts, resource budgets, target tiers, and
  result format in [Testing](docs/TESTING.md).
- [ ] Implement the native host, controlled child fixtures, automated assertions,
  and CTest entry point, reusing useful upstream tests and probes.
- [ ] Cover build/package identity, spawn and session behavior, input, output,
  Unicode, resize, lifecycle, interactive shells, and failure paths.
- [ ] Prepare a standalone Windows 7 test package and manual instructions.
- [ ] Run the automated and manual matrix on the development host and Windows 7.
- [ ] Run the approved lifecycle, resize, idle, active, and performance workloads.
- [ ] Verify negative controls and timeout behavior; keep destructive fault
  probes separate from routine CI.
- [ ] Record the SSH/full-screen baseline or the exact unresolved dependency.
- [ ] Resolve regressions and core-session blockers. Preserve reproducible
  inherited limitations as Milestone 1 inputs.

Exit criterion: all required cases report pass, fail, skipped, or not run;
results and artifact identities are published; no unexplained crash, hang, data
loss, orphan, or material resource regression remains.

### Milestone 0 acceptance

- [ ] Accept Step 0.1 with linked build, debug, baseline, and Windows 7 evidence.
- [ ] Accept Step 0.2A with linked removal and regression evidence.
- [ ] Accept Step 0.2B with linked naming and packaging evidence.
- [ ] Accept Step 0.2C with linked version and identity evidence.
- [ ] Accept Step 0.3 with linked test results and defect dispositions.

`0.5.0-dev` remains a development identity until acceptance. Creating a
`0.5.0` tag or release is a separate publication decision. Milestone 0 does not
claim complete terminal fidelity, VT7 integration, or ConPTY compatibility.

## Milestone 1: Improve backend fidelity and reliability

- [ ] Define the application corpus and measurable fidelity criteria from the
  Milestone 0 baseline, including local shells and the identified SSH path.
- [ ] Prioritize and improve input semantics, output/state reconstruction,
  Unicode preservation, resize, screen restoration, and lifecycle behavior.
- [ ] Add regression coverage and Windows 7 evidence with each accepted change.
- [ ] Where screen scraping cannot preserve required information, prove an
  adaptation or record an explicit scope decision.
- [ ] Publish tested capabilities, limitations, stability, and performance.

Exit criterion: agreed application workflows meet their documented Windows 7
fidelity and reliability criteria with no unexplained baseline regression.
Complete ConPTY equivalence and perfect Unicode behavior are not blanket gates.

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

Exit criterion: the backend is ready for a separately authorized VT7 integration
task. This milestone does not claim that integration has happened.

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
