# VT7Pty roadmap

Updated: 2026-11-23.

I've published 0.5.0 and completed Step 1.2 within the recorded input scope.
Profile rc3 passed the controlled fixtures, physical shell checks and frozen
controls on all three Windows 7 tiers. The next step is output and Unicode
reconstruction. The broader baseline and VT7 integration remain open.
[Profile rc3 qualification](docs/validation/2026-11-23-step-1.2-profiles-rc3-windows7.md)

I check an item only when its stated work and required tests are complete.
Partial implementations stay unchecked. A local pass does not replace a required
Windows 7 run. Detailed results live in the [validation index](docs/validation/README.md).

## Goal and boundaries

My goal is a reliable user-mode backend for VT7 on Windows 7 SP1 and later,
built on WinPTY and informed by ConPTY. I work on VT7Pty here and use VT7 and
Microsoft Terminal as references. Integration into VT7 is a separate task.
Other consumers and a possible ConPTY shim come later. No kernel driver is planned.

The [foundation](docs/FOUNDATION.md) records the engineering decisions;
[compatibility](docs/COMPATIBILITY.md) defines the platform and test matrix.

## Milestone overview

| Milestone | Outcome | Status |
| --- | --- | --- |
| 0 | Modern native VT7Pty 0.5.x foundation | Complete; 0.5.0 accepted and published |
| 1 | Better console fidelity and reliability | In progress. Step 1.3 output and Unicode reconstruction is next |
| 2 | VT7-facing API and standalone integration host | Planned after backend validation |
| 3 | Versioned handoff ready for VT7 integration | Planned after API and consumer tests |
| Later | Other consumers and optional ConPTY compatibility | Deferred |

## Milestone 0: Establish the modern VT7Pty foundation

I completed these steps before publishing the baseline:

- [x] 0.1: Record the upstream behavior and component inventory.
- [x] 0.2: Replace the build and developer infrastructure.
- [x] 0.3: Remove obsolete infrastructure and the Unix adapter.
- [x] 0.4: Establish Windows 7 SP1 x64 as the platform floor.
- [x] 0.5: Complete technical rebranding and the 0.5.x identity.
- [x] 0.6: Modernize the retained native code.
- [x] 0.7: Improve diagnostics and review native security boundaries.
- [x] 0.8: Establish the permanent test system.
- [x] 0.9: Establish the manual release process.

The [detailed checklist](docs/MILESTONE_0.md) preserves the original requirements
and links to their evidence. The final
[0.5.0 package passed release acceptance](docs/validation/2026-10-02-0.5.0-release-acceptance.md)
separately from the earlier development candidates, then was
[published](docs/validation/2026-10-02-0.5.0-publication.md).
This baseline does not claim complete terminal fidelity or VT7 integration.

## Milestone 1: Improve backend fidelity and reliability

I compare changes against the released 0.5.0 runtime. The
[fidelity plan](docs/FIDELITY.md) defines the application corpus and F01-F09
measurements. Console-SSH and remote full-screen tests are
[optional later work](docs/SSH_BASELINE.md); VT7's direct SSH transport bypasses VT7Pty.

### Step 1.1: Establish the local fidelity harness and baseline

- [x] Pin Command Prompt, installed Windows PowerShell, PowerShell 7.2.24 and
  controlled native fixtures, including which test machines require each.
- [ ] Complete the bounded VT state reader, synchronized fixture observations,
  portable reports and tests that deliberately inject faults.
- [ ] Measure the exact 0.5.0 runtime on the development host and all three
  Windows 7 tiers. The initial corpus is measured; seven broader groups remain.
- [ ] Record missing prerequisites, limitations and reproducible failures, then
  prioritize fixes using VT7Pty results.

The [rc3 review](docs/validation/2026-10-12-step-1.1-rc3-windows7-baseline.md)
records six cancellation failures per machine and passing applicable active-child
tests. I retain those failures as the baseline. This step stays open until the
required workflows have identified evidence and the harness can detect incorrect
state and input. The [runner guide](docs/FIDELITY_BASELINE.md) lists remaining coverage.

### Step 1.2: Improve input and control semantics

