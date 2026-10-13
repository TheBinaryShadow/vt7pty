# Host dispatch rc1 evidence

Recorded: 2026-10-13. I accepted H2 on NESSY, TURTLE and LEOPARD.
See the [Windows 7 review](../../../2026-10-13-step-1.2-host-dispatch-rc1-windows7.md)
and the earlier [candidate review](../../../2026-10-13-step-1.2-host-dispatch-rc1-candidate.md).

## Windows 7 results

- [Independent review and 519-file SHA-256 inventory](windows7-evidence-review.json)
- NESSY: [report](nonesu/report.json), [summary](nonesu/summary.txt)
- TURTLE: [report](esu/report.json), [summary](esu/summary.txt)
- LEOPARD: [report](legacy/report.json), [summary](legacy/summary.txt)

The nine target repetitions retain 54 host lifetimes, 3,647 discovery events,
225 exchanges and 45 read results/streams. Every target has three passing
repetitions, no failures, no missing runs and no preflight errors. The legacy
run uses PowerShell 2.0 without KB3191566. All 519 returned files are preserved
without normalization and match their source hashes.

Each repetition contains 57 native files: six H1 observations, six discovery
records, six callback greetings, 25 request/response records, five read results,
five raw streams, three failure/lifetime controls and the native aggregate.
The report and summary bring each portable run to 173 files.

Expected rejections and deliberately suppressed final acknowledgments are
explicit controls, not reclassified unexpected failures. The Windows 7
review records the higher event volume without relaxing identity or buffer checks.

## Candidate and development evidence

- [Exact candidate manifest](candidate-manifest.json)
- [Development review and 173-file SHA-256 inventory](evidence-review.json)
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

The development review checks 18 host lifetimes, 36 discovery events,
75 exchanges and 15 read texts. Its Windows7EvidencePending=true field records
the state at candidate creation and remains unchanged; the separate target
review above records subsequent acceptance.

The precommit H1 directories retain all 14 native files each. Precommit H2
aggregates are summaries of separate Debug/Release comparisons; the full
independently reviewed H2 raw records are the exact-package portable runs.

Precommit verification records identify parent 0a8c035 with SourceTreeClean=false.
The candidate's clean records identify 8b1a20caf969088b1efb01e96a34dea5e63b494f.
The exact package uses the accepted 0.5.0 runtime, not rebuilt research-tree
runtime binaries. CancellationImplemented and MilestoneAccepted remain false.
