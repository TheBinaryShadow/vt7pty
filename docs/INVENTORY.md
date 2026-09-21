# Inherited Component Inventory

Status: completed for Roadmap Step 0.1 and updated through Step 0.3.
Updated: 2026-09-22.

This inventory began as the classification of the inherited WinPTY tree. It is
kept current as items move or are removed so the disposition of inherited
material remains reviewable.

## Classification

| Classification | Meaning |
| --- | --- |
| Retain | The responsibility remains part of VT7Pty. Renaming and ordinary modernization still apply. |
| Modernize | The responsibility remains, but its implementation or interface needs current C++/Windows practice. |
| Replace | A new VT7Pty mechanism must perform the useful responsibility before the inherited file is removed. |
| Rehome | Useful test, tool, or research material moves to the maintained `tests`, `tools`, or `docs` structure. |
| Archive | Preserve as historical evidence outside the active implementation. |
| Remove | No active VT7Pty responsibility remains after prerequisite replacement work. |
| Validate removal | Expected to be removable, but baseline or physical Windows 7 evidence must prove it first. |

Git history and the preserved upstream README remain the authoritative record
for removed implementation material.

## Runtime and public surface

| Path or group | Current responsibility | Classification and destination |
| --- | --- | --- |
| `src/include/winpty.h` | Public C API, ownership rules, pipe discovery, spawn, resize, errors | Retain and modernize as the renamed VT7Pty C surface; no WinPTY alias layer |
| `src/include/winpty_constants.h` | Errors, agent flags, mouse modes, spawn flags | Retain and rebrand; remove the XP/Vista desktop-creation flag after validation |
| `src/libwinpty/AgentLocation.*` | Finds the colocated agent | Retain and modernize path/error handling; rename agent identity |
| `src/libwinpty/LibWinptyException.h` | Maps internal exceptions to public errors | Retain, rebrand, and modernize |
| `src/libwinpty/WinptyInternal.h` | Private client declarations | Retain, rebrand, and modernize |
| `src/libwinpty/winpty.cc` | Public API implementation, agent launch, RPC, spawn, lifecycle | Retain as the client core; rebrand and add version/protocol validation |
| `src/libwinpty/subdir.mk` | GNU Make source list | Replace with MSBuild project membership, then remove |

The inherited DLL exports 19 functions. The complete baseline is recorded in
[the validation report](validation/2026-09-21-upstream-baseline.md).

## Agent core

| Files | Current responsibility | Classification and destination |
| --- | --- | --- |
| `Agent.*`, `main.cc` | Session ownership, RPC dispatch, child launch, shutdown | Retain and modernize |
| `EventLoop.*`, `NamedPipe.*` | Wait/dispatch loop and asynchronous data pipes | Retain; strengthen cancellation, bounds, and lifecycle tests before refactoring |
| `ConsoleInput.*`, `ConsoleInputReencoding.*` | VT input parsing, key/mouse records, mode behavior, encoding | Retain as core fidelity code; modernize incrementally with regression coverage |
| `DefaultInputMap.*`, `InputMap.*`, `DsrSender.h` | Key mapping and device-status response handling | Retain, rebrand, and test |
| `Scraper.*`, `ConsoleLine.*`, `Terminal.*` | Screen-state comparison and VT output generation | Retain as core fidelity code; refactor only with observable-state tests |
| `Win32Console.*`, `Win32ConsoleBuffer.*`, `LargeConsoleRead.*` | Console access, screen buffers, large reads | Retain; modernize Windows 7+ API boundaries |
| `ConsoleFont.*` | Selects a usable console font across old Windows versions | Modernize around Windows 7+ APIs; remove XP-only undocumented fallback paths |
| `AgentCreateDesktop.*` | Creates the pre-Windows 7 background desktop through a helper agent | Validate removal with the XP/Vista background-desktop path |
| `DebugShowInput.*` | Human-readable input diagnostics | Retain and integrate with structured diagnostics |
| `Coord.h`, `SmallRect.h`, `SimplePool.h`, `UnicodeEncoding.h` | Geometry, allocation, and Unicode helpers | Retain and modernize where tests justify it |
| `UnicodeEncodingTest.cc` | Exhaustive encoder experiment and performance loop | Rehome under tests; split correctness from the multi-billion-iteration benchmark |
| `subdir.mk` | GNU Make source list | Replace with MSBuild project membership, then remove |

