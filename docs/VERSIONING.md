# VT7Pty Versioning Policy

Status: approved for Milestone 0. Updated: 2026-09-21.

VT7Pty keeps separate identities for its package, public API, internal
client-agent protocol, and exact source. One number cannot describe all four
without making compatibility ambiguous.

## Current state

`VERSION.txt` still contains the inherited WinPTY version `0.4.4-dev`. Current
binaries, symbols, headers, environment variables, and protocol identifiers
still use WinPTY names. VT7Pty has not published a release or promised API/ABI
stability.

Changing the version and technical identity is Roadmap Step 0.5. The approved
foundation does not pretend those implementation changes already exist.

## Package version

The approved policy is:

- begin technical rebranding with `0.5.0-dev`,
- use `MAJOR.MINOR.PATCH` for published versions and a clear development suffix
  between releases,
- treat the 0.5.x line as pre-stable, with no general source, API, ABI, or
  protocol compatibility promise unless release notes state one,
- use one authoritative package-version source to generate file metadata,
  diagnostics, manifests, and package names,
- publish `0.5.0` only after Milestone 0 acceptance and a separate release
  review.

Advancing a package number records project evolution. It does not alone prove
compatibility or milestone completion.

## Public API and ABI

The inherited WinPTY C interface will be renamed during Roadmap Step 0.5. The
approved technical naming convention is `vt7pty_*` for functions and
`VT7PTY_*` for constants and macros. VT7Pty will not ship WinPTY compatibility
aliases as part of the initial product.

The rename is an intentional source and binary compatibility break. It does not
imply compatibility with upstream WinPTY or ConPTY.

Before a stable VT7-facing ABI is declared, the project will:

- define compile-time API version macros,
- specify calling convention, structure versioning, ownership, encoding,
  errors, threading, cancellation, and lifetime behavior,
- export functions through a reviewed explicit list,
- test the exported surface and incorrect-version behavior,
- state the source and binary compatibility policy for each release.

The numeric API version representation remains a Milestone 2 design decision.

## Client-agent protocol

The DLL and agent are distributed together but communicate across a process
boundary. A VT7Pty client must never connect silently to an incompatible
WinPTY or VT7Pty agent.

Milestone 0 will:

- assign the control protocol an explicit numeric version,
- validate VT7Pty identity and version before ordinary RPC,
- fail quickly with a specific diagnostic on incompatibility,
- test correct, missing, older, newer, malformed, and wrong-identity agents,
- version IPC and debug endpoints where needed to avoid collisions.

Protocol versions need not advance with every package patch. Release notes will
record the package-to-protocol mapping. Exact negotiation fields and error codes
are implementation decisions that must satisfy these rules.

## Windows file versions

Published `MAJOR.MINOR.PATCH` versions map to the first three numeric Windows
file-version components. The fourth component is a repository-owned build or
development value generated consistently by the Milestone 0 build.

Development artifacts must visibly report the `-dev` package identity in
product metadata and diagnostics even though Windows numeric version resources
cannot encode the suffix. The exact fourth-component generation rule will be
documented with the MSBuild implementation and must be reproducible.

## Source and artifact identity

Every test package and diagnostic report identifies:

- VT7Pty package version,
- source commit and dirty state,
- API version once introduced,
- client-agent protocol version once introduced,
- architecture and build configuration,
- compiler/toolset and Windows SDK,
- artifact hashes.

A locally rebuilt artifact is not an issued package merely because its source
version string matches.

## Foundation and release tags

The annotated `milestone-0-start` tag marks the approved documentation commit
from which implementation begins. It is not a release tag.

Published release tags will use the project's version identity and will be
created only through the [manual release review](RELEASING.md). No `0.5.0`
release tag exists at the foundation.

## Upstream history

Keep WinPTY tags, copyright notices, Git authorship, the preserved README, and
[RELEASES.md](../RELEASES.md) intact. VT7Pty changes belong in
[CHANGELOG.md](../CHANGELOG.md). Future upstream imports record their exact
source revision and resulting VT7Pty version without rewriting history.
