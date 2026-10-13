# Step 1.1 Initial Fidelity Baseline Evidence

Date: 2026-10-12. I recorded the initial step-1.1-rc1 corpus here; Step 1.1 remains open.

| Directory | Platform / machine | Original run ID | Pass / Fail / NotApplicable |
| --- | --- | --- | --- |
| [developmenthost](developmenthost/results.json) | Windows 10 / WOLF | developmenthost-20261012-063941-ce3d4f34 | 19 / 4 / 0 |
| [nonesu](nonesu/results.json) | Windows 7 / NESSY | nonesu-20261012-065639-a57d666f | 19 / 4 / 0 |
| [esu](esu/results.json) | Windows 7 / TURTLE | esu-20261012-065725-26823e40 | 19 / 4 / 0 |
| [legacy](legacy/results.json) | Windows 7, PowerShell 2.0 / LEOPARD | legacy-20261012-065613-d5a1b44b | 17 / 4 / 2 |

Each directory preserves complete returned results, case JSON, controlled streams,
console snapshots, shell identities and command-execution markers. The Windows 7
copies contain 214 files, all checked byte-for-byte against K:\VT7_work\results.
[windows7-evidence-review.json](windows7-evidence-review.json) records those
SHA-256 values and identity/case/cleanup checks. Target prompt-cancellation
failures remain Fail; they are different from the development host's Unicode/
resize mismatches. No run has missing required initial cases or harness failures.
All eight fault controls pass and application hashes remain stable on every host.

[candidate-manifest.json](candidate-manifest.json) and
[candidate-cases.json](candidate-cases.json) preserve the exact extracted
candidate metadata without modification. Verification records for the final
clean harness source are stored as debug-verification.json and
release-verification.json. Their existing 29 local entries pass; these short
comparisons did not rerun or replace the 120-minute Windows 7 release soak.

These records support the
[candidate review](../../2026-10-12-step-1.1-candidate.md) and
[Windows 7 baseline review](../../2026-10-12-step-1.1-windows7-baseline.md).
CharacterizationComplete covers the implemented initial corpus; the eight
PendingCoverage groups remain NotRun and MilestoneAccepted remains false for rc1.

## Later extension evidence

[rc3/README.md](rc3/README.md) indexes the separately qualified 37-entry
development-host extension, exact metadata and clean verification reports.
It retains the 29 Pass / 8 Fail development run alongside the later three
rc3 target runs: 31 Pass / 6 Fail on NESSY and TURTLE, 26 Pass / 6 Fail /
5 NotApplicable on LEOPARD. Every applicable active-child case passes; both
cooked-read probes fail on every target. Seven broader groups remain pending.
The original directories above retain unchanged rc1 records; they do not
qualify the extension.
