# VT7Pty Testing Strategy

Status: approved Milestone 0 strategy. Updated: 2026-09-21.

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

The planned maintained entry point is `Verify-VT7Pty.ps1`. Tests that must run
on Windows 7 will work from a portable package without Visual Studio. Their
orchestration will remain compatible with Windows PowerShell 5.1.

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

The ignored local environment-check harness proves that the present workstation
can build and debug the inherited x64 components. It is not the clean-checkout
baseline. Roadmap Steps 0.1 and 0.2 replace it with maintained commands.

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

| Candidate | Non-ESU Windows 7 SP1 x64 | ESU Windows 7 SP1 x64 |
| --- | --- | --- |
| Routine development candidate | Required when target behavior or dependencies change | As needed for diagnosis |
| Milestone candidate | Required | Required |
| Release candidate | Required | Required |

Each run records hardware, OS build, installed-update state, package identity,
commands, per-case results, durations, logs, dumps, and deviations from the
previous accepted result.

## Initial stress profiles

These are starting acceptance workloads and may be revised with recorded
evidence:

- at least 500 create, minimally interact, and close cycles,
- at least 100 alternating small/large resize cycles under activity,
- a two-hour Windows 7 session with defined idle and active intervals,
- bounded output-heavy runs for throughput and memory observation.

Define warm-up treatment, sampling intervals, cleanup, timeouts, and acceptable
resource growth before treating a profile as an acceptance gate.

## Application and remote scenarios

Command Prompt and Windows PowerShell are required local acceptance cases.
Record exact versions and relevant configuration such as profiles or line-editing
modules.

SSH, Vim, and htop remain useful fidelity scenarios. Milestone 0 will either
record a reproducible baseline or document the missing dependency, including
the Windows 7-compatible SSH client, data path, remote OS and shell, `TERM`,
locale, application versions, geometry, and credential-safe setup. End-to-end
remote fidelity becomes a Milestone 1 gate once that path is fixed.

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
