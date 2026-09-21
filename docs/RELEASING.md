# Releasing VT7Pty

Status: approved process; transition tooling partially implemented. Updated:
2026-09-21.

VT7Pty uses local, repository-owned build, verification, and packaging commands
followed by physical Windows 7 acceptance and a manual GitHub Release. The
project does not use a hosted build or test service.

The inherited [WinPTY release notes](../RELEASES.md) remain historical. VT7Pty
release changes are recorded in [CHANGELOG.md](../CHANGELOG.md).

## Candidate preparation

The initial local commands now exist:

```powershell
.\Build-VT7Pty.ps1 -Configuration Release
.\Verify-VT7Pty.ps1 -Configuration Release
.\Package-VT7Pty.ps1 -Configuration Release
```

The current package is a single transition archive used to prove the local
workflow. Roadmap Steps 0.8 and 0.9 must still implement the complete suite,
physical acceptance bundle, final archive split, and final naming before a
release candidate is valid.

A candidate must identify:

- source commit and clean/dirty state,
- VT7Pty package version,
- API and client-agent protocol versions,
- compiler, toolset, Windows SDK, architecture, and configuration,
- artifact hashes and runtime imports/dependencies,
- verification result and tool versions.

## Package set

The initial release process will produce:

- a runtime archive with the DLL, agent, and required runtime material,
- a development archive with public headers and import libraries,
- a symbols archive with matching PDB files,
- a physical Windows 7 test bundle with fixtures and instructions,
- a component/build manifest,
- license, attribution, and third-party notices,
- a SHA-256 checksum file.

The native debug server may be supplied with diagnostics or symbols instead of
the minimal runtime archive. The final placement will be fixed with the package
layout in Roadmap Step 0.9.

## Acceptance sequence

1. Confirm the source tree and intended version.
2. Run a clean x64 Release build.
3. Run the complete local verification suite.
4. Inspect exports, imports, resources, manifests, symbols, and package contents.
5. Create an immutable candidate package and checksum record.
6. Run the candidate on physical non-ESU Windows 7 SP1 x64.
7. Run milestone and release candidates on physical ESU Windows 7 SP1 x64.
8. Review failures, skips, logs, dumps, and differences from the previous
   accepted candidate.
9. Update the changelog, compatibility claims, and release notes.
10. Review the exact tag target and all release assets.
11. Create and push the release tag.
12. Publish the GitHub Release and verify every uploaded checksum.

A local pass cannot replace either required physical release-candidate run.
Every exception or skipped case must have an explicit disposition before
publication.

## Version and tag rules

`0.5.0-dev` is the first VT7Pty development identity. It does not become
`0.5.0` until all Milestone 0 acceptance items are complete and the release has
received a separate review.

The `milestone-0-start` tag marks the approved planning foundation. It is not a
release tag and must not be attached to release assets.

Release tags, names, and archive filenames will use one canonical form fixed by
Roadmap Step 0.9 before the first release candidate is published.

## Deferred distribution work

An installer and code signing are not initial release requirements. They may be
added later when concrete distribution and trust requirements justify their
maintenance and testing cost.
