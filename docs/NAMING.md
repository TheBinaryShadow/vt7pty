# VT7Pty Technical Naming

Status: implemented in Roadmap Step 0.5. Updated: 2026-09-22.

VT7Pty deliberately uses its own technical identity. The rename from the
inherited WinPTY surface is a source and binary compatibility break. No WinPTY
compatibility headers, exported aliases, duplicate binaries, environment
variable aliases, or endpoint aliases are shipped.

## Naming map

| Inherited name | VT7Pty name |
| --- | --- |
| `winpty.dll`, `winpty.lib`, `winpty.pdb` | `VT7Pty.dll`, `VT7Pty.lib`, `VT7Pty.pdb` |
| `winpty-agent.exe` | `VT7Pty-Agent.exe` |
| `winpty-debugserver.exe` | `VT7Pty-DebugServer.exe` |
| `projects/winpty*.vcxproj` | `projects/VT7Pty*.vcxproj` |
| `src/libwinpty` | `src/libvt7pty` |
| `src/include/winpty.h` | `src/include/vt7pty.h` |
| `src/include/winpty_constants.h` | `src/include/vt7pty_constants.h` |
| no public API-version header | `src/include/vt7pty_version.h` |
| `winpty_*` C functions and types | `vt7pty_*` C functions and types |
| `WINPTY_*` public constants and macros | `VT7PTY_*` public constants and macros |
| `WINPTY_DEBUG` | `VT7PTY_DEBUG` |
| `WINPTY_SHOW_CONSOLE` | `VT7PTY_SHOW_CONSOLE` |
| `\\.\pipe\winpty-control-*` | `\\.\pipe\vt7pty-control-v1-*` |
| `\\.\pipe\winpty-{conin,conout,conerr}-*` | `\\.\pipe\vt7pty-data-v1-{conin,conout,conerr}-*` |
| `\\.\pipe\DebugServer` | `\\.\pipe\VT7Pty-Debug-v1` |
| `trivial_test.exe` | `BackendSmokeTest.exe` |
| implicit client-agent compatibility | `VT7Pty-Agent` identity plus protocol version `1` |
| package `0.4.4-dev` | package `0.5.0-dev` |

The inherited public surface still contains 19 operations, but every exported
symbol now uses the `vt7pty_*` prefix. `vt7pty_version.h` declares API version
`1.0`; the encoded `VT7PTY_API_VERSION` value stores the major component in the
upper 16 bits and the minor component in the lower 16 bits.

## Version identities

The identities evolve independently:

- Package version: `0.5.0-dev`, sourced only from `VERSION.txt`.
- Public C API version: `1.0`, sourced from `src/include/vt7pty_version.h`.
- Client-agent protocol version: `1`, sourced from `src/shared/Protocol.h`.
- Source identity: the Git commit embedded at build time.

The agent's `--version` output, verification records, and package manifest carry
all applicable identities. Windows resources on every maintained binary use
VT7Pty product metadata and the package version.

## Client-agent compatibility

The agent begins its setup packet with the exact identity `VT7Pty-Agent` and an
integer protocol version. The DLL validates both before accepting pipe names.
A missing executable returns `VT7PTY_ERROR_AGENT_EXE_MISSING`; a missing,
malformed, wrong-identity, older, or newer handshake returns
`VT7PTY_ERROR_AGENT_INCOMPATIBLE` with a diagnostic that identifies the reason.

The protocol and debug endpoint names include a version marker to avoid silent
collisions. A future compatible protocol change may retain the protocol number;
an incompatible change must advance it and update its tests and release notes.

## Preserved WinPTY references

WinPTY remains named in copyright notices, credits, upstream provenance,
migration notes, historical documentation, validation records, issue links,
and Git history. These references acknowledge the implementation foundation;
they are not active VT7Pty product identifiers or compatibility aliases.
