# Step 1.2 Mechanism rc1 Evidence

Updated: 2026-10-13. Development and all three Windows 7 tiers measured;
both public-API probes fail on each target.
Candidate harness source: 3937d5493e7460b799eb6d75b1c8fcb81a1f614f.
Runtime: unchanged accepted 0.5.0.

The [candidate review](../../../2026-10-12-step-1.2-mechanism-rc1-candidate.md)
records archive identity, scope, development findings and the Windows 7 handoff.

- [Development report](developmenthost-20261012-090848-65d6a6fd/results.json):
  WOLF, 29 Pass / 10 Fail, all 39 entries observed, healthy harness.
- [Candidate manifest](candidate-manifest.json) and [case definitions](cases.json):
  exact version 4 bundle identity and recipes.
- debug-verification.json and release-verification.json: clean source, 29
  regression entries passed in each configuration. The corresponding tests.json
  files retain per-case Pass statuses.
- [Evidence review](evidence-review.json): byte counts and SHA-256 for every
  preserved run, metadata and exploratory file; 37 native/aggregate comparisons,
  37 binary child-packet comparisons, JSON round-trip equality and unchanged
  outcomes for the 37 prior recipes.

The complete development run has 171 files. Metadata has six files.
Exploratory trials have 22 files, kept outside the identified candidate run.

## Windows 7 evidence

The [target review](../../../2026-10-13-step-1.2-mechanism-rc1-windows7.md)
records the following unchanged-runtime measurements:

| Directory | Machine | Original run ID | Pass / Fail / NotApplicable |
| --- | --- | --- | --- |
| [nonesu](nonesu/results.json) | NESSY | nonesu-20261013-075038-81cc7e4c | 31 / 8 / 0 |
| [esu](esu/results.json) | TURTLE | esu-20261013-075116-e7aba0ab | 31 / 8 / 0 |
| [legacy](legacy/results.json) | LEOPARD | legacy-20261013-075032-9e112e1a | 26 / 8 / 5 |

[windows7-evidence-review.json](windows7-evidence-review.json) checks exact
candidate identities, applicability, native/aggregate equality for 106 cases,
102 binary child snapshots, three JSON round-trips, stable application/update
identities and all 111 prior recipe outcomes. Its hash inventory covers all
481 target files: 170 non-ESU, 170 ESU and 141 legacy. Copies preserve the
original bytes, including failed observations and fault-control native failures.

All applicable child guards, both oracles and all ten controls pass on each
target. There are no harness failures, preflight errors, timeouts or missing
required cases. Record injection retains the line and adds character 0x03.
Direct cancellation returns error 1168 while the original read stays pending;
the next read works after boundary Enter recovery. Both mechanisms remain Fail.

The earlier development evidence-review.json retains its original
TargetMeasurementsPending=true field as the dated 2026-10-12 review snapshot.
The separate Windows 7 review above records completion of target measurements.

## Exploratory scope

exploratory contains three failed window-message comparisons and a rejected
thread-hook attempt. These used uncommitted exploratory edits based on 078c223.
Their exact source/binary identities were not retained. They are not candidate
qualification, Windows 7 evidence or supported runtime mechanisms.

In host-hook/case.json, ConsoleHostInstrumented=true was an experimental
diagnostic label for the requested test. The target guard returned error 50
before installing a hook or loading the helper into another process.
That field therefore does not prove instrumentation occurred. The raw record
is preserved unchanged; the prototype was removed before source 3937d549.

The identified candidate contains only the two controlled public-API probes,
the original corpus and unchanged runtime. It preserves failed observations
and does not accept Milestone 1 or complete Steps 1.1/1.2.