## Shared implementation

| Files | Current responsibility | Classification and destination |
| --- | --- | --- |
| `AgentMsg.h` | Client-agent message structures and opcodes | Retain, rebrand, validate sizes, and add explicit protocol identity/version |
| `BackgroundDesktop.*` | XP/Vista hidden-console desktop management | Validate removal after confirming no Windows 7+ call path requires it |
| `Buffer.*` | RPC message serialization and parsing | Retain; add bounds/malformed-message tests and typed size handling |
| `DebugClient.*` | Native diagnostic transport | Retain, rebrand, and modernize with bounded structured logging |
| `GenRandom.*` | Random identifiers for pipes and objects | Retain; modernize size conversions and verify failure handling |
| `Mutex.h` | Mutex/lock shim for old MinGW | Replaced with `std::mutex` and removed in Step 0.3 |
| `OsModule.h` | Dynamic module/symbol lookup | Retain and modernize for explicit Windows 7-safe capability detection |
| `OwnedHandle.*` | Move-only Win32 handle ownership | Retain and modernize rather than replace for style alone |
| `PrecompiledHeader.h` | Legacy common include set | Removed in Step 0.3; projects use explicit includes and shared MSBuild policy |
| `StringBuilder.h`, `StringBuilderTest.cc` | Diagnostic string builder and standalone test | Retain behavior; rehome test and evaluate standard C++ replacement during modernization |
| `StringUtil.*` | UTF-16 formatting and error strings | Retain; modernize conversions and bounds |
| `TimeMeasurement.h` | Small timing helper | Rehomed to `tests/manual` in Step 0.3 |
| `ControlCharacters.h` (formerly `UnixCtrlChars.h`) | Control-character decoding used by Windows input parsing | Retained under a platform-neutral name in Step 0.3 |
| `WindowsSecurity.*` | Security descriptors, pipe/process identity, token data | Retain as a security boundary; modernize conversions and add focused tests |
| `WindowsVersion.*` | OS detection, architecture, native process launch helpers | Retain required launch work; remove x86, XP/Vista, old MinGW, and deprecated version-detection paths |
| `StringFormatting.h` (replaces `winpty_snprintf.h`) | Bounded diagnostic formatting | Uses the current C++ runtime; the retired-compiler shim was removed in Step 0.3 |
| `WinptyAssert.*` | Agent-aware assertion reporting | Retain responsibility; rebrand and integrate with diagnostics |
| `WinptyException.*` | Internal exception hierarchy and old `noexcept` shim | Retain hierarchy, remove compiler shim, and rebrand |
| `WinptyVersion.*` | Reports generated version and commit | Replace with authoritative VT7Pty package/API/protocol/source identity |
| `GetCommitHash.bat`, `UpdateGenVersion.bat` | GYP-era generated version header | Replaced by MSBuild/PowerShell generation and removed in Step 0.3 |

## Executables, tests, and adapter

| Path | Current responsibility | Classification and destination |
| --- | --- | --- |
| `src/debugserver/DebugServer.cc` | Native timestamped diagnostic collector | Retain, rebrand, and modernize as `VT7Pty-DebugServer.exe` |
| `src/debugserver/subdir.mk` | GNU Make source list | Replace, then remove |
| `src/tests/trivial_test.cc` | Opens a session, connects pipes, spawns a child, validates output and exit code | Retain and rehome as an integration test |
| `src/tests/subdir.mk` | GNU Make test rule | Replace, then remove |
| `src/unix-adapter/*` | Cygwin/MSYS terminal frontend, POSIX input/output, wakeup FD, utility wrappers | Removed in Step 0.3 after the native MSBuild baseline was reproduced |
| `src/unix-adapter/subdir.mk` | Unix-adapter build membership | Removed with the adapter in Step 0.3 |

## Build, packaging, and repository infrastructure

