# Step 1.2 Mechanism rc1 Evidence

Date: 2026-10-12. Windows 7 results pending.
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
