# VT7Pty Testing Strategy

Status: proposed Milestone 0 strategy. Updated: 2026-09-21.

This document defines how VT7Pty will distinguish preserved WinPTY behavior,
new regressions, inherited limitations, and accepted improvements. Test results
must identify the source and binary under test; a plan is not evidence that a
capability works.

## Principles

- Capture a baseline before removal, renaming, or behavior changes.
- Automate deterministic protocol and lifecycle checks where practical.
- Test observable terminal state rather than requiring one exact VT byte
  encoding when multiple encodings are equivalent.
- Keep development-host results separate from Windows 7 acceptance.
- Report every required case as pass, fail, skipped, or not run.
- Preserve failures and partial results; do not turn missing coverage into a pass.
- Use negative controls to prove that the harness detects broken behavior.
- Keep credentials, private terminal content, and personal information out of
  default logs and published artifacts.

## Test layers

| Layer | Purpose | Expected execution |
| --- | --- | --- |
| Unit | Encoding, buffers, mappings, protocol helpers, and deterministic algorithms | Development host and CI |
| Component | Client/agent control, fixtures, pipe behavior, spawn, resize, and teardown | Development host, CI where suitable, Windows 7 package |
| Contract | Public API ownership, errors, concurrency, cancellation, and version negotiation | Added with Milestone 2 |
| Application | Command Prompt, Windows PowerShell, and defined console applications | Development host comparison and Windows 7 acceptance |
| Stress | Repeated lifecycle/resize, long sessions, resource stability, output load | Bounded local runs and Windows 7 acceptance |
| Manual | Visible console behavior, keyboard layouts, interactive editing, repaint and screen restoration | Recorded target-machine procedure |

## Baseline record

Before Step 0.2 begins, preserve:

- Exact source revision and working-tree state.
- Compiler, SDK, architecture, configuration, runtime model, and build command.
- Artifact names, sizes, hashes, imports, exports, and symbol identity.
- Test command, timeout, exit status, and captured output.
- Development-host OS information and separate Windows 7 target information.
- Known failures, limitations, skipped cases, and reasons.

The temporary ignored environment-check harness is useful evidence that local
tools work, but it is not the reproducible baseline for a fresh clone. Step 0.1
must produce the supported baseline through repository-owned commands.

## Milestone 0 coverage matrix

| Area | Minimum coverage |
| --- | --- |
| Build and package | Fresh checkout, Debug/Release, generated version, imports, exports, runtime dependencies, symbols, manifest and notices |
| Discovery and startup | Correct agent, missing agent, wrong/incompatible agent, hidden console behavior, startup timeout and early agent exit |
| Process and session | Application name/command line, arguments, environment, working directory, exit code, launch failure, EOF, simultaneous sessions |
| Input | ASCII, UTF-16/Unicode cases, Enter/Tab/Backspace/Escape, navigation and function keys, modifiers, AltGr where available, processed/unprocessed Ctrl+C, supported mouse modes |
| Output | Text and attributes, cursor movement, erase/clear, wrap, scroll, alternate or active buffer changes, large and fragmented output, final drain |
| Unicode | Surrogate pairs and isolated surrogates where relevant, wide characters, combining sequences, malformed input, inherited width limitations |
| Resize | Approved size set, repeated small/large transitions, application-observed dimensions, resize during output, startup and shutdown |
| Lifecycle | Normal exit, close while idle/busy, blocked I/O, parent/agent failure, descendants, handles/pipes, orphan detection, repeated recreation |
| Interactive | Command Prompt and Windows PowerShell editing, echo, cancellation, repaint, resize, exit and return to a stable prompt |
| Diagnostics | Enabled/disabled logging, source/version identity, bounded data, useful failure messages, no secrets by default |
| Stability/performance | Idle CPU, memory and handles, input response, output throughput, resize latency, repeated sessions and long-running use |

Exact fixtures, expected terminal state, timeouts, and platform-specific skips
will be added before Step 0.3 implementation.

## Proposed stress profiles

These values are starting proposals and are not current results:

- At least 500 create, minimally interact, and close cycles.
- At least 100 alternating small/large resize cycles under activity.
- A two-hour session on the Windows 7 acceptance target, with defined idle and
  active intervals.
- Bounded output-heavy runs for throughput and memory observation.

Before accepting these profiles, define warm-up treatment, sampling intervals,
process-tree cleanup, timeouts, resource budgets, and what constitutes material
growth. Failed limits remain failures until the limit or implementation is
changed through an explicit reviewed decision.

## Application and remote scenarios

Command Prompt and Windows PowerShell are required local acceptance cases.
Record exact versions and relevant configuration such as PowerShell profiles or
line-editing modules.

SSH, Vim, and htop remain valuable fidelity scenarios, but the test cannot be
specified honestly until the project identifies:

- The Windows 7-compatible SSH client and version.
- Whether the client uses console APIs, inherited streams, or another path.
- The terminal host or state model used to judge emitted output.
- The remote OS, shell, `TERM`, locale, application versions, and PTY geometry.
- Authentication setup that can be exercised without publishing credentials.

Milestone 0 should record a baseline or a precise unresolved dependency.
End-to-end remote fidelity becomes a Milestone 1 gate once this path is fixed.

## Negative controls

At minimum, demonstrate that the suite fails when:

- The agent is absent or deliberately incompatible.
- A fixture returns incorrect output or exit status.
- A test exceeds its timeout.
- An assertion detects leaked/orphaned state.
- Required output is truncated or reordered by an injected fixture.

Deliberate crashes, hangs, and parent/agent termination probes must run in an
isolated test group with bounded cleanup. They should not make routine CI
unreliable or leave processes behind.

## Result records

Each acceptance record should include:

- Source commit, dirty-state description, version, and artifact hashes.
- Build configuration, compiler, SDK, runtime dependencies, and package ID.
- Host and target OS, update level, architecture, and VM/hardware description.
- Commands and fixture/application versions.
- Per-case status, duration, exit code, and relevant measurements.
- Log locations and redaction notes.
- Differences from the previous accepted baseline.
- Known defects, untested areas, and the next decision enabled by the result.

Roadmap items link to these records when completed. Results should live under a
future `docs/validation/` directory, while reusable procedures stay here.
