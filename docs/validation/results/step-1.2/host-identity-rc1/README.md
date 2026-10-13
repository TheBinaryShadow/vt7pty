# Host identity rc1 evidence

Recorded: 2026-10-13. Development-host evidence only; all Windows 7 tiers pending.

- [Exact candidate manifest](candidate-manifest.json)
- [Independent review and 44-file SHA-256 inventory](evidence-review.json)
- [Portable development report](developmenthost/report.json)
- [Development summary](developmenthost/summary.txt)
- [Clean candidate Release verification](candidate-release-verification.json)
- [Clean candidate Release tests](candidate-release-tests.json)
- [Precommit Debug verification](precommit-debug-verification.json)
- [Precommit Debug tests](precommit-debug-tests.json)
- [Precommit Release verification](precommit-release-verification.json)
- [Precommit Release tests](precommit-release-tests.json)
- [Package-integrity negative control](integrity-control.json)

The three run directories retain six host observations, three read results,
a closed-client observation, three raw streams and native aggregate metadata
each. All 44 portable run files are copied without normalization. The independent
review confirms 18 identity observations, nine read texts and all three
native/aggregate comparisons.

Precommit verification records retain parent c9e49f4 and SourceTreeClean=false;
the clean candidate records identify ab442287eee97c94249c55f0b1573c660873268e.
No precommit record is represented as exact-package acceptance.

The integrity control uses a separate altered extraction, never the issued ZIP.
DispatchThreadVerified and CancellationImplemented remain false throughout.
