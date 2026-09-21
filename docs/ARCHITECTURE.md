# VT7Pty Architecture

Status: current native architecture and planned boundaries. Updated:
2026-09-22.

This document describes the maintained native architecture through Roadmap
Step 0.5. Source code remains authoritative when this overview and the
implementation disagree.

## Purpose

VT7Pty is a user-mode console backend for Windows 7 and later. It uses WinPTY's proven
legacy-console bridge as its foundation and uses ConPTY as a behavioral and API
reference. The primary intended consumer is VT7.

The backend must allow a terminal host to launch and control ordinary Windows
console applications without displaying the backing console window. It must
translate terminal input into console input records and reconstruct terminal
output from the legacy console state.

## Current components

| Component | Current role | Milestone 0 direction |
| --- | --- | --- |
| `VT7Pty.dll` / `src/libvt7pty` | Client API, agent launch, versioned control RPC, process requests, pipe discovery | Retain and modernize; public redesign comes later |
| `VT7Pty-Agent.exe` / `src/agent` | Owns the hidden console, launches the child, handles input, scrapes output, resizes, and manages session lifetime | Retain as the backend core |
| `src/shared` | Shared handles, buffers, security, protocol, encoding, and diagnostics | Retain required native code |
| `VT7Pty-DebugServer.exe` / `src/debugserver` | Collects timestamped diagnostic output | Retain and modernize |
| `src/tests`, `tests`, and native probes | Existing smoke tests and focused console investigations | Retain useful coverage and integrate it with the permanent test system |

The Cygwin/MSYS `winpty.exe` adapter and its Unix-facing source have been
removed. Active runtime and API identity is documented in [Technical Naming](NAMING.md).

## Current session flow

```text
Terminal host
    |
    | VT7Pty C API 1.0
    v
VT7Pty.dll
    |  starts agent and exchanges control messages
    |  exposes named data-pipe paths
    v
VT7Pty-Agent.exe
    |  owns hidden legacy console
    |  launches child process
    +-------------------------------+
    |                               |
terminal input                  console screen state
    |                               |
VT/input parser                polling and comparison
    |                               |
INPUT_RECORD values            VT output generation
    |                               |
WriteConsoleInputW                 output pipe
    |                               |
    +---------- legacy console -----+
                    |
              child application
```

The current API creates one session object, starts one agent, validates the
`VT7Pty-Agent` protocol identity and version, returns names for
the input/output pipes, permits one child spawn, supports resize and process-list
queries, and closes the control relationship when the session is freed. See
[vt7pty.h](../src/include/vt7pty.h) for the exact inherited contract.

## Process and handle ownership

Current ownership must be verified and captured by contract tests before an API
redesign. The inherited header documents these important rules:

- The session object owns its internal agent and control resources.
- The agent-process handle returned by the session is borrowed and must not be
  closed by the caller.
- Process and thread handles returned by a successful spawn are duplicated for
  the caller, which must close them.
- Pipe names remain valid until the session object is freed; callers open their
  own pipe handles.
- Freeing a session breaks its agent connection. Other threads must not still be
  using that session object.
- Spawn can be called once per session object.

The exact behavior of output draining, descendants, concurrent resize/close,
blocked I/O, agent failure, and partial startup belongs in the testing baseline
and the later public API contract. Documentation must not promise behavior that
has not been verified.

## Input path

The terminal host writes byte-oriented terminal input to the input pipe. The
agent parses keyboard and mouse sequences, maps modifiers and keys, creates
Win32 `INPUT_RECORD` values, and writes them to the backing console input
buffer. Console mode affects how applications receive and interpret those
records.

This path is valuable because legacy console applications expect Win32 console
events rather than a raw pseudoterminal byte stream. Fidelity work must cover
processed and unprocessed input modes, keyboard layouts, modifier combinations,
mouse modes, Unicode, and control events.

## Output path and fidelity boundary

The child application changes the backing console through writes or Win32
Console APIs. The agent polls the screen buffer, compares its state with the
previous state, and emits VT sequences describing the observed result.

This is reconstruction, not interception of the original application intent.
Information already reduced by the legacy console representation cannot always
be recovered by improving the emitter. Milestone 1 should preserve working
scraper behavior while testing whether a specific fidelity requirement needs an
augmentation or replacement at a narrower boundary.

## Resize and lifecycle

Resize is a control operation sent to the agent, which applies legacy console
window and buffer changes and notifies the console input path where required.
Legacy resize operations are ordering-sensitive and must be treated as part of
the session contract rather than a cosmetic operation.

The separate agent intentionally isolates console ownership and legacy console
state from the terminal host. Removing the agent is not a current goal. Clean
startup, output drain, child/descendant handling, console closure, agent exit,
and parent failure require explicit tests.

## Planned boundaries

Milestone 0 has replaced the build system, removed adapter-only and pre-Windows
7 code, and renamed the runtime. It continues by modernizing the retained C++
implementation without changing backend semantics without evidence.

Milestone 1 improves observable backend behavior against a tested application
corpus. Milestone 2 defines the VT7Pty integration API around the proven
backend. That API will borrow useful ConPTY conventions but must specify an
explicit Windows 7 process-launch operation and complete ownership, concurrency,
and shutdown semantics.

A future ConPTY user-mode shim is outside these milestones. It must address the
process-creation attachment contract as well as public function names.

## Architectural rules

- Keep the Windows 7 SP1 x64 floor visible at platform boundaries while
  preserving useful behavior on later Windows versions.
- Do not retain XP, Vista, x86, Unix-adapter, or obsolete toolchain paths.
- Retain the separate agent unless evidence supports a reviewed design change.
- Preserve observable behavior during build cleanup and technical renaming.
- Separate public API design from internal protocol and implementation details.
- Version the client API and client-agent protocol independently when they can
  evolve independently.
- Treat input, output, resize, and teardown as concurrent operations with stated
  ownership and cancellation behavior.
- Measure application-visible fidelity; do not infer it from successful launch.
- Keep VT7 integration requirements in view without modifying VT7 from this
  repository.

See the [roadmap](../ROADMAP.md), [testing strategy](TESTING.md),
[compatibility contract](COMPATIBILITY.md), and [versioning policy](VERSIONING.md)
for the gates around architectural changes.