I completed the scoped input work using the console-host design. I keep the
earlier public API experiments and their failures in the investigation.
[Console-host design](docs/HOST_CANCELLATION_DESIGN.md) ·
[Cancellation investigation](docs/INPUT_CANCELLATION.md)

- [x] H1: Verify host identity and isolation on all three Windows 7 tiers.
  [Results](docs/validation/2026-10-13-step-1.2-host-identity-rc1-windows7.md).
- [x] H2: Verify actual host dispatch and a session-bound request/acknowledgment
  channel, including stale requests, failures and isolation.
  [Results](docs/validation/2026-10-13-step-1.2-host-dispatch-rc1-windows7.md).
- [x] H3: Cancel a pending cooked read, discard the old line, deliver exactly
  one control event, preserve the next read and recover real shell prompts.
  The [Windows 7 guard review](docs/validation/2026-11-14-step-1.2-host-guards-rc1-windows7.md)
  accepts nine guard trials and 282/282 cases, including completion/close races,
  actual access denial, active children and the applicable PowerShell 7 matrix.
  Earlier [controlled contrasts](docs/validation/2026-11-13-step-1.2-host-freeze-diagnostics-rc1-windows7.md)
  established selection as a delivery blocker; the
  [coordinated baseline](docs/validation/2026-11-13-step-1.2-host-coordination-rc1-windows7.md)
  qualified protected dispatch and recording. Original failures remain in the
  [validation history](docs/validation/README.md). H3 accepts the controlled
  private-desktop prototype; ordinary-desktop and production lifetime work belong to H4.
- [x] H4: Integrate the proven mechanism into the runtime and pass regression tests.
  I accept the [opt-in H4 contract](docs/H4_RUNTIME.md), including private backing
  desktops and API 1.1 two-phase close, on all three Windows 7 tiers. The
  [rc4 automated review](docs/validation/2026-11-18-step-1.2-h4-runtime-rc4-windows7.md)
  verifies 345 runtime, 21 qualification and 90 regression cases, both full
  soak phases per machine and the elevated-caller checks. The
  [manual review](docs/validation/2026-11-19-step-1.2-h4-manual-rc1-windows7.md)
  completes all nine physical keyboard and lock/unlock workflows, with 12
  restored keyboard operations and 12 cleaned session lifetimes. Broader input
  fidelity remains below; descendant/job policy remains in Step 1.5.
- [x] Correct the measured UTF-8 and cursor-reply fragmentation failures and
  qualify the fixes on all three Windows 7 tiers. The
  [rc2 review](docs/validation/2026-11-19-step-1.2-input-rc2-windows7.md) verifies
  387/387 input cases, 345/345 H4 guards, 21 lifetime/fault guards and 42
  regressions. The unchanged rc1 reference retains its three failures per machine.
- [x] Qualify controlled profiles and ordinary PowerShell startup on all three
  Windows 7 tiers. Profile rc3 passes 30/30 controlled cases, 27/27 publication
  checks and 40/40 physical checkpoints. The five manual shells use their
  normal configurations with empty profile scopes. The controlled fixtures
  cover profile precedence. [Windows 7 review](docs/validation/2026-11-23-step-1.2-profiles-rc3-windows7.md)
