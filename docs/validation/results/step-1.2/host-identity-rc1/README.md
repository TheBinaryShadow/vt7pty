# Host identity rc1 evidence

Recorded: 2026-10-13. H1 accepted on the development host and all three designated
Windows 7 tiers. See the [target review](../../../2026-10-13-step-1.2-host-identity-rc1-windows7.md).

- [Exact candidate manifest](candidate-manifest.json)
- [Independent Windows 7 review and 132-file SHA-256 inventory](windows7-evidence-review.json)
- [Non-ESU report: NESSY](nonesu/report.json)
- [ESU report: TURTLE](esu/report.json)
- [Legacy report: LEOPARD](legacy/report.json)
- [Independent development review and 44-file SHA-256 inventory](evidence-review.json)
- [Portable development report](developmenthost/report.json)
- [Development summary](developmenthost/summary.txt)
- [Clean candidate Release verification](candidate-release-verification.json)
- [Clean candidate Release tests](candidate-release-tests.json)
- [Precommit Debug verification](precommit-debug-verification.json)
- [Precommit Debug tests](precommit-debug-tests.json)
- [Precommit Release verification](precommit-release-verification.json)
- [Precommit Release tests](precommit-release-tests.json)
- [Package-integrity negative control](integrity-control.json)

Each machine has three run directories retaining six host observations, three
read results, a closed-client observation, three raw streams and native aggregate
metadata each, plus a portable report and summary. All 44 files per machine
are copied without normalization.

The Windows 7 review confirms nine native/aggregate matches, 54 host observations
and 27 read texts across 132 returned files. All nine target repetitions pass.
The separate development review confirms three native/aggregate matches,
18 host observations and nine read texts across 44 files.

The earlier evidence-review.json is the immutable development review snapshot;
its Windows7EvidencePending=true records the state before the target results
arrived. The later windows7-evidence-review.json records H1 target acceptance.

Precommit verification records retain parent c9e49f4 and SourceTreeClean=false;
the clean candidate records identify ab442287eee97c94249c55f0b1573c660873268e.
No precommit record is represented as exact-package acceptance.

The integrity control uses a separate altered extraction, never the issued ZIP.
DispatchThreadVerified and CancellationImplemented remain false throughout.
