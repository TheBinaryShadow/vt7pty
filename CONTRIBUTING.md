# Contributing to VT7Pty

Thank you for helping improve the console backend for VT7 on Windows 7 and
later.
VT7Pty is in early development, and focused changes with reproducible evidence
are especially useful.

## Before starting

Read the [README](README.md),
[development foundation](docs/FOUNDATION.md), [roadmap](ROADMAP.md),
[architecture](docs/ARCHITECTURE.md),
[compatibility contract](docs/COMPATIBILITY.md), [build notes](BUILDING.md), and
[upstream record](UPSTREAM.md). Search the
[issue tracker](https://github.com/TheBinaryShadow/vt7pty/issues) before opening
a new report or proposal.

Discuss substantial API changes, backend replacements, component removals,
new dependencies, and compatibility shims before implementing them. Small
documentation corrections and focused fixes can go straight to a pull request.

## Project priorities

The primary goal is application-facing fidelity and reliability on Windows 7+,
followed by a clean integration boundary for eventual use in VT7. ConPTY is a
behavioral and architectural reference. Compatibility for unrelated consumers
is later work; a kernel driver is outside the project direction.

Useful contributions include:

- Minimal reproductions for input, output, Unicode, resize, and screen-restoration
  defects.
- Tests for process lifetime, cancellation, shutdown, and resource leaks.
- Reproducible local native builds and checks for Windows 7 API compatibility.
- Target-machine testing with clear environment and artifact details.
- Focused improvements that preserve known working legacy-console behavior.

The Unix adapter, obsolete toolchains, and pre-Windows 7 branches have been
removed after their replacement and regression gates passed. Relate a proposed
change to the relevant roadmap step and its acceptance criteria; draft plans
are not completed work.
Avoid mixing renames, removals, broad formatting changes, and functional fixes
in the same pull request. Work in this repository; changes to VT7 itself belong
in that project's own review process.

## Windows 7 and validation

Windows 7 SP1 x64 is the minimum target; Windows 8 through Windows 11 x64 remain
in scope. XP, Vista, and x86 are unsupported. An API's presence in a modern SDK
does not establish its availability or behavior on Windows 7. Check imports,
runtime dependencies, flags, and lifetime assumptions when they change.

For behavior changes, describe the expected and observed result and run the
relevant existing tests. Add focused regression coverage when practical.
Documentation-only changes normally need link and consistency checks rather
than a native rebuild.

Record the source revision, compiler/toolset, SDK, architecture, build
configuration, Windows version and update level, affected application and its
version, and exact test steps. Identify local development results separately
from Windows 7 results. State what was not tested and retain evidence of
remaining failures.

Follow the result and artifact-identification requirements in
[Testing](docs/TESTING.md). Changes to package, public API, ABI, or agent
protocol identity must also update [Versioning](docs/VERSIONING.md) and the
[VT7Pty changelog](CHANGELOG.md).

Remove credentials, private terminal content, and personal information from
logs and crash reports. Report suspected vulnerabilities through
[SECURITY.md](SECURITY.md), not a public issue.

## Source and attribution

Follow the approved C++20 repository style and keep maintained targets clean
under `/W4 /WX` and Release static analysis. Do not extend retired compiler,
Unix-adapter, XP, or Vista paths.
Explain why a remaining compatibility workaround exists and preserve relevant
historical context.

Keep WinPTY's copyright notices and Git authorship intact. For third-party
code, identify the project, exact source URL and revision, applicable license,
and what was adapted. Preserve required notices and update
[UPSTREAM.md](UPSTREAM.md) and [CREDITS.md](CREDITS.md) as appropriate. Credit
design references too, while distinguishing them from code actually included.

## Pull requests

A useful pull request explains:

- The concrete problem and resulting behavior.
- Why the change fits the Windows 7 backend and VT7 integration goals.
- Compatibility assumptions and any new dependencies.
- Validation performed, results, and remaining limitations.
- Upstream provenance for reused material.

Keep changes reviewable and commit subjects descriptive. Draft pull requests
are welcome for bounded experiments; label unproven conclusions clearly.

## License and community

VT7Pty uses the [MIT License](LICENSE). By submitting a contribution, you agree
to provide it under that license and confirm that you have the right to do so.
Third-party material must retain its applicable license and notices.

Be patient, specific, and respectful. The [Code of Conduct](CODE_OF_CONDUCT.md)
applies throughout the project.
