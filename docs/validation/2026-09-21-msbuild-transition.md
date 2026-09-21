# MSBuild Transition Validation: 2026-09-21

Status: accepted development-host evidence for the implemented portion of
Roadmap Step 0.2.

This record proves that the supported VS2022/MSBuild workflow reproduces the
inherited native boundary before obsolete build paths or components are
removed. It is not physical Windows 7 acceptance and does not complete Step
0.2 by itself.

## Identity and environment

| Item | Recorded value |
| --- | --- |
| Source commit | `2c8f1126946721f8e73e200f41429fc31b048c4f` |
| Source state | Clean for the recorded builds and verification |
| Package identity | Inherited `0.4.4-dev`; technical rebranding remains Step 0.5 |
| Host | Windows 10 IoT Enterprise LTSC 2021, build 19044, x64 |
| Shell validation | Windows PowerShell 5.1 |
| Visual Studio | Visual Studio 2022 Enterprise 17.14.40 |
| Toolset | MSVC v143, tools 14.44.35207 |
| Inspection tool | `dumpbin.exe` 14.44.35228.0 |
| Windows SDK | 10.0.26100.0 |
| Configurations | x64 Debug and Release |

The maintained build uses C++20, Unicode, `_WIN32_WINNT=0x0601`,
`WINVER=0x0601`, `NTDDI_VERSION=0x06010000`, the static CRT, `/permissive-`,
`/W4`, `/Brepro`, embedded object debug information, full linked PDBs, ASLR,
DEP compatibility, and PE subsystem version 6.01. Release uses `/O2`, function
level linking, string pooling, reference elimination, and identical COMDAT
folding.

The solution includes all 13 inherited client-library translation units, all
28 agent translation units, and all 8 debug-server translation units. It also
builds the two inherited automated tests. Source membership was compared
directly with the old make fragments.

## Commands and results

The accepted run used the maintained commands through Windows PowerShell 5.1:

```powershell
.\Verify-VT7Pty.ps1
.\Package-VT7Pty.ps1 -NoVerify
```

| Configuration | Check | Result |
| --- | --- | --- |
| Debug | `StringBuilderTest.exe` | Pass, exit 0, reported `All tests completed!` |
| Debug | `trivial_test.exe` | Pass, exit 0, completed the inherited process/output lifecycle |
| Debug | `winpty-agent.exe --version` | Pass; reported `0.4.4-dev` and the exact source commit |
| Release | `StringBuilderTest.exe` | Pass, exit 0, reported `All tests completed!` |
| Release | `trivial_test.exe` | Pass, exit 0, completed the inherited process/output lifecycle |
| Release | `winpty-agent.exe --version` | Pass; reported `0.4.4-dev` and the exact source commit |

All tests used a 60-second timeout. Release tests retain assertions so their
checks are not compiled out by `NDEBUG`.

Every inspected PE image is x64 and declares subsystem version 6.01. The
direct import sets match the inherited baseline exactly:

| Binary | Direct imports |
| --- | --- |
| `winpty.dll` | `ADVAPI32.dll`, `KERNEL32.dll`, `USER32.dll` |
| `winpty-agent.exe` | `ADVAPI32.dll`, `KERNEL32.dll`, `SHELL32.dll`, `USER32.dll` |
| `winpty-debugserver.exe` | `ADVAPI32.dll`, `KERNEL32.dll` |
| `trivial_test.exe` | `KERNEL32.dll`, `winpty.dll` |
| `StringBuilderTest.exe` | `KERNEL32.dll` |

The static CRT introduces no Visual C++ runtime DLL dependency. The DLL still
exports the same 19 `winpty_*` functions listed in the
[inherited baseline](2026-09-21-upstream-baseline.md). Every PE image contains
generated `VT7Pty` product metadata, `0.4.4-dev` file/product versions, and the
correct inherited filename. Binary names and API symbols intentionally remain
unchanged until Step 0.5.

## Artifact record

