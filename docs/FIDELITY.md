# Milestone 1 Fidelity Plan

Status: initial Step 1.1 harness implemented and exercised locally; Windows 7 characterization and broader recipes pending.
Updated: 2026-10-12.

The [roadmap](../ROADMAP.md) owns completion. This document defines the local
application corpus, measurements, and evidence needed to improve VT7Pty after
the [published 0.5.0 baseline](validation/2026-10-02-0.5.0-publication.md).
Planning does not qualify an application or complete an implementation step.

## Scope and application corpus

Milestone 1 targets VT7's local console workflows. Implementation and tests
belong in VT7Pty; VT7 is a reference repository. Console-SSH and remote Vim/htop
testing are deferred optional scenarios and do not block local acceptance.
VT7's direct SSH transport bypasses this backend. The
[remote baseline record](SSH_BASELINE.md) retains the prerequisites for later use.

| Application | Required role | Identity and configuration |
| --- | --- | --- |
| Command Prompt | Editing, prompt cancellation, active-child interruption, native children, exit | System x64 cmd.exe; record file version/hash and launch arguments |
| Windows PowerShell 5.1 | Local shell and ConsoleHost editing on tiers A/B | Record executable/engine version, hash, loaded editor modules, and clean/ordinary profile variants |
| Windows PowerShell 2.0 | Local shell on legacy tier C without KB3191566 | Use the installed engine; record the same identities; no machine upgrade required |
| PowerShell 7.2.24 x64 | Match VT7's pinned PowerShell 7 consumer on designated tiers A/B | Record binary/package hashes and loaded PSReadLine version/configuration before qualification; provision app-locally |
| Repository-owned console fixtures | Deterministic input records, screen state, buffer transitions, resize, child lifetime | Build/package with exact source identity, controlled input and expected state |

PowerShell 7 availability is an application-test prerequisite, not a native
runtime requirement. Tier C must complete the native fixture and installed
shell matrix. PowerShell 7 there is optional and may be NotApplicable with its
reason. A missing required shell or fixture on a designated tier is NotRun
and blocks that gate. Newer PowerShell versions do not qualify 7.2.24.

The initial full-screen corpus uses native Win32 console fixtures. Additional
applications need an exact version, distribution/license record, and reproducible
scenario before becoming gates. Automated shell runs use clean profiles;
ordinary profiles have a separately recorded manual check. Do not change global
profiles, execution policy, console settings, or installed code pages.

## Measurement matrix

These IDs define scenario groups rather than one completed test each. The
initial [baseline bundle](FIDELITY_BASELINE.md) implements 14 bounded workflow
observations, an independent oracle unit suite and eight fault controls.
Its case manifest lists remaining coverage as NotRun. The existing 30-case
suite remains a required regression gate.

| ID | Scenario | Required observable result |
| --- | --- | --- |
| F01 | Ctrl+C at empty and partially typed shell prompts | Typed line canceled, canceled text never executed, fresh prompt accepts the next command, shell remains alive |
| F02 | Ctrl+C/Ctrl+Break during an active child; processed/unprocessed input | Record mode and expected shell/child policy; verify the intended event/key representation, no duplicate delivery, expected child status and usable parent prompt |
| F03 | Editing and input records | Text, Enter, Tab, Backspace, Delete, arrows, Home/End, supported modifiers and paste arrive in order with correct records; repeat and fragmented UTF-8 do not duplicate text |
| F04 | Unicode and code pages | Ordinal text comparison, valid stream decoding, separate cursor/width results and requested-versus-actual code-page reporting; Croatian, combining, wide and supplementary text |
| F05 | Positioned output, attributes, erase, wrap and scroll | Reconstructed cells, supported attributes, cursor and dimensions match expected stable state at a synchronization barrier |
| F06 | Alternate console buffers and shell return | Correct active-buffer content/cursor; original screen/prompt restored after explicit restoration and application exit |
| F07 | Resize while editing and drawing full-screen output | Child and host dimensions converge; final layout/cursor match; no hang, lost final marker or lasting corruption |
| F08 | Child exit, final output, close and failure | Final output drains before completion, exit status preserved, bounded shutdown with no owned agent/child left behind |
| F09 | Response and resources | Record input/resize/barrier latency, idle/active CPU, memory and handles for repeatable loads; enforce published case budgets |

Manual layout checks initially cover US and Croatian keyboards, including
AltGr, dead keys and repeat. Automated record tests do not prove host UI focus,
IME composition or physical keyboard-layout behavior. Test mouse delivery in
a controlled supported console input mode. Host selection, clipboard shortcuts
and IME UI remain consumer responsibilities; backend delivery of committed text
and key/mouse records is in scope.