- [x] Record the accepted F01-F03 scope. I qualify prompt and active-child
  Ctrl+C, editing and the documented records and byte streams within the
  opt-in H4 contract. Ctrl+Break is covered by native references and has no
  public transport. Standalone Escape latency and cursor-report/F3 ambiguity
  go into the Milestone 2 input contract.
  [Scope and evidence](docs/validation/2026-11-23-step-1.2-profiles-rc3-windows7.md#step-12-scope)

The released 0.5.0 runtime still has the original prompt cancellation defect.
H1, H2 and H3 remain the research evidence for the H4 implementation.

### Step 1.3: Improve output and Unicode reconstruction

- [ ] Compare the expanded [output corpus](docs/OUTPUT_FIDELITY.md) on the
  released and accepted runtimes across all three Windows 7 tiers.
- [ ] Resolve measured F04-F05 failures in cells, cursor, supported attributes,
  erase, wrap, scroll, encoding and requested/actual code-page behavior.
- [ ] Record text and width results separately; prove a narrow adaptation or
  document my scope decision for information lost by the backing console.

I'll complete this step when stable states and text match independently checked
expectations, with raw-VT, width and intermediate-state limits documented.

I qualified the automated output characterization package locally and verified
its delivery to the Windows 7 test share. The target comparison is next.
[Output rc2 candidate](docs/validation/2026-11-23-step-1.3-output-rc2-candidate.md)

### Step 1.4: Improve full-screen restoration and resize

- [ ] Resolve measured F06-F07 failures in active-buffer transitions, return to
  the original screen, shell repaint and authoritative child dimensions.
- [ ] Exercise repeated resize during editing/output and application exit.

I'll complete this step when full-screen fixtures and shells reach the required
state after resize and exit, without unexplained lasting corruption or hangs.

### Step 1.5: Validate lifecycle and resource behavior under real workloads

- [ ] Resolve measured F08-F09 failures in final drain, child status, close,
  failure cleanup, response times and resource stability.
- [ ] Before 1.0, qualify Windows 7 job inheritance, permitted/denied breakaway,
  abrupt owner exit and descendant cleanup. Follow the
  [process lifetime plan](docs/PROCESS_LIFETIME.md), prompted by
  [VSoft.Process PR #18](https://github.com/VSoftTechnologies/VSoft.Process/pull/18).
- [ ] Preserve existing lifecycle/stress gates and establish target-derived
  performance budgets before accepting performance changes.

I'll complete this step when application workloads meet the defined lifetime
and resource limits and preserve the accepted 0.5.0 regression suite.

### Step 1.6: Review and publish the improved backend results

- [ ] Run the exact milestone candidate against the required corpus and existing
  suite on all designated Windows 7 tiers, including the full soak.
- [ ] Resolve required failures or document my decisions about scope; publish
  exact identities, capabilities, limitations and performance measurements.
- [ ] Link acceptance evidence and the remaining VT7-facing contract requirements
  before Milestone 2 API work.

I'll accept the milestone when the required local workflows meet their Windows
7+ fidelity and reliability criteria with no unexplained baseline regression.

I'll test the consumer boundary with a narrow API and standalone host in this
repository. Changes to VT7 itself belong to a separate integration task.

## Milestone 2: Define the VT7 integration API

- [ ] Review VT7's connection requirements as reference material without
  modifying the VT7 repository.
- [ ] Specify handle ownership, stream encoding, errors, threading, cancellation,
  resize/close races, output draining, process lifetime, job inheritance and
  breakaway policy, version negotiation, and distribution requirements before
  freezing an ABI.
- [ ] Resolve the [input/reply framing](docs/INPUT_FIDELITY.md#input-and-reply-contract)
  policy before freezing the API, including standalone Escape latency and the
  cursor-report/modified-F3 ambiguity of the inherited byte stream.
- [ ] Implement documented headers and the API over the proven backend.
- [ ] Build a standalone consumer and contract tests in this repository.
- [ ] Validate the contract on Windows 7 and publish examples and migration
  guidance from the renamed inherited interface.

I'll accept the API when its documented behavior, contract tests, examples and
standalone consumer pass against the Windows 7 backend.

## Milestone 3: Prepare the VT7 handoff

- [ ] Assemble identifiable binaries, symbols, headers, licenses, provenance,
  compatibility claims, and test evidence.
- [ ] Validate the intended consumer boundary through the standalone host.
- [ ] Document integration steps, remaining risks, and a fallback strategy.

I'll complete the handoff when the backend is ready for a separate VT7
integration task. Completing this milestone does not mean VT7 is integrated.

## Later work

After VT7 readiness, I'll evaluate other consumers and an optional user-mode shim
for specific ConPTY clients. Process creation and attachment semantics are part
of that work, not just export names. Each supported client needs an explicit
contract and tests. No kernel driver is planned.

## Maintaining this roadmap

I keep plans separate from results and record scope changes explicitly.
Failed and partial results stay in the validation history. Procedures and
measurements belong in the linked guides so this roadmap stays readable.
