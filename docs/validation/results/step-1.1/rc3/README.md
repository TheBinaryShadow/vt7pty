# Step 1.1 rc3 Development-Host Evidence

Date: 2026-10-12. Windows 10 development host only; target extension evidence
remains pending. Runtime 0.5.0 is unchanged. Harness version 3 source:
fa1e68345b5b94fcc367a0e08e2b257657105659.

[developmenthost/results.json](developmenthost/results.json) and the complete
developmenthost directory preserve the independently extracted rc3 run:
developmenthost-20261012-075946-6441d889. Its 37 entries report 29 Pass / 8 Fail,
zero NotRun, healthy oracles/fault controls, stable application identities and
successful native cleanup. All nine active-child cases pass. The retained
failures cover four prompt cases, two cooked-read probes and development-host
Unicode/resize comparisons.

All 158 run files and four metadata files were copied byte-for-byte.
[evidence-review.json](evidence-review.json) records SHA-256 values and checks,
including semantic equality of all 35 native case observations with the aggregate
report. The [candidate manifest](candidate-manifest.json) and
[case definitions](candidate-cases.json) retain exact package identity and recipes.
debug-verification.json and release-verification.json preserve the clean-source
29-entry local verification for each configuration.

JSON_ORACLE preserves its controlled input.json and roundtrip.json; unlike the
retired rc2 report, child metadata is intact in the aggregate. Rc2 was not handed
off and is not qualification evidence. See the
[candidate review](../../../2026-10-12-step-1.1-rc3-candidate.md) for that disposition
and the local cooked-read diagnosis.

The original [three-tier version 1 evidence](../README.md) remains separate.
No Windows 7 extension or milestone acceptance is implied by this directory.