## Oracles and synchronization

SessionContractTest currently strips CSI sequences for marker checks. That
supports lifecycle tests but cannot establish cursor, attributes, erase,
screen restoration or cell fidelity.

Add a bounded test-only VT state reader for sequences emitted by VT7Pty. Prove
it independently with hand-authored cell-placement, erase, scrolling, attribute,
cursor and fragmentation tests. Unsupported sequences relevant to a case must
fail rather than disappear. Compare decoded output with explicit fixture
expectations and controlled child-console snapshots captured through a separate
test channel. Snapshots identify buffer, geometry, cursor, modes, code pages
and supported attributes.

Use child readiness/completion barriers, keep output reading active, and
compare stable states; fixed sleeps alone are not an oracle. Polling cannot
guarantee every rapidly overwritten intermediate state. Final-screen equality
does not establish original-byte preservation. Test raw VT separately from
Win32 console writes and record loss already imposed by the backing console.

Compare Unicode ordinally and record text preservation separately from cursor
width. If a requested code page was rejected, do not send bytes under that
mapping. Known width/raw-VT limitations need documented scope decisions.

Negative controls must detect wrong cells/cursor/attributes, duplicate input,
lost final output, timeouts and orphans. Do not reuse backend emitter/scraper
logic as the expected-result oracle.

## Execution and budgets

1. Implement the external harness and controlled fixtures; prove their oracles
   and negative controls before changing backend behavior.
2. Exercise the accepted 0.5.0 runtime with that harness and record both
   identities. Keep release binaries unchanged. Disclose instrumentation where
   needed; an instrumented run is not an unchanged-release result.
3. Run baseline scenarios on tiers A, B and C, preserving failures and
   distinguishing Windows 7 behavior from the development host's console.
4. Prioritize reproducible failures, starting with F01/F02. Each fix carries
   focused regressions and existing local verification. Routine target
   candidates use tier A; milestone/release candidates use all three tiers.

Before a case becomes a gate, commit exact input, expected state, synchronization,
timeout, cleanup and resource budgets. Initial harness hang limits are 30 seconds
for short controlled cases and 10 seconds for shutdown; application startup and
long stress tests have separate named deadlines. These are hang limits, not
interactive latency targets. Revise them only with recorded evidence. Establish
latency and CPU/memory budgets from repeated 0.5.0 target measurements before
accepting performance changes. Preserve existing 500-session, 100-resize,
four-handle post-warm-up and 120-minute milestone/release stress gates.

The portable runner remains PowerShell 2.0 compatible. Extend the manifest and
case-level reports as cases are implemented. The separate initial fidelity
bundle does not replace or extend the original 30-case release gate by itself.
Exact-package qualification follows the
[manual release process](RELEASING.md).

## Evidence and disposition

Record source/package/harness identity, machine/tier/update state, application
hash/version/profile/editor, console modes and actual code pages, geometry,
input recipe, expected/observed state, status, timings and cleanup. Controlled
fixture content may be retained; ordinary user session content and credentials
must not enter default diagnostics.

Classify failures with reproductions as backend regressions, inherited/backend
limitations, backing-console limitations, harness defects or missing prerequisites.
VT7 evidence is a lead, not a VT7Pty pass/fail result. Characterize F01 against
0.5.0 before fixing it: VT7's finding used WinPTY 0.4.3, while VT7Pty derives
from 0.4.4-dev. Reference records:

- [VT7 WinPTY characterization](https://github.com/TheBinaryShadow/VT7/blob/85d2fb1bcdd5bd0c5952d12fb2013d38790062d0/doc/vt7/validation/2026-09-14-winpty-p01.md)
- [VT7 prompt-cancellation finding](https://github.com/TheBinaryShadow/VT7/blob/85d2fb1bcdd5bd0c5952d12fb2013d38790062d0/doc/vt7/validation/2026-09-17-winpty-root-3b.md)
- [VT7 PowerShell profiles](https://github.com/TheBinaryShadow/VT7/blob/85d2fb1bcdd5bd0c5952d12fb2013d38790062d0/doc/vt7/validation/2026-09-17-powershell-profiles-3b2.md)

Required workflow failures block acceptance until fixed or explicitly reconsidered
with owner-approved scope and preserved evidence. Optional/inapplicable cases
retain their real status. Publish capabilities and limitations with the accepted
candidate; launching or showing a prompt is insufficient.

Milestone 1 completes when the required corpus passes on designated Windows 7
tiers, regressions/resource failures are resolved, and limitations have explicit
dispositions. Milestone 2 then specifies the VT7-facing API and validates a
standalone consumer in this repository. General ConPTY compatibility is later work.
