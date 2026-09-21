# VT7Pty Versioning Policy

Status: proposed for approval before Roadmap Step 0.2. Updated: 2026-09-21.

VT7Pty needs separate identities for its package, public API, internal
client-agent protocol, and exact source. Treating one number as all four would
make compatibility ambiguous.

## Current state

`VERSION.txt` contains the inherited WinPTY version `0.4.4-dev`. Current
binaries, symbols, headers, environment variables, and protocol identifiers
still use WinPTY names. VT7Pty has not published a release or promised API/ABI
stability.

Documentation that mentions `0.5.0-dev` describes the proposed first VT7Pty
development line. The version file has not been changed yet.

## Package version

Proposed policy:

- Begin technical rebranding with `0.5.0-dev`.
- Use `MAJOR.MINOR.PATCH` for published versions and a clear development suffix
  between releases.
- The 0.5.x line is pre-stable. Source, API, ABI, and protocol compatibility may
  change unless a release note explicitly promises otherwise.
- Advancing a number records project evolution; it does not by itself promise
  compatibility or completion of a roadmap milestone.
- A `0.5.0` tag/release is a separate decision after Milestone 0 acceptance.

Keep one authoritative package-version source and generate file metadata,
diagnostics, manifests, and package names from it.

## Public API and ABI

The inherited WinPTY interface will be renamed during Step 0.2B without an
interchangeability promise. Milestone 2 will define the VT7 integration API.

Before freezing that API:

- Add compile-time API version macros.
- Define calling convention, structure size/version rules, ownership, encoding,
  errors, threading, cancellation, and lifetime behavior.
- Export functions deliberately through a reviewed definition or equivalent
  list, and test the export surface.
- State whether a release maintains source compatibility, binary compatibility,
  or neither with earlier VT7Pty versions.
- Do not imply compatibility with upstream WinPTY or ConPTY from similar names.

A future stable ABI can use a major API version and size-tagged structures or
explicit version negotiation. The exact mechanism remains a Milestone 2 design
decision.

## Client-agent protocol

The DLL and agent are distributed together but communicate across a process
boundary. Technical renaming must not allow a VT7Pty client to connect silently
to an incompatible WinPTY or VT7Pty agent.

Proposed requirements:

- Give the control protocol an explicit numeric version.
- Exchange or validate identity/version during startup before ordinary RPC.
- Fail quickly with a specific diagnostic on incompatibility.
- Test correct, missing, older, newer, and malformed agent cases.
- Version IPC/debug endpoint names where needed to avoid cross-version
  collisions while preserving actionable diagnostics.

Protocol version changes need not match every package patch. Record the mapping
between package versions and protocol versions in release notes.

## Source and artifact identity

Every test package and diagnostic report should identify:

- VT7Pty package version.
- Source commit and dirty state.
- API version, once introduced.
- Agent protocol version, once introduced.
- Architecture and build configuration.
- Compiler/toolset and Windows SDK.
- Artifact hashes.

Do not label a locally rebuilt artifact as an issued package solely because its
source version string matches.

## Upstream history

Keep WinPTY tags, copyright notices, Git authorship, preserved README, and
[RELEASES.md](../RELEASES.md) intact. VT7Pty changes belong in
[CHANGELOG.md](../CHANGELOG.md). If upstream code is imported later, record the
exact source revision and resulting VT7Pty version without rewriting history.

## Decisions required

- [ ] Approve `0.5.0-dev` as the first technical-rebranding version.
- [ ] Approve the absence of general API/ABI stability promises during 0.5.x.
- [ ] Choose the numeric API-version representation before Milestone 2.
- [ ] Choose the protocol negotiation and incompatible-agent error contract.
- [ ] Define Windows file-version mapping for development suffixes.
