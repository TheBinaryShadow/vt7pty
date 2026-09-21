# Inherited Component Inventory

Status: completed for Roadmap Step 0.1. Updated: 2026-09-21.

This inventory classifies the WinPTY-derived tree before build replacement,
removal, technical rebranding, or runtime modernization. It describes intended
Milestone 0 treatment, not work already performed.

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
| `Mutex.h` | Mutex/lock shim for old MinGW | Replace with standard C++ synchronization, then remove |
| `OsModule.h` | Dynamic module/symbol lookup | Retain and modernize for explicit Windows 7-safe capability detection |
| `OwnedHandle.*` | Move-only Win32 handle ownership | Retain and modernize rather than replace for style alone |
| `PrecompiledHeader.h` | Legacy common include set | Replace with the MSBuild-era include/precompiled-header policy |
| `StringBuilder.h`, `StringBuilderTest.cc` | Diagnostic string builder and standalone test | Retain behavior; rehome test and evaluate standard C++ replacement during modernization |
| `StringUtil.*` | UTF-16 formatting and error strings | Retain; modernize conversions and bounds |
| `TimeMeasurement.h` | Small timing helper | Rehome with tests/diagnostics if still used; otherwise remove after reference audit |
| `UnixCtrlChars.h` | Control-character constants used by Windows input parsing | Retain behavior and rename to a platform-neutral control-character header |
| `WindowsSecurity.*` | Security descriptors, pipe/process identity, token data | Retain as a security boundary; modernize conversions and add focused tests |
| `WindowsVersion.*` | OS detection, architecture, native process launch helpers | Retain required launch work; remove x86, XP/Vista, old MinGW, and deprecated version-detection paths |
| `winpty_snprintf.h` | Formatting shim for old MSVC, MinGW, Cygwin, and MSYS | Replace with current C++/MSVC formatting helpers, then remove |
| `WinptyAssert.*` | Agent-aware assertion reporting | Retain responsibility; rebrand and integrate with diagnostics |
| `WinptyException.*` | Internal exception hierarchy and old `noexcept` shim | Retain hierarchy, remove compiler shim, and rebrand |
| `WinptyVersion.*` | Reports generated version and commit | Replace with authoritative VT7Pty package/API/protocol/source identity |
| `GetCommitHash.bat`, `UpdateGenVersion.bat` | GYP-era generated version header | Replace with MSBuild targets, then remove |

## Executables, tests, and adapter

| Path | Current responsibility | Classification and destination |
| --- | --- | --- |
| `src/debugserver/DebugServer.cc` | Native timestamped diagnostic collector | Retain, rebrand, and modernize as `VT7Pty-DebugServer.exe` |
| `src/debugserver/subdir.mk` | GNU Make source list | Replace, then remove |
| `src/tests/trivial_test.cc` | Opens a session, connects pipes, spawns a child, validates output and exit code | Retain and rehome as an integration test |
| `src/tests/subdir.mk` | GNU Make test rule | Replace, then remove |
| `src/unix-adapter/*` | Cygwin/MSYS terminal frontend, POSIX input/output, wakeup FD, utility wrappers | Remove in full after the native MSBuild baseline is reproduced |
| `src/unix-adapter/subdir.mk` | Unix-adapter build membership | Remove with the adapter |

## Build, packaging, and repository infrastructure

| Path | Current responsibility | Classification and destination |
| --- | --- | --- |
| `src/winpty.gyp`, `src/configurations.gypi`, `vcbuild.bat` | Python 2/GYP generation of VS2013/2015 projects, Win32/x64 and XP toolsets | Replace with checked-in VS2022/MSBuild files, then remove |
| `configure`, `Makefile`, `src/subdir.mk`, component `subdir.mk` files | Cygwin/MSYS/MinGW configuration and GNU Make build | Remove after MSBuild contains the retained native source membership |
| `appveyor.yml` | VS2015 job installing Cygwin/MSYS/MinGW and invoking Python 2 shipping | Remove; no hosted replacement is planned |
| `ship/common_ship.py`, `ship/ship.py` | Python 2 environment, Unix variants, tar packaging | Replace retained manifest/hash/package duties with PowerShell, then remove |
| `ship/make_msvc_package.py` | VS2013/2015, x86/x64, XP/non-XP ZIP production | Replace with x64 Windows 7+ packaging, then remove |
| `ship/build-pty4j-libpty.bat` | IntelliJ/pty4j-specific legacy native packaging | Remove; unrelated to the VT7-first package contract |
| `.gitignore` | Generated-file policy for inherited build routes | Replace entries as the new layout lands |
| `.gitattributes` | Text and line-ending policy | Retain and update for new MSBuild/PowerShell/resource files |
| `VERSION.txt` | Single inherited `0.4.4-dev` string | Replace with authoritative `0.5.0-dev` version input during technical rebranding |
| `tools/baseline/*` | Temporary reproducible capture of the inherited native boundary | Retain through Milestone 0 comparison; archive or remove after the permanent test/build evidence supersedes it |

