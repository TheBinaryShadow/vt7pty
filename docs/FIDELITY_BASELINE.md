# Running the VT7Pty 0.5.0 Fidelity Baseline

This is a characterization bundle for Roadmap Step 1.1. It combines the unchanged,
accepted 0.5.0 DLL/agent with a separately identified new harness, fixture and
independent VT state reader. Its manifest records both source identities and
hashes. It is not a new runtime release or Milestone 1 acceptance.

## Windows 7 runs

Extract the entire ZIP into a fresh local directory and use the matching file:

- RUN-FIDELITY-NONESU.cmd: Windows 7 non-ESU tier with Windows PowerShell 5.1.
- RUN-FIDELITY-ESU.cmd: Windows 7 ESU tier with Windows PowerShell 5.1.
- RUN-FIDELITY-LEGACY.cmd: Windows 7 legacy tier with Windows PowerShell 2.0.

The first two default to C:\Program Files\PowerShell\7\pwsh.exe and require
engine 7.2.24. Pass -PowerShell7Path followed by the quoted executable path
to a CMD launcher if the installation differs. Tier C uses its installed
PowerShell 2.0; PowerShell 7 cases are explicitly NotApplicable there.

The runner validates every packaged file and the unchanged runtime hashes.
It checks application/module hashes before and after the run, and records
OS/update identities, console modes/code pages and console fonts,
state/cursor/attribute comparisons, timing, controlled terminal bytes,
fixture snapshots, failures and cleanup. Keep the complete results directory.

Run the package's tests on x64 Windows using Windows PowerShell. The target
runner supports PowerShell 2.0 and does not require KB3191566. Local build and
packaging commands use the existing development tools and modern PowerShell.

## Interpreting results

The 23 entries comprise the state-reader unit suite, 14 initial workflow
observations and eight deliberate negative controls. Controls pass only when
their exact injected failure is detected and owned processes are cleaned up.
The native control record therefore says Fail while the containing control
entry says Pass; both remain visible.

Normal workflow failures are preserved as Fail. A missing/wrong pinned shell
is NotRun. The runner exits nonzero if required evidence is missing, the harness
fails, or a workflow fails. A nonzero result is useful baseline evidence;
return the complete records rather than changing statuses to Pass.

CharacterizationComplete means the implemented initial cases were observed
with a healthy harness. PendingCoverage separately lists required work not yet
implemented. MilestoneAccepted remains false. No failure is automatically
classified as a Windows 7 backend defect based on a newer development console.

The reader models the emitted sequence subset and uses declared corpus width
rules for combining text, CJK samples and the selected emoji. It does not
implement general Unicode width policy or terminal reflow. Resize mismatches
need review against the recorded stream, snapshots and consumer resize policy.
Text and cursor-width results are separate. RawUtf16CellDifferences is a
diagnostic count and is not the normalized Unicode text oracle.

Automation uses clean shell profiles. PowerShell 7 imports its bundled PSReadLine
with Windows editing, prediction disabled, and session-local history configured
to SaveNothing. The loaded engine/editor versions, module path and editing/history
configuration are captured through the tested session.
Ordinary profiles, full key/mouse/layout coverage, distinct Ctrl+Break, the
broader code-page matrix, automatic buffer restoration and target performance
budgets remain listed in PendingCoverage.

## Recorded initial baseline

The [candidate review](validation/2026-10-12-step-1.1-candidate.md) identifies
step-1.1-rc1 and its development-host observations. The
[Windows 7 review](validation/2026-10-12-step-1.1-windows7-baseline.md) preserves
the same candidate's three target runs: NESSY and TURTLE each report 19 Pass /
4 Fail; LEOPARD reports 17 Pass / 4 Fail / 2 NotApplicable. All four failures
concern Command Prompt and installed Windows PowerShell prompt cancellation.
The controlled Unicode and resize cases pass on all three targets; PowerShell 7
passes on A/B. Oracles and fault controls pass, with no harness failures.

These are initial characterization results. Step 1.1 remains open for the
PendingCoverage recipes, and failing workflows have not been accepted.

## Development commands

From a clean checkout, build/verify and create an identified candidate:

    powershell.exe -NoProfile -File .\Package-FidelityBaseline.ps1 -Suffix step-1.1-rc1

The command verifies the existing regression suite and new binary imports,
runs the independent oracle tests, and refuses to replace an existing candidate
or use a modified 0.5.0 runtime archive. Candidate ZIPs and checksums are under
artifacts\fidelity\packages. Matching harness PDBs are included; existing
0.5.0 symbols remain in the published symbols archive.

For a development-host comparison, run the extracted package with:

    powershell.exe -NoProfile -File .\tools\fidelity\Invoke-FidelityBaseline.ps1 -Tier DevelopmentHost

The development host may lack exact PowerShell 7.2.24; that stays NotRun.
The original 30-case suite and 120-minute Windows 7 release soak remain separate
regression/qualification gates. This short bundle does not replace them.
