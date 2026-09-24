# VT7Pty Testing Strategy

Status: approved Milestone 0 strategy; Step 0.8 accepted on both Windows 7 tiers.
Updated: 2026-09-24.

This document defines how VT7Pty distinguishes preserved WinPTY behavior, new
regressions, inherited limitations, and accepted improvements. Results must
identify the exact source and binaries under test.

## Principles

- Capture a baseline before removal, renaming, or intentional behavior changes.
- Add tests throughout modernization rather than postponing them to its end.
- Automate deterministic protocol and lifecycle checks where practical.
- Test observable terminal state when multiple byte encodings are equivalent.
- Keep development-host results separate from physical Windows 7 acceptance.
- Report every required case as pass, fail, skipped, or not run.
- Preserve failures and partial results; missing coverage is not a pass.
- Use negative controls to prove that the harness detects broken behavior.
- Keep credentials, private terminal content, and personal information out of
  default logs and published artifacts.

## Execution model

Verification uses repository-owned native executables and Windows PowerShell
scripts. There is no hosted build or test service.

The maintained entry point is `Verify-VT7Pty.ps1`. Tests that must run
on Windows 7 will work from a portable package without Visual Studio. The
target-side acceptance and diagnostic scripts are designed for the PowerShell
2.0 version included with Windows 7 SP1; development-host build tools retain their
modern PowerShell requirements.

| Layer | Purpose | Expected execution |
| --- | --- | --- |
| Unit | Encoding, buffers, mappings, protocol helpers, and deterministic algorithms | Development host; portable subset on targets where useful |
| Component | Client/agent control, fixtures, pipes, spawn, resize, and teardown | Development host and portable Windows 7 package |
| Contract | Public API ownership, errors, concurrency, cancellation, and version negotiation | Introduced during rebranding and completed in Milestone 2 |
| Application | Command Prompt, Windows PowerShell, and defined console applications | Development-host comparison and physical acceptance |
| Stress | Repeated lifecycle/resize, long sessions, resource stability, and output load | Bounded local and physical-machine runs |
| Manual | Keyboard layouts, interactive editing, repaint, resize, and screen restoration | Recorded physical-machine procedure |

## Baseline record

Before obsolete components are removed, preserve:

- exact source revision and working-tree state,
- compiler, SDK, architecture, configuration, runtime model, and build command,
- artifact names, sizes, hashes, imports, exports, and symbol identity,
- test command, timeout, exit status, and captured output,
- development-host OS information and separate target information,
- known failures, limitations, skipped cases, and reasons.

Roadmap Step 0.1 produced the accepted
[inherited native baseline](validation/2026-09-21-upstream-baseline.md) through
a temporary checked-in harness. It records Debug/Release artifacts, hashes,
imports, exports, warnings, and the two inherited test results. Step 0.2 must
reproduce that boundary through the supported MSBuild workflow. The original
ignored environment-check harness is no longer baseline evidence.

Roadmap Step 0.2 now provides the first maintained development-host command:

```powershell
.\Verify-VT7Pty.ps1
```

Release verification also runs the MSVC native code analyzer. It can be run
independently with `.\Analyze-VT7Pty.ps1`; diagnostics fail the build, just as
compiler warnings do under the maintained `/W4 /WX` policy.

It builds Debug and Release by default, runs unit, protocol, lifecycle,
application, agent-rejection, argument-quoting, and bounded-output checks. The
negative controls exercise a missing agent plus malformed, wrong-identity,
older, and newer protocol handshakes. It validates
architecture, subsystem floor, direct imports, the inherited DLL export
boundary, PDB presence, and generated artifact identity for all maintained
binaries. Results and dumpbin evidence are written below ignored
`artifacts/verification`. Verification also extracts every embedded manifest,
requires the Windows 7 through Windows 11 compatibility declarations, rejects
delay-loaded DLLs and a maintained set of post-Windows 7 direct imports, and
runs real Command Prompt and Windows PowerShell sessions after a resize. The
interactive console fixtures are built and inspected here but remain reserved
for focused tests that can provide their required console and input state.

Source and caller line resolution can be checked independently with:

```powershell
.\tools\debug\Verify-SourceDebugging.ps1
```

That check launches the Debug integration test under CDB, breaks at
`vt7pty_config_new`, and requires resolved locations in both `vt7pty.cc` and
`BackendSmokeTest.cc`. These checks establish build-transition parity only; they do
not satisfy the broader permanent suite or either physical Windows 7 tier.

The accepted clean-commit result is the
[2026-09-21 MSBuild transition record](validation/2026-09-21-msbuild-transition.md).
The added fixture/tool boundary and source-debugging proof are recorded in the
[2026-09-22 Step 0.2 completion record](validation/2026-09-22-step-0.2-completion.md).
The same boundary passed after obsolete infrastructure and compatibility code
were removed, as recorded in the
[2026-09-22 Step 0.3 completion record](validation/2026-09-22-step-0.3-completion.md).
Step 0.6 adds the warning-clean compiler/analyzer gate and a focused
`ModernCppTest` for the standard formatting and checked-narrowing replacements.
Step 0.8 adds `SessionFixture` and `SessionContractTest` as separate controlled
child and component executables. The default local command runs both Debug and
Release suites. A one-minute soak can be exercised locally with
`./Verify-VT7Pty.ps1 -Configuration Release -SoakMinutes 1`.

