# Inherited WinPTY Baseline Harness

This temporary Milestone 0 tool builds and records the unmodified native
WinPTY-derived runtime before VT7Pty replaces its build system or source
identity. It is evidence tooling, not the future supported VT7Pty build.

The script refuses to run when `src` or `VERSION.txt` differs from the
`milestone-0-start` foundation tag. It derives the inherited native source lists
from the existing make fragments, invokes the approved reference MSVC/SDK
installation directly, and writes all output below ignored `build/baseline`.

Run both x64 configurations from PowerShell:

```powershell
.\tools\baseline\Build-WinPtyBaseline.ps1
```

The harness records:

- x64 Debug and Release binaries and PDBs,
- compiler and tool locations,
- PE headers, imports, and DLL exports,
- SHA-256 hashes and file sizes,
- host, source, target-macro, CRT, and configuration identity,
- `StringBuilderTest` and `trivial_test` output and status.

It deliberately uses the inherited C++14 default, `_WIN32_WINNT=0x0501`, and
WinPTY names to capture the pre-modernization boundary. These are not approved
VT7Pty product settings. The supported build introduced in Roadmap Step 0.2
will use C++20, the Windows 7 API floor, and VT7Pty identity in later steps.

The harness does not build the Unix adapter, run the very long experimental
Unicode encoder benchmark, or establish Windows 7 acceptance. Those exclusions
are recorded in the committed baseline report.
