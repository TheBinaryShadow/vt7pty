# Inherited Native Baseline: 2026-09-21

Status: accepted development-host baseline for Roadmap Step 0.1.

This record captures the unmodified WinPTY-derived native runtime before
VT7Pty build replacement, component removal, technical rebranding, or source
modernization. It is comparison evidence, not Windows 7 acceptance.

## Identity

| Item | Value |
| --- | --- |
| Upstream runtime source | WinPTY `0.4.4-dev`, upstream commit `7e59fe2d09adf0fa2aa606492e7ca98efbc5184e` |
| VT7Pty foundation marker | `c7580e5d5264bc36c0fe5919e7ce2a4efb63439f`, tag `milestone-0-start` |
| Baseline harness/source commit | `1d8846f6b069bb5d2b649befca833c95f6838511` |
| Runtime source comparison | `src` and `VERSION.txt` match the foundation marker exactly |
| Working tree during recorded run | Clean |
| Architecture | x64 |
| Configurations | Debug and Release |

The harness is [documented here](../../tools/baseline/README.md). Generated
artifacts and full compiler/dumpbin logs remain under ignored `build/baseline`
and can be regenerated from the recorded commit.

## Build environment

| Item | Recorded value |
| --- | --- |
| Host | Windows 10 IoT Enterprise LTSC 2021, build 19044 |
| Visual Studio | Visual Studio 2022 Enterprise 17.14.40 |
| Compiler | MSVC 19.44.35228.0, v143 tools 14.44.35207 |
| Linker | 14.44.35228.0 |
| Windows SDK | 10.0.26100.0 |
| Language mode | C++14 inherited baseline |
| Win32 macros | `_WIN32_WINNT=0x0501`, `WINVER=0x0501` inherited source baseline |
| PE subsystem floor | 6.01, the approved Windows 7 baseline floor |
| Runtime | Debug `/MTd`; Release `/MT` |
| Debug information | `/Z7` objects and full linked PDBs |
| Reproducibility | `/Brepro` compiler and linker mode |

The C++14 and XP-era API macros record the inherited source boundary. They are
not approved VT7Pty product settings. Roadmap Steps 0.2 and 0.4 replace them
with C++20 and the Windows 7 API floor.

## Test results

| Configuration | Test | Result | Observed behavior |
| --- | --- | --- | --- |
| Debug | `StringBuilderTest.exe` | Pass, exit 0 | Reported `All tests completed!` and no error lines |
| Debug | `trivial_test.exe` | Pass, exit 0 | Opened agent/pipes, spawned its child, captured `HI`/`XY`, verified child exit 42, and freed the session |
| Release | `StringBuilderTest.exe` | Pass, exit 0 | Reported `All tests completed!` and no error lines |
| Release | `trivial_test.exe` | Pass, exit 0 | Repeated the complete native process/output session successfully |

Both tests had a 60-second timeout and produced no unexpected standard output
or error output. This covers a minimal lifecycle and output path, not the
complete testing strategy.

## Binary interface and dependencies

All five PE files in each configuration report machine `0x8664` (x64) and
subsystem version `6.01`.

| Binary | Direct imported DLLs |
| --- | --- |
| `winpty.dll` | `ADVAPI32.dll`, `USER32.dll`, `KERNEL32.dll` |
| `winpty-agent.exe` | `ADVAPI32.dll`, `USER32.dll`, `SHELL32.dll`, `KERNEL32.dll` |
| `winpty-debugserver.exe` | `ADVAPI32.dll`, `KERNEL32.dll` |
| `trivial_test.exe` | `winpty.dll`, `KERNEL32.dll` |
| `StringBuilderTest.exe` | `KERNEL32.dll` |

The inherited DLL exports these 19 undecorated C functions:

```text
winpty_agent_process
winpty_conerr_name
winpty_config_free
winpty_config_new
winpty_config_set_agent_timeout
winpty_config_set_initial_size
winpty_config_set_mouse_mode
winpty_conin_name
winpty_conout_name
winpty_error_code
winpty_error_free
winpty_error_msg
winpty_free
winpty_get_console_process_list
winpty_open
winpty_set_size
winpty_spawn
winpty_spawn_config_free
winpty_spawn_config_new
```

This export set is the technical-rebranding comparison surface. It is not the
future stable VT7-facing API.

## Artifact record

### Debug

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `StringBuilderTest.exe` | 2073600 | `2cb9206d9cf9010fbc66e36684065233e88a56e2202c9ca276b50ed81f055531` |
| `StringBuilderTest.pdb` | 10211328 | `15c72295f9a53da4361ed042937b322dd7b44f5b28e633cb4fa478af2478a6ce` |
| `trivial_test.exe` | 1014272 | `6fcb02ea3b70f6b50ea7569e30ac2c057dc98dddd4ddfd5f103c7ba707efb5c3` |
| `trivial_test.pdb` | 6606848 | `268acd2b9fcc227d881cba4789c579aee9600f8f997e0f7f89f150a072f1f131` |
| `winpty.dll` | 1562112 | `b56721bdd48218928797bef030d430f8391e7ed8d8e1f5062d24d891aff265bf` |
| `winpty.exp` | 3196 | `01eed9fc339a86c44275c27e07c2bd0138f591a909f6876c5f58956f6744ec67` |
| `winpty.lib` | 5762 | `526d14d99df7d612493503c1b02a6514398fceb1a81fae8f7e01adc46a167a29` |
| `winpty.pdb` | 8695808 | `1b88598004abc123fc1a3d8b0f8d11f992365e150f6ff8abc7872503b02023a3` |
| `winpty-agent.exe` | 1745408 | `ab0e8409a6e03547a42f6f38ad11e7b46cc0f36bc353cb1ffdce52c0d674898e` |
| `winpty-agent.pdb` | 9842688 | `bfbc178f4d0db1f1a4ebaf010d3ee05e72aef3f868f425d750a5daf761f4527c` |
| `winpty-debugserver.exe` | 1456640 | `69b73aa8b6dba653fe6ec07c1fb8cb27da84d6b900d04ae6aed1c21750ca8f6a` |
| `winpty-debugserver.pdb` | 8351744 | `84078c831daa7aa5dba4316e378817a7dab0fc7f22701056b361d10310ebe26d` |

