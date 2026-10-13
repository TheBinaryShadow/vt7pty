# Testing VT7Pty

I use local native tests, portable Windows 7 packages and focused research
probes. They answer different questions, so a passing research probe does not
replace release acceptance. The [roadmap](../ROADMAP.md) records what is complete.

## Choose a test run

| Task | Entry point | Guide |
| --- | --- | --- |
| Verify a local build | Verify-VT7Pty.ps1 | Commands below |
| Test a release candidate | The candidate's -tests.zip | [Windows 7 acceptance](WINDOWS7_ACCEPTANCE.md) |
| Measure shell and console fidelity | Fidelity baseline package | [Runner guide](FIDELITY_BASELINE.md) |
| Check console-host identity | Host identity research package | [H1 probe](HOST_IDENTITY_PROBE.md) |
| Check host dispatch and isolation | Host dispatch research package | [H2 probe](HOST_DISPATCH_PROBE.md) |
| Collect failure details | Diagnostic tools | [Diagnostics](DIAGNOSTICS.md) |

The target runners support Windows PowerShell 2.0 without KB3191566.
Build-host commands use the tools in [Building](../BUILDING.md). I do not use
hosted CI.

## Local verification

From the repository root:

~~~powershell
.\Verify-VT7Pty.ps1
~~~

This builds and tests Debug and Release by default. Results and binary inspection
records go under artifacts/verification. Each configuration keeps tests.json
and tests.txt even when a case fails before binary inspection finishes.

The suite covers units, protocol validation, missing/incompatible agents, child
launch, quoting, output, console input, Unicode, resize, lifecycle, diagnostics
and deliberate faults. Inspection checks x64 images, subsystem 6.01, imports,
exports, PDBs, resources and embedded compatibility manifests. It rejects
delay-loaded DLLs and a maintained list of post-Windows 7 direct imports.
Release verification also runs static analysis under /W4 /WX.

The current local suite has 31 checks without the optional soak, including
the H1 and H2 logic checks. Historical release bundles retain their own counts;
the published 0.5.0 acceptance bundle has 30 cases including its full soak.

For a short local soak:

~~~powershell
.\Verify-VT7Pty.ps1 -Configuration Release -SoakMinutes 1
~~~

[Building](../BUILDING.md) covers standalone analysis and source debugging.
The [native security review](SECURITY_REVIEW.md) distinguishes automated checks
from design review. The live diagnostic-pipe descriptor can also be checked with:

~~~powershell
.\artifacts\bin\x64\Release\VT7Pty-DebugServer.exe --self-test
~~~

## How I assess results

I record the baseline before changing behavior, use deterministic tests where
possible, and compare terminal state when different byte streams can represent
the same result. Tests that deliberately inject incorrect output, status or
lifetime behavior must prove that the harness detects it.

Every required case keeps its actual Pass, Fail, Skipped or NotRun result.
Missing coverage stays open. I preserve partial runs and failures, and separate
development-host observations from Windows 7 results. Default logs must exclude
credentials and private terminal content.

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

## Windows 7 test matrix

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

## Stress profiles

I use these acceptance workloads and change them only with recorded evidence:

- 500 output-checked create and close cycles following five warm-up sessions;
- 100 alternating 80x25 and 120x40 resizes during 200 numbered output rows;
- a 120-minute Windows 7 soak with five-second idle intervals, repeated resize,
  active output, agent/child exit checks, and a four-handle post-warm-up budget;
- bounded output-heavy runs for throughput and memory observation.

The portable milestone runner includes the 120-minute soak by default. Setting
`-SoakMinutes 0` provides a fast diagnostic run but records the soak as
`NotRun` and fails milestone/release preflight. A per-case timeout exceeds
the requested soak duration by two minutes. Output is bounded to one MiB per
session; the fixture load is 200 rows and the runner kills timed-out process
trees. The four-handle budget follows warm-up to exclude one-time runtime
initialization.

## Fidelity and cancellation research

I use the [fidelity plan](FIDELITY.md) for the application corpus, expected state,
timing limits and remaining coverage. The
[baseline guide](FIDELITY_BASELINE.md) explains clean shell profiles, pinned
PowerShell versions and how to interpret expected fault-control results.
Marker output alone does not establish complete screen, cursor or attribute fidelity.

Ctrl+C cancellation remains unresolved in CMD and native Windows PowerShell.
The [investigation](INPUT_CANCELLATION.md) records failed public-API approaches.
The [console-host design](HOST_CANCELLATION_DESIGN.md) separates identity (H1),
dispatch (H2), cancellation (H3) and production integration (H4).
H1 and H2 are accepted; neither cancels a read or reruns the wider fidelity corpus.

Each research package has its own instructions and evidence. I keep the
released 0.5.0 regression suite and full Windows 7 soak as separate requirements.
[Console-SSH testing](SSH_BASELINE.md) is optional later work.

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

I keep the following with each acceptance record:

- source commit, dirty-state description, version, and artifact hashes,
- configuration, compiler, SDK, dependencies, and package ID,
- host and target OS, update level, architecture, and hardware description,
- commands and fixture/application versions,
- per-case status, duration, exit code, and relevant measurements,
- log and dump locations with redaction notes,
- differences from the previous accepted baseline,
- known defects, untested areas, and the next decision enabled by the result.

I keep reusable procedures here and dated reviews in the
[validation index](validation/README.md). Raw machine records retain their
original bytes; edits to review prose must preserve the measured results and
explain any factual correction.