## Documentation and provenance

| Path or group | Classification |
| --- | --- |
| `LICENSE`, `CREDITS.md`, `UPSTREAM.md`, preserved Git history | Retain permanently |
| `docs/UPSTREAM_WINPTY_README.md`, `RELEASES.md` | Retain as clearly labeled historical upstream documentation |
| VT7Pty root policies and `docs/*.md` | Retain and update with implementation evidence |
| `docs/validation/*` | Retain immutable milestone evidence; corrections append or supersede transparently |

## `misc` native probes and research

The `misc` directory is not removed wholesale. Each item has a disposition so
useful knowledge does not disappear with Unix-era tooling.

| Item | Classification and destination |
| --- | --- |
| `.gitignore` | Merge relevant fixture outputs into the root ignore policy, then remove |
| `build32.sh`, `build64.sh` | Remove; superseded Unix-shell compiler wrappers |
| `color-test.sh` | Replace useful color patterns with a native fixture, then remove |
| `DebugClient.py`, `DebugServer.py` | Remove after native diagnostics cover their useful behavior; pywin32/Python helpers are not retained |
| `DumpLines.py`, `Spew.py` | Replace with deterministic native output fixtures, then remove |
| `UnixEcho.cc` | Remove; Unix terminal-mode experiment |
| `Notes.txt` | Archive any unique console findings; discard obsolete build commands |
| `EnableExtendedFlags.txt`, `MouseInputNotes.txt`, `font-notes.txt` | Rehome under `docs/historical` as input/font research |
| `Font-Report-June2016/*` | Rehome under `docs/historical` as dated console-font evidence |
| `TestUtil.cc`, `FormatChar.h` | Rehome as maintained fixture/test helpers and convert included `.cc` patterns to ordinary compilation where appropriate |
| `ShowArgv.cc`, `ShowConsoleInput.cc`, `GetCh.cc`, `Utf16Echo.cc`, `Win32Echo1.cc`, `Win32Echo2.cc`, `Win32Write1.cc`, `WriteConsole.cc` | Rehome as process/input/output fixtures |
| `ConinMode.cc`, `ConinMode.ps1`, `ConoutMode.cc` | Rehome as console-mode inspection/control tools |
| `GetConsolePos.cc`, `MoveConsoleWindow.cc`, `SetBufferSize.cc`, `SetBufInfo.cc`, `SetCursorPos.cc`, `SetWindowRect.cc` | Rehome as console geometry/manual diagnostic tools; automate only where assertions are stable |
| `ChangeScreenBuffer.cc`, `ClearConsole.cc`, `ScreenBufferTest.cc`, `ScreenBufferTest2.cc` | Rehome as screen-buffer behavior tests; split destructive/manual scenarios from routine verification |
| `BufferResizeTests.cc`, `Win10ResizeWhileFrozen.cc`, `Win10WrapTest1.cc`, `Win10WrapTest2.cc` | Rehome as version-aware resize/wrap tests and research fixtures |
| `FreezePerfTest.cc`, `ScreenBufferFreezeInactive.cc`, `SelectAllTest.cc`, `VkEscapeTest.cc` | Rehome as isolated selection/freeze/manual tests with bounded cleanup |
| `UnicodeDoubleWidthTest.cc`, `UnicodeWideTest1.cc`, `UnicodeWideTest2.cc`, `VeryLargeRead.cc` | Rehome as Unicode/large-read research and derive deterministic regression cases |
| `FontSurvey.cc`, `GetFont.cc`, `SetFont.cc` | Rehome as font diagnostic tools used to validate the Windows 7+ font rewrite |
| `IsNewConsole.cc`, `OSVersion.cc` | Rehome temporarily as platform probes; remove when permanent capability/version tests supersede them |
| `Win32Test1.cc`, `Win32Test2.cc`, `Win32Test3.cc` | Rehome useful window-station, selection, and console behavior cases; remove pre-Windows 7-only assumptions |
| `winbug-15048.cc`, `WindowsBugCrashReader.cc` | Archive and keep any executable form in an explicitly destructive/manual test group |

## Removal gates

Nothing classified `Replace` or `Validate removal` is deleted until its stated
replacement or evidence exists. In particular:

1. The checked-in MSBuild build must reproduce the baseline before GYP, Make,
   Python 2 packaging, or their source lists disappear.
2. Physical Windows 7 evidence must precede removal of the background-desktop
   path and XP/Vista public flag.
3. Useful native probes must reach their new test/tool/document location before
   `misc` cleanup.
4. Attribution, original notices, and historical release material remain even
   when their implementation paths are removed.
