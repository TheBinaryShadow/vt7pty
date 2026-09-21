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

The initial VT7Pty changes establish the project identity, goals, contributor
guidance, community policies, and acknowledgements. They do not change source
behavior, remove components, rename runtime artifacts, or change the inherited
`VERSION.txt`. That version identifies the inherited source, not a new VT7Pty
release.

The approved [roadmap](ROADMAP.md) requires infrastructure modernization, audited
component removal, technical rebranding, and a new 0.5.x version series in
Milestone 0. The [development foundation](docs/FOUNDATION.md) records the
approved Windows 7+ platform and engineering decisions. These are future
divergence points; no runtime change or version bump is implied by the planning
documents. Historical WinPTY names and notices remain part of the attribution
record after technical renaming.

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
