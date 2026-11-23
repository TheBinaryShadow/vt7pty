# Test results and validation history

I keep dated reviews here so I can trace a decision back to the exact source,
package and test environment. The [roadmap](../../ROADMAP.md) describes current
progress; each review describes the state at the time it was written.

I completed Steps 1.2 and 1.3 within their recorded scopes. Output rc3
establishes the measured text and cell-placement boundary on all three
Windows 7 tiers.
[Accepted output scope](../OUTPUT_FIDELITY.md#accepted-output-scope)

Profile rc3 passes 115 H4
guards, 129 input cases, 30 controlled profile cases, 27 publication checks
and 40 physical checkpoints across the three Windows 7 tiers. The earlier
H3, H4 and input qualification results remain linked below. Milestone 1 is
still in progress. [Profile rc3 Windows 7 review](2026-11-23-step-1.2-profiles-rc3-windows7.md)

## Current fidelity work

| Work | Candidate and development results | Windows 7 results |
| --- | --- | --- |
| Step 1.4 full-screen restoration and resize | [Screen rc1, 14 WOLF observations and verified delivery](2026-11-24-step-1.4-screen-rc1-candidate.md) | Pending on NESSY, TURTLE and LEOPARD |
| Step 1.3 Unicode width | [rc3 experiments, local review and verified delivery](2026-11-24-step-1.3-output-rc3-candidate.md) | [126 observations and 48 available replays reviewed](2026-11-24-step-1.3-output-rc3-windows7.md) |
| Step 1.3 output | [rc2 local characterization and verified delivery](2026-11-23-step-1.3-output-rc2-candidate.md) | [78 observations reviewed, with two reproducible width differences](2026-11-23-step-1.3-output-rc2-windows7.md) |
| Step 1.2 profiles | [rc3 local qualification and delivery](2026-11-23-step-1.2-profiles-rc3-candidate.md) | [rc3 qualified on all three tiers](2026-11-23-step-1.2-profiles-rc3-windows7.md) |
| Profile rc2 | [Earlier candidate](2026-11-20-step-1.2-profiles-rc2-candidate.md) | [Frozen controls passed. Profile blockers identified.](2026-11-23-step-1.2-profiles-rc2-windows7.md) |
| Step 1.2 input decoder | [rc2: exact archive, 129 input and 123 H4 guards pass locally](2026-11-19-step-1.2-input-rc2-candidate.md) | [Qualified: 387 input, 345 H4 guards, 21 lifetime/fault guards and 42 regressions pass](2026-11-19-step-1.2-input-rc2-windows7.md) |
| Step 1.2 input coverage | [rc1 characterization, 33 new cases and frozen guards](2026-11-19-step-1.2-input-rc1-candidate.md) | [Reviewed: 345 guards pass; three input failures confirmed](2026-11-19-step-1.2-input-rc1-windows7.md) |
| H4 manual follow-up | [Corrected runner and exact-package verification](2026-11-19-step-1.2-h4-manual-rc1-candidate.md) | [Accepted: 9/9 physical workflows; H4 complete](2026-11-19-step-1.2-h4-manual-rc1-windows7.md) |
| H4 runtime integration | [rc4 full local qualification and issuance](2026-11-17-step-1.2-h4-runtime-rc4-candidate.md) | [Automated matrix passed](2026-11-18-step-1.2-h4-runtime-rc4-windows7.md); [manual completion](2026-11-19-step-1.2-h4-manual-rc1-windows7.md) |
| H3 guard qualification | [rc1 candidate and exact-package verification](2026-11-13-step-1.2-host-guards-rc1-candidate.md) | [Accepted: 282/282 guard cases; H4 remains](2026-11-14-step-1.2-host-guards-rc1-windows7.md) |
| Coordinated H3 prototype | [rc1 candidate and exact-package verification](2026-11-13-step-1.2-host-coordination-rc1-candidate.md) | [Reviewed: baseline and recording verified; H3 guards remain](2026-11-13-step-1.2-host-coordination-rc1-windows7.md) |
| H3 scraper coordination | [rc1 candidate, controlled contrasts and retained failures](2026-11-13-step-1.2-host-freeze-diagnostics-rc1-candidate.md) | [Reviewed: coordination supported; H3 open](2026-11-13-step-1.2-host-freeze-diagnostics-rc1-windows7.md) |
| H3 paired diagnostics | [rc1 candidate and retained local failure](2026-11-13-step-1.2-host-cancellation-diagnostics-rc1-candidate.md) | [Reviewed: selection-state correlation; H3 open](2026-11-13-step-1.2-host-cancellation-diagnostics-rc1-windows7.md) |
| Host cancellation (H3) | [rc1 prototype and local results](2026-10-13-step-1.2-host-cancellation-rc1-candidate.md) | [Not accepted: five failing repetitions](2026-11-13-step-1.2-host-cancellation-rc1-windows7.md) |
| Host dispatch (H2) | [rc1 candidate](2026-10-13-step-1.2-host-dispatch-rc1-candidate.md) | [Accepted](2026-10-13-step-1.2-host-dispatch-rc1-windows7.md) |
| Host identity (H1) | [rc1 candidate](2026-10-13-step-1.2-host-identity-rc1-candidate.md) | [Accepted](2026-10-13-step-1.2-host-identity-rc1-windows7.md) |
| Public-API cancellation experiments | [Mechanism rc1](2026-10-12-step-1.2-mechanism-rc1-candidate.md) | [Both approaches failed](2026-10-13-step-1.2-mechanism-rc1-windows7.md) |
| Active-child and cooked-read baseline | [Step 1.1 rc3](2026-10-12-step-1.1-rc3-candidate.md) | [Six cancellation failures per machine](2026-10-12-step-1.1-rc3-windows7-baseline.md) |
| Initial shell and fixture baseline | [Step 1.1 initial candidate](2026-10-12-step-1.1-candidate.md) | [Four prompt-cancellation failures per machine](2026-10-12-step-1.1-windows7-baseline.md) |

The [H3 failure investigation](2026-11-13-step-1.2-host-cancellation-rc1-investigation.md)
compares the original failures with passes. The [paired diagnostic review](2026-11-13-step-1.2-host-cancellation-diagnostics-rc1-windows7.md)
records the selection-state finding. The [historical reassessment](2026-11-13-step-1.2-cancellation-reassessment.md)
rechecks the evidence chain and strengthens the next experiment with held-state
controls, freeze/Escape tracing and separate shell measurements. The
[controlled Windows 7 review](2026-11-13-step-1.2-host-freeze-diagnostics-rc1-windows7.md)
records the resulting contrasts, stale frozen state and neighbor trace limit.

The H1-H3 research bundles use the released 0.5.0 runtime. Passing a research stage
does not resolve the recorded shell defects or accept Milestone 1.

## Published baseline

| Release | Candidate | Acceptance | Publication |
| --- | --- | --- | --- |
| 0.5.0 | [Final package](2026-10-02-0.5.0-release-candidate.md) | [Three-machine review](2026-10-02-0.5.0-release-acceptance.md) | [Tag and downloaded assets](2026-10-02-0.5.0-publication.md) |

## Milestone 0 history

| Step | Candidate or initial record | Completion |
| --- | --- | --- |
| 0.1: Inherited native baseline | [2026-09-21 baseline](2026-09-21-upstream-baseline.md) | Recorded in baseline |
| 0.2: Build and debugging | [MSBuild transition](2026-09-21-msbuild-transition.md) | [Completed](2026-09-22-step-0.2-completion.md) |
| 0.3: Remove obsolete components | [Completed removals and checks](2026-09-22-step-0.3-completion.md) | Recorded together |
| 0.4: Windows 7 platform | [Candidate](2026-09-22-step-0.4-candidate.md) | [Accepted with recorded evidence exception](2026-09-22-step-0.4-completion.md) |
| 0.5: Technical identity | [Candidate](2026-09-22-step-0.5-candidate.md) | [Accepted with recorded evidence exception](2026-09-22-step-0.5-completion.md) |
| 0.6: Native modernization | [Candidate](2026-09-22-step-0.6-candidate.md) | [Accepted](2026-09-23-step-0.6-completion.md) |
| 0.7: Diagnostics and security | [Candidate](2026-09-24-step-0.7-candidate.md) | [Accepted](2026-09-24-step-0.7-completion.md) |
| 0.8: Permanent test system | [Candidate](2026-09-24-step-0.8-candidate.md) | [Accepted](2026-09-24-step-0.8-completion.md) |
| 0.9: Release process | [Superseded candidate](2026-09-24-step-0.9-candidate.md), [three-machine rc7](2026-09-24-step-0.9-rc7-candidate.md) | [Accepted](2026-09-25-step-0.9-completion.md) |

The [completed checklist](../MILESTONE_0.md) retains the detailed requirements.

## Reading the evidence

Each review links its raw records under results, along with hashes and original
run IDs. I preserve those files byte-for-byte, including failures and partial
runs. Review prose can be edited for clarity; factual corrections must be
identified explicitly or recorded in a dated successor.

I keep procedures in [Testing](../TESTING.md) and the individual runner guides.
This directory records what ran, what happened and what I concluded.