### Debug

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `winpty.dll` | 2041344 | `0d64d1f0c84b022c020c4c3b65fdd788fed59ea417cb0d699fa5781c5ef12910` |
| `winpty.lib` | 5762 | `526d14d99df7d612493503c1b02a6514398fceb1a81fae8f7e01adc46a167a29` |
| `winpty.pdb` | 10588160 | `d388d4c047d1ff32b9977b8db8c8c81e92662eb2ff93461df4ae4adcab08364e` |
| `winpty-agent.exe` | 2307584 | `8359f4a1be0353a6f399d1466934f3dd3689b98bc562e83c7c24437162a67e9d` |
| `winpty-agent.pdb` | 12718080 | `5f85b6c5b247677b9c621373a1dab5a6008632dbf8e9dcd1b64b08bec3227901` |
| `winpty-debugserver.exe` | 1897472 | `ddc31ab15594a57a788f8d3606847236dee04bfb1c142a3ebf0f7f3267f4ef55` |
| `winpty-debugserver.pdb` | 9998336 | `2aad5b186b6d39cd919a39ad8b95d878704fd8965ffbc0f75e2da86c3dad2be3` |
| `StringBuilderTest.exe` | 2684928 | `7ffa8a0523443461cb0f56240e27cafa67ba42fdc260fc040e33987757bd7f2d` |
| `StringBuilderTest.pdb` | 11653120 | `2b7129af40beafa612cbb468683f1fe385a02b6e3910c60116d5a9fc59013a9f` |
| `trivial_test.exe` | 1333248 | `281d555542f1acea7a6e3fd4a283f6451e5cf53fe9da3b8da10d421b054a47c5` |
| `trivial_test.pdb` | 7868416 | `f03258ddfccec0d7c79a8baadeb5991a1b5deb6123dffe578190bd24a338dbee` |

### Release

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `winpty.dll` | 233472 | `d9b6b0adda28b55146bed3b20c7cdb0022b45337bc380cebe7d2a4727c04f1b4` |
| `winpty.lib` | 5762 | `526d14d99df7d612493503c1b02a6514398fceb1a81fae8f7e01adc46a167a29` |
| `winpty.pdb` | 5206016 | `5af0c8a884921d1bcfc77aea6ea61dfe046b36d796484ebedf651dd17b9d8272` |
| `winpty-agent.exe` | 322560 | `af7927c7c5295c85df7f5ec36370f508d865d7abeb6506319ab0a514d2915fb8` |
| `winpty-agent.pdb` | 6238208 | `971a51fb1488387e9dbc7594a9afc216d1227e6790c3ebb2ee40f7b568e35ed4` |
| `winpty-debugserver.exe` | 190464 | `cc8dda8c8a7570e24af189daec7be25eec44f0d5e5d182e09e64468898523a6b` |
| `winpty-debugserver.pdb` | 4943872 | `7866b5d7529f8b4517f5d6a5df654278d747a0ac86c3f67811824048d57d9e8e` |
| `StringBuilderTest.exe` | 372736 | `224accc06d6e3bcfb475e15fd9e905988dd2f2f2239a56f4f91cc12d63108467` |
| `StringBuilderTest.pdb` | 6352896 | `c6ab1126a94506e8156169916cc37246168339c76a40d29a3395be6b7bd3fce5` |
| `trivial_test.exe` | 183808 | `2118402f2216381c783c33aef7460cb36ce12ef422e57ab5baf1628ce17be423` |
| `trivial_test.pdb` | 4444160 | `341b2dfaeee164a07ad5dc9914db5be031c63268fa0cfb71019fd190ff57b5c4` |

Two consecutive clean rebuilds produced identical hashes for every required
EXE, DLL, and import library. PDBs changed between rebuilds, matching the open
limitation found in the inherited baseline. The hashes above identify the
second accepted rebuild.

The transition ZIP
`VT7Pty-0.4.4-dev-win7-x64-release.zip` has SHA-256
`bdcf44b1e4f183b7ba8ae95647e15e85d9320004a60f64720f95a8b0ddbcc21b`.
Its manifest records the same source commit and a clean source tree. This is a
workflow artifact, not a release candidate.

## Warning baseline and remaining work

Each clean configuration rebuild reports 298 warning occurrences across 82
unique source/code locations. Repeated occurrences result from shared sources
being compiled into multiple targets.

| Code | Occurrences | Unique locations | Meaning |
| --- | ---: | ---: | --- |
| C4100 | 8 | 4 | Unreferenced parameters |
| C4127 | 6 | 6 | Constant conditional expressions |
| C4244 | 241 | 50 | Potentially lossy conversions |
| C4245 | 1 | 1 | Signed/unsigned conversion |
| C4267 | 42 | 21 | `size_t` narrowing conversions |

These warnings are visible under `/W4` and are not suppressed. Warning cleanup
and eventual `/WX` remain Roadmap Step 0.6 work.

The following work remains before Step 0.2 can close:

- add maintained project membership for retained fixtures and tools,
- verify an actual source-level Visual Studio debug session,
- decide how to handle non-deterministic linked PDB content,
- run later physical Windows 7 acceptance; this record does not make that
  compatibility claim.

The Unix adapter, x86, XP/Vista paths, experimental Unicode benchmark, and
broader fidelity scenarios remain outside this transition run. No obsolete
build route has been removed yet.
