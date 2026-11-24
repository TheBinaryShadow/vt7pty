# VT7Pty documentation

I keep the [roadmap](../ROADMAP.md) as the source of truth for progress.
These guides explain how to use the repository, how the backend works, and
what I've learned from testing it.

## Build and use the project

| Guide | Purpose |
| --- | --- |
| [Building](../BUILDING.md) | Set up Visual Studio, build, verify and debug |
| [Testing](TESTING.md) | Choose the right test suite and understand its limits |
| [Windows 7 acceptance](WINDOWS7_ACCEPTANCE.md) | Run a portable release candidate on the test machines |
| [Lifecycle tests](LIFECYCLE_FIDELITY.md) | Collect Step 1.5 shutdown and performance measurements |
| [Diagnostics](DIAGNOSTICS.md) | Collect logs and failure bundles |
| [Releasing](RELEASING.md) | Package, review and publish a release |
| [Compatibility](COMPATIBILITY.md) | Platform targets, test machines and verified support |

## Understand the backend

| Guide | Purpose |
| --- | --- |
| [Architecture](ARCHITECTURE.md) | Components, input/output flow and resource ownership |
| [Process lifetime and cleanup](PROCESS_LIFETIME.md) | Windows 7 job limits and required cleanup coverage before 1.0 |
| [Versioning](VERSIONING.md) | Package, API and client-agent protocol versions |
| [Naming](NAMING.md) | Migration from WinPTY names to VT7Pty |
| [Security review](SECURITY_REVIEW.md) | Native boundaries and remaining limitations |

## Follow current development

I've completed the scoped input, output and full-screen work in Milestone 1.
Step 1.5 starts with paired lifecycle measurements.
[Screen rc3 acceptance](validation/2026-11-24-step-1.4-screen-rc3-windows7.md#accepted-scope)

H1, H2 and the controlled H3 prototype are accepted on all three Windows 7 tiers. The
[guard review](validation/2026-11-14-step-1.2-host-guards-rc1-windows7.md)
verifies 282/282 cases, including races, actual access denial, native children
and the applicable PowerShell 7 matrix. The opt-in H4 runtime candidate now
implements the approved desktop and close contract. The
[completed Windows 7 rc2 matrix](validation/2026-11-16-step-1.2-h4-runtime-rc2-turtle-rerun.md)
verifies 345/345 runtime cases across all three tiers. The
[rc4 candidate](validation/2026-11-17-step-1.2-h4-runtime-rc4-candidate.md) passes
local qualification and its full opt-in soak. The
[Windows 7 rc4 review](validation/2026-11-18-step-1.2-h4-runtime-rc4-windows7.md)
verifies all automated and elevated-caller checks. The
[manual review](validation/2026-11-19-step-1.2-h4-manual-rc1-windows7.md)
passes all nine physical workflows on those machines and completes H4 within
its approved opt-in scope. The [input rc1 review](validation/2026-11-19-step-1.2-input-rc1-windows7.md)
now confirms three fragmented-input failures on all three Windows 7 tiers;
its frozen H4 guards pass. The [input rc2 review](validation/2026-11-19-step-1.2-input-rc2-windows7.md)
qualifies the corrections on all three tiers, with 387/387 input cases, 345/345 H4
guards, 21 lifetime/fault guards and 42 regressions passing. I completed the
scoped Step 1.2 input work after profile rc3 passed all 30 controlled cases
and 40 physical checkpoints on the same tiers.
[Profile rc3 Windows 7 review](validation/2026-11-23-step-1.2-profiles-rc3-windows7.md)
The released 0.5.0 runtime still has the recorded cancellation defect.

| Guide | Purpose |
| --- | --- |
| [Step 1.3 output characterization](OUTPUT_FIDELITY.md) | Unicode, code pages and independently checked console reconstruction |
| [Step 1.2 profile tests](PROFILE_TESTS.md) | Controlled editors, ordinary profiles and physical input without private transcripts |
| [Step 1.2 input tests](INPUT_FIDELITY.md) | Editing, stream fragmentation, mouse records and native Break reference |
| [H4 runtime qualification](H4_RUNTIME.md) | Accepted opt-in contract, three-machine runner and test scope |
| [H4 manual checks](H4_MANUAL_CHECKS.md) | Physical US/Croatian input, lock/unlock and elevated-caller refusal |
| [Fidelity plan](FIDELITY.md) | Required applications, measurements and remaining coverage |
| [Fidelity test bundle](FIDELITY_BASELINE.md) | Run and interpret the shell and fixture tests |
| [Cancellation investigation](INPUT_CANCELLATION.md) | Failed approaches and what they established |
| [Console-host design](HOST_CANCELLATION_DESIGN.md) | Identity, dispatch, cancellation and production stages |
| [Host identity probe](HOST_IDENTITY_PROBE.md) | Run the H1 research package |
| [Host dispatch probe](HOST_DISPATCH_PROBE.md) | Run the H2 research package |
| [Host cancellation probe](HOST_CANCELLATION_PROBE.md) | Run the first H3 prototype and see its remaining limits |
| [Cancellation diagnostics](HOST_CANCELLATION_DIAGNOSTICS.md) | Collect interleaved rc1 control and traced trials |
| [Scraper coordination experiment](HOST_FREEZE_DIAGNOSTICS.md) | Compare held selection, quiet intervals, activity and delay controls |
| [H3 guard qualification](HOST_GUARDS.md) | Run completion/close races, actual denial, native children and pinned PowerShell 7 checks |
| [Coordinated cancellation prototype](HOST_COORDINATION_PROBE.md) | Qualify baseline coordination, callback completion, expiry and neighbor collection |
| [Optional SSH testing](SSH_BASELINE.md) | Deferred remote-test prerequisites |

## Results and project history

I keep failed experiments and original machine records alongside successful
runs. Each dated review describes the state at that time; the roadmap describes
the current state.

- [Validation index](validation/README.md): candidates, test reviews and raw evidence.
- [0.5.0 release notes](releases/0.5.0.md) and [changelog](../CHANGELOG.md).
- [Completed Milestone 0 checklist](MILESTONE_0.md).
- [Development foundation](FOUNDATION.md) and [component inventory](INVENTORY.md).
- [Upstream provenance](../UPSTREAM.md), [original WinPTY README](UPSTREAM_WINPTY_README.md),
  [upstream release notes](../RELEASES.md) and [historical research](historical/README.md).

## Project maintenance

[Contributing](../CONTRIBUTING.md), [conduct](../CODE_OF_CONDUCT.md),
[security reporting](../SECURITY.md) and [GitHub setup](GITHUB_SETUP.md)
cover the repository workflow. I keep documentation in first person for my
decisions and reviews, use direct language for technical facts, and link to
detailed evidence instead of repeating it. Raw test records keep their original
bytes and results.