| Path | Current responsibility | Classification and destination |
| --- | --- | --- |
| `src/winpty.gyp`, `src/configurations.gypi`, `vcbuild.bat` | Python 2/GYP generation of VS2013/2015 projects, Win32/x64 and XP toolsets | Replaced by checked-in VS2022/MSBuild files and removed in Step 0.3 |
| `configure`, `Makefile`, `src/subdir.mk`, component `subdir.mk` files | Cygwin/MSYS/MinGW configuration and GNU Make build | Removed in Step 0.3 after MSBuild captured retained membership |
| `appveyor.yml` | VS2015 job installing Cygwin/MSYS/MinGW and invoking Python 2 shipping | Removed in Step 0.3; no hosted replacement is planned |
| `ship/common_ship.py`, `ship/ship.py` | Python 2 environment, Unix variants, tar packaging | Manifest, hash, and package duties moved to PowerShell; removed in Step 0.3 |
| `ship/make_msvc_package.py` | VS2013/2015, x86/x64, XP/non-XP ZIP production | Replaced by the x64 Windows 7+ PowerShell packager and removed in Step 0.3 |
| `ship/build-pty4j-libpty.bat` | IntelliJ/pty4j-specific legacy native packaging | Removed in Step 0.3 as unrelated to the VT7-first package contract |
| `.gitignore` | Generated-file policy | Updated for the maintained artifact, package, and MSBuild layout in Step 0.3 |
| `.gitattributes` | Text and line-ending policy | Updated for the maintained MSBuild/PowerShell/resource files in Step 0.3 |
| `VERSION.txt` | Single inherited `0.4.4-dev` string | Replace with authoritative `0.5.0-dev` version input during technical rebranding |
| `tools/baseline/*` | Temporary reproducible capture of the inherited native boundary | Removed in Step 0.3 after its results and commands were preserved in validation records |

## Documentation and provenance

| Path or group | Classification |
| --- | --- |
| `LICENSE`, `CREDITS.md`, `UPSTREAM.md`, preserved Git history | Retain permanently |
| `docs/UPSTREAM_WINPTY_README.md`, `RELEASES.md` | Retain as clearly labeled historical upstream documentation |
| VT7Pty root policies and `docs/*.md` | Retain and update with implementation evidence |
| `docs/validation/*` | Retain immutable milestone evidence; corrections append or supersede transparently |

## Resolved `misc` classification

Step 0.3 removed the `misc` container after classifying every item. Useful
Windows-native probes now live under `tests`, developer utilities under
`tools`, and research notes under `docs/historical`. Git history preserves the
discarded experiments and obsolete scripts.

| Item | Classification and destination |
| --- | --- |
| `.gitignore`, `build32.sh`, `build64.sh`, `color-test.sh` | Removed; root policy and native MSBuild fixtures cover retained duties |
| `DebugClient.py`, `DebugServer.py`, `DumpLines.py`, `Spew.py` | Removed; native diagnostics and deterministic output fixtures cover retained duties |
| `UnixEcho.cc`, `GetCh.cc`, `FormatChar.h`, `ConinMode.ps1` | Removed after duplicate and retained behavior review |
| `Notes.txt`, input/mouse/font notes, `Font-Report-June2016/*` | Preserved under `docs/historical` |
| `ShowArgv.cc`, `ShowConsoleInput.cc`, `Utf16Echo.cc`, `Win32Echo1.cc`, `Win32Echo2.cc`, `Win32Write1.cc`, `WriteConsole.cc` | Rehomed under `tests/fixtures` with maintained MSBuild projects in Step 0.2 |
| `OutputLines.cc`, `ConsoleColorGrid.cc` | Added as native replacements for the useful deterministic output and color patterns |
| `ConinMode.cc`, `ConoutMode.cc`, `IdentifyConsoleWindow.ps1` | Rehomed under `tools/console` |
| Remaining native console, resize, screen-buffer, Unicode, font, platform, and bug probes | Rehomed individually under `tests/manual`; disruptive cases are explicitly documented |
| `TestUtil.cc`, `TimeMeasurement.h`, `UnicodeEncodingTest.cc` | Rehomed under `tests/manual`; permanent test integration remains Step 0.8 |

## Removal gates

Nothing classified `Replace` or `Validate removal` is deleted until its stated
replacement or evidence exists. In particular:

1. The checked-in MSBuild build must reproduce the baseline before GYP, Make,
   Python 2 packaging, or their source lists disappear.
2. Physical Windows 7 evidence must precede removal of the background-desktop
   path and XP/Vista public flag.
3. Useful native probes reached their new test/tool/document locations before
   `misc` was removed in Step 0.3.
4. Attribution, original notices, and historical release material remain even
   when their implementation paths are removed.
