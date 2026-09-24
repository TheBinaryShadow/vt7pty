# Releasing VT7Pty

Status: Step 0.9 manual release process under validation. Updated: 2026-09-24.

VT7Pty uses local VS2022/MSBuild commands, physical Windows 7 acceptance, and a
reviewed manual GitHub Release. There is no hosted build or test service.
[CHANGELOG.md](../CHANGELOG.md) records VT7Pty changes; the inherited
[WinPTY release history](../RELEASES.md) remains historical.

## Build a candidate set

From a clean source tree, run:

```powershell
.\Package-VT7Pty.ps1 -Configuration Release -PackageSuffix m0-rc4
```

The command refuses a dirty tree. It performs a clean x64 Release rebuild,
MSVC static-analysis rebuild, and the complete local Release verification. It
checks the source commit and every test result before packaging. It refuses to
overwrite any existing candidate asset, extracts each produced archive, and
verifies every file against its own manifest. The
package set is written under `artifacts/packages/`.

The canonical candidate stem is
`VT7Pty-{version}-win7-x64-release[-suffix]`. The command creates:

| Asset | Contents |
| --- | --- |
| `-runtime.zip` | `VT7Pty.dll`, `VT7Pty-Agent.exe`, compatibility guidance, attribution, and manifest |
| `-development.zip` | Public C headers, `VT7Pty.lib`, API/version guidance, attribution, and manifest |
| `-symbols.zip` | Matching DLL, agent, debug-server, and test PDBs, diagnostics guidance, attribution, and manifest |
| `-tests.zip` | Self-contained Windows 7 acceptance runner, runtime and debug-server binaries, controlled fixtures, tests, verification/inspection records, guidance, attribution, and manifest |
| `-release-set.json` | Source, API, protocol, toolchain, local verification, archive sizes, counts, manifest hashes, and archive hashes |
| `.sha256` | SHA-256 for all four archives and the release-set manifest |

Every ZIP carries `README.md`, `VERSION.txt`, `CHANGELOG.md`, `LICENSE.txt`,
`CREDITS.md`, and `UPSTREAM.md`. The native debug server belongs to the test
bundle; its matching PDB belongs to symbols. The minimal runtime archive does
not include debugging tools. Static runtime linking avoids a separate MSVC
redistributable package, but the import and manifest inspection records in the
test archive and execution on Windows 7 remain the dependency evidence.

Use a fresh suffix for every candidate whose bytes change. Never replace a
candidate already sent for physical acceptance. Compare source commit, test
manifest hash, and archive SHA-256 before testing or publishing.

## Physical acceptance and review

Copy the same `-tests.zip` and the `.sha256` file to all three Windows 7
tiers. Follow [Windows 7 platform acceptance](WINDOWS7_ACCEPTANCE.md); the
ordinary runner performs all 30 cases, including the 120-minute soak. Preserve
each machine's complete `results` directory and review logs, failures, skips,
dumps, update inventories, and the recorded environment disposition.

After all three runs, validate the exact release set and result records:

```powershell
.\tools\release\Review-VT7PtyRelease.ps1 `
    -ReleaseSetPath .\artifacts\packages\VT7Pty-0.5.0-dev-win7-x64-release-m0-rc4-release-set.json `
    -NonEsuResult C:\path\to\nonesu\results\windows7-nonesu-<run>.json `
    -EsuResult C:\path\to\esu\results\windows7-esu-<run>.json `
    -LegacyResult C:\path\to\legacy\results\windows7-legacy-<run>.json
```

The review checks the four archive hashes, the checksum file, test-manifest
identity, source/version/API/protocol identity, three distinct machine records, all
30 passing cases, the full soak, and absence of preflight failures, inventory
warnings, and crash artifacts. It writes a machine-readable review under
`artifacts/release-review/`. A passing automated review does not replace a
human assessment of the logs, hardware inventory, compatibility claim,
changelog, and release notes. The review record explicitly does not authorize
publication.

## Version, tag, and publication

`0.5.0-dev` remains the development identity until Milestone 0 acceptance.
The Step 0.9 tooling candidate may use that identity. Changing `VERSION.txt`
to `0.5.0` changes the exact binaries and requires a new clean package set and
three physical acceptance runs; earlier `0.5.0-dev` evidence cannot qualify it.

For an accepted version `X.Y.Z`, the canonical tag is `vX.Y.Z`, the GitHub
Release title is `VT7Pty X.Y.Z`, and the unsuffixed asset stem is
`VT7Pty-X.Y.Z-win7-x64-release`. Candidate suffixes such as `m0-rc4` identify
trial package sets and are never removed by renaming their files. Build the
final unsuffixed `X.Y.Z` set from its exact clean source commit, run all three
physical tiers on that exact test ZIP, and review its own release-set manifest
and checksums before publication.

Before publication, review the exact clean commit/tag target, local verification
and physical records, exports/imports/resources/manifests/symbols, license and
attribution, changelog, release notes, and every final archive/checksum. Tag
and GitHub publication are separate user decisions after a concrete reviewable
result exists. Once authorized, push the tag, upload the four ZIPs, release-set
manifest, and checksum file, then verify the hosted downloads against SHA-256.

The `milestone-0-start` tag is a planning marker, not a release tag. Installers
and code signing remain outside the initial release process.
