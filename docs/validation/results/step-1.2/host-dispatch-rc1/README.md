# Host dispatch rc1 evidence

Recorded: 2026-10-13. Development evidence only; Windows 7 qualification pending.
See the [candidate review](../../../2026-10-13-step-1.2-host-dispatch-rc1-candidate.md).

- [Exact candidate manifest](candidate-manifest.json)
- [Independent review and 173-file SHA-256 inventory](evidence-review.json)
- [Portable development report](developmenthost/report.json)
- [Development summary](developmenthost/summary.txt)
- [Clean candidate Release verification](candidate-release-verification.json)
- [Clean candidate Release tests](candidate-release-tests.json)
- [Precommit Debug verification](precommit-debug-verification.json)
- [Precommit Debug tests](precommit-debug-tests.json)
- [Precommit Release verification](precommit-release-verification.json)
- [Precommit Release tests](precommit-release-tests.json)
- [Precommit Debug H1 aggregate](precommit-debug-h1/probe.json)
- [Precommit Release H1 aggregate](precommit-release-h1/probe.json)
- [Precommit Debug H2 aggregate](precommit-debug-h2.json)
- [Precommit Release H2 aggregate](precommit-release-h2.json)
- [Modified-callback integrity control](integrity-control.json)

Each of the three exact-package repetitions retains 57 native files: six H1
observations, six discovery records, six callback greetings, 25 request/response
records, five read results, five raw streams, three failure/lifetime controls
and the native aggregate. The report and summary bring the portable run to
173 files. All are copied without normalization.

Independent review checks 18 host lifetimes, 36 discovery events, 75 exchanges
and 15 read texts. Expected rejections and deliberately suppressed final
acknowledgments are explicit controls, not reclassified unexpected failures.

The precommit H1 directories retain all 14 native files each. Precommit H2
aggregates are summaries of separate Debug/Release comparisons; the full
independently reviewed H2 raw records are the exact-package development run.

Precommit verification records identify parent 0a8c035 with SourceTreeClean=false.
The candidate's clean records identify 8b1a20caf969088b1efb01e96a34dea5e63b494f.
The exact package uses the accepted 0.5.0 runtime, not rebuilt research-tree
runtime binaries. CancellationImplemented and MilestoneAccepted remain false.
