# VT7Pty Changelog

This file records VT7Pty changes after the WinPTY `0.4.4-dev` baseline. It does
not replace or rewrite the preserved [WinPTY release history](RELEASES.md).

VT7Pty has not published a release. Planned roadmap items are not listed as
changes until implemented.

## Unreleased

### Documentation

- Established the VT7Pty project identity and direction while preserving all
  inherited source components and runtime names.
- Added upstream provenance, credits, build notes, contribution guidance, code
  of conduct, security policy, roadmap, and supporting architecture, testing,
  compatibility, and versioning documents.
- Approved and documented the full Milestone 0 modernization foundation:
  Windows 7 SP1+ x64, Visual Studio/MSBuild, C++20, local-only verification,
  physical non-ESU/ESU acceptance, manual releases, and removal of inherited
  Unix, pre-Windows 7, and obsolete toolchain paths.
- Completed the inherited component inventory and captured a reproducible x64
  Debug/Release native baseline with artifact hashes, imports, exports, warning
  debt, and passing inherited tests.
- Added the first supported VS2022/MSBuild x64 solution with centralized v143,
  C++20, SDK 26100, Windows 7, static CRT, symbol, manifest, security, and
  generated version-resource settings.
- Added maintained local build, verification, and transition-package commands;
  verification preserves the inherited export/import boundary and runs the
  inherited native lifecycle tests.
- Documented the 30-case permanent test system and its acceptance on both
  Windows 7 tiers, including a 120-minute soak on each machine.
- Defined the manual release archive layout, exact package identity, physical
  evidence review, and separate publication decision.
- Extended release-candidate acceptance to a third Windows 7 SP1 machine using
  PowerShell 2.0 without KB3191566, with a shared 30-case target suite.
- Preserved the original WinPTY README and release history as historical
  documentation.

### Implementation

- Modernized the retained implementation around C++20 standard formatting,
  checked integer conversion, typed byte spans, scoped enums, explicit null
  handling, and stronger RAII/move ownership for handles, modules, pipes,
  allocations, and security data.
- Removed the inherited string-builder, formatting, exception-indirection,
  compiler, architecture, integer, and header compatibility shims that no
  longer serve the supported MSVC toolchain.
- Reorganized unit and integration tests and the native debug server under the
  maintained `tests` and `tools` boundaries, and added a focused modern-C++
  utility test.
- Resolved the inherited warning baseline, enabled `/W4 /WX`, and added a
  repeatable MSVC native static-analysis gate to Release verification.
- Accepted the exact Step 0.6 candidate on both Windows 7 tiers, with the
  owner-approved `NESSY` environment disposition recorded in the validation
  evidence.

- Established the `0.5.0-dev` VT7Pty technical identity across native
  artifacts, projects, headers, 19 exported C functions, constants,
  diagnostics, environment variables, named endpoints, packaging, and tests.
- Added public API version 1.0 and client-agent protocol version 1 with an
  explicit `VT7Pty-Agent` handshake. Missing, malformed, wrong-identity,
  older, and newer agents now fail with specific actionable errors.
- Added protocol unit and incompatible-agent negative-control tests and record
  package, API, protocol, and source identities in verification and packages.
- Accepted the exact Step 0.5 candidate on both declared physical Windows 7
  tiers, with the owner-approved evidence treatment recorded in the validation
  record.

- Removed the Cygwin/MSYS adapter, GYP, GNU Make, Python 2 packaging,
  AppVeyor, old generated-file helpers, and obsolete shell/batch build paths.
- Classified and removed `misc`: native fixtures, manual probes, console tools,
  and historical research now have maintained locations.
- Replaced retired compiler shims with C++20/MSVC facilities, including
  standard formatting, `noexcept`, defaulted moves, and `std::mutex`.
- Renamed the Unix-oriented control-character helper for its retained Windows
  input role and enabled MSVC's conforming preprocessor.
- Preserved the inherited runtime behavior, 19-function DLL boundary, direct
  dependency sets, and source-level debugging through the cleanup.
- Added controlled console fixtures and component tests for input, Unicode,
  output, resize under load, repeated sessions, shutdown, concurrent sessions,
  soak, resource stability, and deliberate failures.
- Split the clean Release packaging workflow into runtime, development,
  symbols, and self-contained Windows 7 test archives with per-file manifests,
  extracted-archive verification, a release-set manifest, and checksums.
