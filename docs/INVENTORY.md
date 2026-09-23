# Inherited Component Inventory

Status: completed for Roadmap Step 0.1 and updated through Step 0.6.
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
| `src/include/vt7pty.h` | Public C API, ownership rules, pipe discovery, spawn, resize, errors | Retain and modernize as the renamed VT7Pty C surface; no WinPTY alias layer |
| `src/include/vt7pty_constants.h` | Errors, agent flags, mouse modes, spawn flags | Retain and rebrand; the XP/Vista desktop-creation flag was removed in Step 0.4 |
| `src/libvt7pty/AgentLocation.*` | Finds the colocated `VT7Pty-Agent.exe` | Retain and modernize path/error handling |
| `src/libvt7pty/ClientException.h` | Maps internal exceptions to public errors | Retain, rebrand, and modernize |
| `src/libvt7pty/VT7PtyInternal.h` | Private client declarations | Retain, rebrand, and modernize |
| `src/libvt7pty/vt7pty.cc` | Public API implementation, agent launch, RPC, spawn, lifecycle | Retain as the client core; explicit identity/protocol validation added in Step 0.5 |
| `src/libvt7pty/subdir.mk` | GNU Make source list | Replace with MSBuild project membership, then remove |

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
| `ConsoleFont.*` | Selects a usable console font on Windows 7 and later | Uses the documented Windows 7+ extended console-font APIs after Step 0.4 |
| `AgentCreateDesktop.*` | Formerly created the pre-Windows 7 background desktop through a helper agent | Removed in Step 0.4 with the unreachable XP/Vista path |
| `DebugShowInput.*` | Human-readable input diagnostics | Retain and integrate with structured diagnostics |
| `Coord.h`, `SmallRect.h`, `SimplePool.h`, `UnicodeEncoding.h` | Geometry, allocation, and Unicode helpers | Retain and modernize where tests justify it |
| `UnicodeEncodingTest.cc` | Exhaustive encoder experiment and performance loop | Rehome under tests; split correctness from the multi-billion-iteration benchmark |
| `subdir.mk` | GNU Make source list | Replace with MSBuild project membership, then remove |

## Shared implementation

| Files | Current responsibility | Classification and destination |
| --- | --- | --- |
| `Protocol.h` | Client-agent identity, protocol version, handshake, message structures, and opcodes | Retain; explicit identity/version and negative tests added in Step 0.5 |
| `BackgroundDesktop.*` | Formerly managed a hidden desktop for XP/Vista consoles | Removed in Step 0.4; the inherited runtime selected it only below Windows 7 |
| `Buffer.*` | RPC message serialization and parsing | Uses `std::span`, byte views, typed sizes, and checked bounds after Step 0.6; malformed-message expansion remains later test work |
| `DebugClient.*` | Native diagnostic transport | Emits bounded structured JSON records with severity, subsystem, build, process, and thread identity; bounded pipe waits and `OutputDebugString` fallback were added in Step 0.7 |
| `GenRandom.*` | Random identifiers for pipes and objects | Uses checked size conversions after Step 0.6; failure-path expansion remains later test work |
| `Mutex.h` | Mutex/lock shim for old MinGW | Replaced with `std::mutex` and removed in Step 0.3 |
| `OsModule.h` | Dynamic module/symbol lookup | Move-only, null-safe module ownership retained for explicit Windows 7-safe capability detection |
| `OwnedHandle.*` | Move-only Win32 handle ownership | Provides nonthrowing close/reset/destruction and `noexcept` move semantics after Step 0.6 |
| `PrecompiledHeader.h` | Legacy common include set | Removed in Step 0.3; projects use explicit includes and shared MSBuild policy |
| `StringBuilder.h`, `StringBuilderTest.cc` | Former diagnostic string builder and standalone test | Removed in Step 0.6 after call sites moved to C++20 `std::format`, `std::to_chars`, or streams; the maintained replacement test is `tests/unit/ModernCppTest.cc` |
| `StringUtil.*` | UTF-16 formatting and error strings | Uses standard formatting with explicit conversion and buffer bounds after Step 0.6 |
| `TimeMeasurement.h` | Small timing helper | Rehomed to `tests/manual` in Step 0.3 |
| `ControlCharacters.h` (formerly `UnixCtrlChars.h`) | Control-character decoding used by Windows input parsing | Retained under a platform-neutral name in Step 0.3 |
| `WindowsSecurity.*` | Security descriptors, pipe/process identity, token data | Retained with owner/System/administrators pipe ACLs, peer-PID support, and the broad Everyone-write descriptor removed in Step 0.7 |
| `WindowsVersion.*` | OS detection and module-version diagnostics | Retained with `RtlGetVersion`-based detection and x64-only diagnostics in Step 0.4 |
| `StringFormatting.h` (formerly replaced `vt7pty_snprintf.h`) | Former bounded diagnostic formatting shim | Removed in Step 0.6; maintained call sites use standard C++ or bounded CRT formatting directly |
| `Assert.*` | Agent-aware assertion reporting | Retain responsibility; rebrand and integrate with diagnostics |
| `Exception.*` | Internal exception hierarchy | Concrete exception ownership and direct throws replace the inherited indirection after Step 0.6 |
| `Version.*` | Reports generated package, API, protocol, and source identity | Retain as the diagnostic identity boundary |
| `GetCommitHash.bat`, `UpdateGenVersion.bat` | GYP-era generated version header | Replaced by MSBuild/PowerShell generation and removed in Step 0.3 |

## Executables, tests, and adapter

| Path | Current responsibility | Classification and destination |
| --- | --- | --- |
| `tools/debug/DebugServer.cc` | Native structured diagnostic collector | Retained as `VT7Pty-DebugServer.exe`; bounded file output, live ACL self-test, and exact version reporting were added in Step 0.7 |
| `tools/diagnostics/New-VT7PtyDiagnosticBundle.ps1` | Physical-machine diagnostic bundle collector | Maintained support boundary; captures logs, exact build identity, host servicing data, and binary hashes without network access |
| `tests/integration/BackendSmokeTest.cc` | Opens a session, connects pipes, spawns a child, validates output and exit code | Rehomed as the maintained integration test in Step 0.6 |
| `tests/unit/ProtocolTest.cc`, `tests/unit/ModernCppTest.cc` | Protocol validation and standard-library/narrowing checks | Maintained unit-test boundary established in Step 0.6 |
| `tests/fixtures/ProtocolTestAgent.cc` | Deliberately incompatible agent fixture | Rehomed with controlled child-process fixtures in Step 0.6 |
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
| `VERSION.txt` | Authoritative `0.5.0-dev` package version | Retain as the single package-version input |
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
2. The inherited baseline and source path had to prove that the background
   desktop served only systems below Windows 7 before its removal. The
   resulting candidate must pass both physical Windows 7 tiers before Step 0.4
   is accepted.
3. Useful native probes reached their new test/tool/document locations before
   `misc` was removed in Step 0.3.
4. Attribution, original notices, and historical release material remain even
   when their implementation paths are removed.
