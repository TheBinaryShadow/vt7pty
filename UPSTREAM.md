# VT7Pty upstream provenance

## WinPTY foundation

| Item | Baseline |
| --- | --- |
| Project | [WinPTY](https://github.com/rprichard/winpty) |
| Creator | Ryan Prichard, with the WinPTY contributors |
| Inherited commit | [`7e59fe2d09adf0fa2aa606492e7ca98efbc5184e`](https://github.com/rprichard/winpty/commit/7e59fe2d09adf0fa2aa606492e7ca98efbc5184e) |
| Commit date | 2018-12-19 |
| Source version | `0.4.4-dev` |
| License | MIT; original notice retained in [LICENSE](LICENSE) and source files |

The Git history records upstream authorship. The original README is preserved
in [docs/UPSTREAM_WINPTY_README.md](docs/UPSTREAM_WINPTY_README.md), and
[RELEASES.md](RELEASES.md) retains upstream release history.

## Current divergence

VT7Pty now uses a Windows 7 SP1+ x64 platform contract, VS2022/MSBuild and
C++20, a native-only source tree, a `0.5.0-dev` VT7Pty identity, and maintained
local build, test, diagnostic, and packaging commands. The inherited Unix
adapter, pre-Windows 7 paths, and obsolete build infrastructure were removed.
The retained native implementation has been modernized without claiming
ConPTY compatibility or complete terminal fidelity. The exact changes and
Windows 7 acceptance evidence are linked from the [roadmap](ROADMAP.md) and
[validation index](docs/validation/README.md). Historical WinPTY names and
notices remain part of the provenance record.

VT7Pty changes are recorded separately in [CHANGELOG.md](CHANGELOG.md), under
the identity rules in [docs/VERSIONING.md](docs/VERSIONING.md).

## ConPTY and Microsoft Terminal

[Microsoft Terminal](https://github.com/microsoft/terminal), including its
console-host and ConPTY work, is an architectural and behavioral reference.
Credit belongs to Microsoft and the project's contributors. Its upstream
[license](https://github.com/microsoft/terminal/blob/main/LICENSE) is MIT.

This rebranding imports no ConPTY implementation. Acknowledging its influence
does not imply that VT7Pty implements its ABI or process-attachment contract.
Future source reuse must record the exact upstream revision, adapted files,
changes, and applicable notices when it happens.

[VT7](https://github.com/TheBinaryShadow/VT7) is the intended primary consumer
and a reference for integration requirements and community-document structure.
Its source is maintained separately from this backend.

## Preserving provenance

- Retain upstream history, authorship, license text, and file-level notices.
- Identify upstream fixes by their original commit when importing them.
- For copied or adapted third-party material, record its source, revision,
  license, and required notices alongside the change.
- Distinguish implemented reuse from design inspiration and behavioral study.
- Update [CREDITS.md](CREDITS.md) when new work becomes part of the project.
- Do not redirect historical upstream issue links to unrelated VT7Pty issues.

New VT7Pty-authored contributions use the MIT license. Existing third-party
material retains its applicable terms and attribution.
