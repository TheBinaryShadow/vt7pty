# Step 1.1 rc3 Fidelity Baseline Evidence

Date: 2026-10-12. I recorded the implemented corpus on the development host and
all three Windows 7 tiers; broader Step 1.1 coverage remains open.
Runtime 0.5.0 is unchanged. Harness version 3 source:
fa1e68345b5b94fcc367a0e08e2b257657105659.

| Directory | Platform / machine | Original run ID | Pass / Fail / NotApplicable |
| --- | --- | --- | --- |
| [developmenthost](developmenthost/results.json) | Windows 10 / WOLF | developmenthost-20261012-075946-6441d889 | 29 / 8 / 0 |
| [nonesu](nonesu/results.json) | Windows 7 / NESSY | nonesu-20261012-081316-eda0860f | 31 / 6 / 0 |
| [esu](esu/results.json) | Windows 7 / TURTLE | esu-20261012-081344-6f8f2e17 | 31 / 6 / 0 |
| [legacy](legacy/results.json) | Windows 7, PowerShell 2.0 / LEOPARD | legacy-20261012-081259-fb10cf83 | 26 / 6 / 5 |

Each directory preserves its complete report, native case files, streams,
console/child snapshots, shell identities and command-execution markers.
Every applicable active-child case passes. Both oracles and all ten fault
controls pass on every host, with stable application hashes and successful
native process cleanup.

[evidence-review.json](evidence-review.json) records the independently extracted
development run: 158 run files and four metadata files, copied byte-for-byte,
with all 35 native observations matching the aggregate report.

[windows7-evidence-review.json](windows7-evidence-review.json) records the
445 returned target files and their SHA-256 values. It checks all 100 native
observations against aggregate values, 78 binary child snapshots against
native metadata, portable JSON round-trips, identities, applicability,
expected fault controls, cleanup and command-execution markers. Both oracles
also pass on LEOPARD's installed PowerShell 2.0 engine without KB3191566.

The [candidate manifest](candidate-manifest.json) and
[case definitions](candidate-cases.json) retain exact package identity and
recipes. debug-verification.json and release-verification.json preserve the
clean-source 29-entry local verification for each configuration. These short
comparisons do not replace the original 120-minute release soak.

Four native-editor prompt cases and two cooked-read probes remain Fail on
every Windows 7 target. WOLF additionally retains its Unicode/resize mismatches;
the bounded target Unicode/resize comparisons pass. JSON_ORACLE preserves its
controlled input.json and roundtrip.json; unlike retired rc2, child metadata is
intact in the aggregate. Rc2 was never handed off or used as qualification.

See the [candidate review](../../../2026-10-12-step-1.1-rc3-candidate.md) for
package/local qualification and the
[three-tier review](../../../2026-10-12-step-1.1-rc3-windows7-baseline.md) for
target observations and diagnosis. The
[original version 1 evidence](../README.md) remains separate.

CharacterizationComplete applies to the implemented 37-entry corpus. Seven
broader groups remain NotRun and MilestoneAccepted is false. These reports
preserve cancellation failures rather than accepting them.