### Release

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `StringBuilderTest.exe` | 370688 | `5f25fe826047de305441a08a6f3ee4b812ba1ea51e8b7e680c0b7f6747a1eda5` |
| `StringBuilderTest.pdb` | 6393856 | `8bff225953054264afb541e8b8f07651e4ae1e36fe1cc1d0b4a71747c0cfddfe` |
| `trivial_test.exe` | 180736 | `6287b7e33475f6072845b8cdc815727bb5ae66d16b8bbd72ee7024f6cea6c79b` |
| `trivial_test.pdb` | 4419584 | `fd7e69e44ac61e85362417d898d10d3149120d50ddc8d9dc80fb7c9475efe7e2` |
| `winpty.dll` | 233472 | `cab28e97bc32d9bb1d963d5a743a4ed6d73808fa8ab80e9ef747018b598d3e79` |
| `winpty.exp` | 3200 | `3c134ac084bfa87325c893b933e882f7438fb7868d65dc4d7e19d9ce13b6737f` |
| `winpty.lib` | 5762 | `526d14d99df7d612493503c1b02a6514398fceb1a81fae8f7e01adc46a167a29` |
| `winpty.pdb` | 5246976 | `b9066430775688efa5ff9c7fc2498cff8f066f1e313344e139da9a8c4e58ffa3` |
| `winpty-agent.exe` | 321536 | `6d7d5eb7026f905212029c4915575be6c5d8926cd7e029c558cb5093c5bc88cb` |
| `winpty-agent.pdb` | 6254592 | `1ee30371f446619e2523dfe80d6ba9662b448edef11d52970957a03c385ad83f` |
| `winpty-debugserver.exe` | 188416 | `e117c3e00d8c1345b4234f0575426fe171b0efd6a0dd35ce992a6f4c6bbd52d9` |
| `winpty-debugserver.pdb` | 4976640 | `8a6a12484f3f52b29ae9067faede50d7f3c67d61b139ca61e9965cdb58ae5f21` |

## Reproducibility result

Two consecutive clean builds of the same source and settings produced identical
hashes for every EXE, DLL, import library, and export file. Linked PDB content
did not reproduce byte-for-byte, although the corresponding binaries did. PDB
determinism remains a Step 0.2 build-infrastructure issue; each emitted symbol
file is still identified by the hash from its candidate run.

## Inherited warning baseline

Debug and Release each produced 41 warning occurrences representing the same
29 unique source locations. All were narrowing conversions reported as C4244
or C4267; there were no ignored options, compile errors, linker warnings, or
fatal errors.

| Source | Warning locations |
| --- | --- |
| `agent/Agent.cc` | C4267 at 438, 443, 445 |
| `agent/ConsoleInput.cc` | C4244 at 121, 137, 155, 172; C4267 at 340, 355 |
| `agent/ConsoleLine.cc` | C4267 at 127 |
| `agent/DefaultInputMap.cc` | C4267 at 262; C4244 at 294 |
| `agent/EventLoop.cc` | C4267 at 75 |
| `agent/NamedPipe.cc` | two C4267 diagnostics at 191 |
| `agent/Scraper.cc` | C4244 at 560, 623 |
| `agent/Terminal.cc` | C4267 at 365, 369 |
| `agent/Win32Console.cc` | C4267 at 73 |
| `libwinpty/winpty.cc` | C4267 at 305, 333 |
| `shared/DebugClient.cc` | C4267 at 79 |
| `shared/GenRandom.cc` | C4267 at 71, 78 |
| `shared/StringUtil.cc` | C4267 at 42, 50, 51 |
| `shared/WindowsSecurity.cc` | C4267 at 181 |

These warnings are modernization work. They must be resolved deliberately
before `/W4 /WX`; suppression is not accepted as equivalent evidence.

## Limitations and open evidence

- No physical Windows 7 run was performed. Tier A and Tier B acceptance remains
  required during later Milestone 0 steps.
- The Unix adapter, Cygwin/MSYS behavior, x86, XP, and Vista were not built or
  tested because they are outside the approved VT7Pty product.
- `UnicodeEncodingTest.cc` was not run. It mixes useful exhaustive correctness
  work with multi-billion-iteration benchmarking and does not return failure on
  every reported mismatch; it will be split before joining routine verification.
- The two inherited automated tests cover only string formatting and one simple
  process/output lifecycle. Input modes, Unicode fidelity, resize, diagnostics,
  failures, concurrency, resource stability, interactive shells, and remote
  scenarios remain untested.
- Successful development-host execution does not prove runtime imports or
  behavior on Windows 7. The recorded direct imports still require physical
  target validation.

No inherited failure blocked the minimal native session. The warning debt and
missing coverage become explicit inputs to Roadmap Steps 0.2, 0.6, and 0.8.
