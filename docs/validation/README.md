# Test results and validation history

I keep dated reviews here so I can trace a decision back to the exact source,
package and test environment. The [roadmap](../../ROADMAP.md) describes current
progress; each review describes the state at the time it was written.

**Current result:** host identity and dispatch are accepted on all three
Windows 7 machines. Ctrl+C cancellation remains unresolved. Next is a
cooked-read cancellation prototype.

## Current fidelity work

| Work | Candidate and development results | Windows 7 results |
| --- | --- | --- |
| Host dispatch (H2) | [rc1 candidate](2026-10-13-step-1.2-host-dispatch-rc1-candidate.md) | [Accepted](2026-10-13-step-1.2-host-dispatch-rc1-windows7.md) |
| Host identity (H1) | [rc1 candidate](2026-10-13-step-1.2-host-identity-rc1-candidate.md) | [Accepted](2026-10-13-step-1.2-host-identity-rc1-windows7.md) |
| Public-API cancellation experiments | [Mechanism rc1](2026-10-12-step-1.2-mechanism-rc1-candidate.md) | [Both approaches failed](2026-10-13-step-1.2-mechanism-rc1-windows7.md) |
| Active-child and cooked-read baseline | [Step 1.1 rc3](2026-10-12-step-1.1-rc3-candidate.md) | [Six cancellation failures per machine](2026-10-12-step-1.1-rc3-windows7-baseline.md) |
| Initial shell and fixture baseline | [Step 1.1 initial candidate](2026-10-12-step-1.1-candidate.md) | [Four prompt-cancellation failures per machine](2026-10-12-step-1.1-windows7-baseline.md) |

These research bundles use the released 0.5.0 runtime. Passing a research stage
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