The normal suite verifies exact output order and exit status, console input,
Unicode output, 100 resizes under load, 500 repeated sessions after five warm-up
sessions, 20 live-session shutdowns, failed child creation, and four concurrent
sessions. It also verifies that deliberate output, status, order, truncation,
timeout, and handle-leak faults are detected. The agent compatibility controls
remain separate. `artifacts/verification/x64/<configuration>/tests.json` and
`tests.txt` retain each local case status even if a case fails before binary
inspection completes.

## Coverage matrix

| Area | Minimum coverage |
| --- | --- |
| Build and package | Clean checkout, Debug/Release, version generation, imports, exports, dependencies, symbols, manifests, notices, and hashes |
| Discovery and startup | Correct agent, missing agent, malformed or incompatible agent, hidden console, startup timeout, and early agent exit |
| Process and session | Application/command line, arguments, environment, working directory, exit status, launch failure, EOF, and simultaneous sessions |
| Input | ASCII, UTF-16, Enter/Tab/Backspace/Escape, navigation, function keys, modifiers, AltGr where available, processed/unprocessed Ctrl+C, and supported mouse modes |
| Output | Text and attributes, cursor movement, erase/clear, wrap, scroll, buffer changes, large and fragmented output, and final drain |
| Unicode | Surrogate pairs and isolated surrogates where relevant, wide characters, combining sequences, malformed input, and inherited width limitations |
| Resize | Approved sizes, repeated transitions, application-observed dimensions, output under resize, startup, and shutdown |
| Lifecycle | Normal exit, close while idle/busy, blocked I/O, parent/agent failure, descendants, handles/pipes, orphan detection, and repeated recreation |
| Interactive | Command Prompt and Windows PowerShell editing, echo, cancellation, repaint, resize, exit, and return to a stable prompt |
| Diagnostics | Enabled/disabled logging, source/version identity, bounded data, useful failure messages, and no secrets by default |
| Security boundaries | Pipe peer/permissions, handle inheritance, malformed IPC, path lookup, command quoting, bounds, and cleanup races |
| Stability/performance | Idle CPU, memory and handles, input response, output throughput, resize latency, repeated sessions, and long-running use |

Exact fixtures, expected terminal state, timeouts, target-specific skips, and
resource budgets are deliverables of the relevant implementation step.

## Physical Windows 7 matrix

| Candidate | Non-ESU Windows 7 SP1 x64 | ESU Windows 7 SP1 x64 | Legacy PowerShell 2.0 Windows 7 SP1 x64 |
| --- | --- | --- | --- |
| Routine development candidate | Required when target behavior or dependencies change | As needed for diagnosis | As needed for diagnosis |
| Milestone candidate from Step 0.9 | Required | Required | Required |
| Release candidate | Required | Required | Required |

Each run records hardware, OS build, installed-update state, package identity,
commands, per-case results, durations, logs, dumps, and deviations from the
previous accepted result.

The portable procedure for both current and PowerShell 2.0 installations is documented in
[Windows 7 Platform Acceptance](WINDOWS7_ACCEPTANCE.md). A clean-tree package
contains the runner, runtime binaries, native tests, fixtures, and a hash
manifest. Matching symbols are in the separate symbols archive. The same test
ZIP is run on all three machines.

## Initial stress profiles

These are starting acceptance workloads and may be revised with recorded
evidence:

- 500 output-checked create and close cycles following five warm-up sessions;
- 100 alternating 80x25 and 120x40 resizes during 200 numbered output rows;
- a 120-minute Windows 7 soak with five-second idle intervals, repeated resize,
  active output, agent/child exit checks, and a four-handle post-warm-up budget;
- bounded output-heavy runs for throughput and memory observation.

The portable milestone runner includes the 120-minute soak by default. Setting
`-SoakMinutes 0` provides a fast diagnostic run but records the soak as
`NotRun` and fails the Step 0.8 milestone preflight. A per-case timeout exceeds
the requested soak duration by two minutes. Output is bounded to one MiB per
session; the fixture load is 200 rows and the runner kills timed-out process
trees. The four-handle budget follows warm-up to exclude one-time runtime
initialization.

## Application and remote scenarios

Command Prompt and Windows PowerShell are required local acceptance cases.
Record exact versions and relevant configuration such as profiles or line-editing
modules.

SSH, Vim, and htop remain useful fidelity scenarios. The exact unresolved
dependency and the criteria for a Milestone 1 baseline are recorded in
[SSH and full-screen baseline dependency](SSH_BASELINE.md).

## Negative controls

At minimum, demonstrate that the suite fails when:

- the agent is absent or deliberately incompatible,
- a fixture returns incorrect output or status,
- a test exceeds its timeout,
- an assertion detects leaked or orphaned state,
- required output is truncated or reordered.

Deliberate crashes, hangs, and parent/agent termination probes run in an
isolated group with bounded cleanup.

## Result records

Each acceptance record includes:

- source commit, dirty-state description, version, and artifact hashes,
- configuration, compiler, SDK, dependencies, and package ID,
- host and target OS, update level, architecture, and hardware description,
- commands and fixture/application versions,
- per-case status, duration, exit code, and relevant measurements,
- log and dump locations with redaction notes,
- differences from the previous accepted baseline,
- known defects, untested areas, and the next decision enabled by the result.

Reusable procedures remain here. Immutable results will live under
`docs/validation/` and roadmap items will link to their evidence.

Step 0.7 also validates structured diagnostic record bounds and escaping in
`ProtocolTest.exe`, and validates the real debug-pipe ACL with:

```powershell
.\artifacts\bin\x64\Release\VT7Pty-DebugServer.exe --self-test
```

Physical failure collection follows [Diagnostics](DIAGNOSTICS.md). The
[native boundary security review](SECURITY_REVIEW.md) records which properties
are automated and which are architectural dispositions.
