# Running the VT7Pty 0.5.0 Fidelity Baseline

This is a characterization bundle for Roadmap Step 1.1 and controlled mechanism
research for Step 1.2. It combines the unchanged,
accepted 0.5.0 DLL/agent with a separately identified new harness, fixture and
independent VT state reader. Its manifest records both source identities and
hashes. It is not a new runtime release or Milestone 1 acceptance.

## Windows 7 runs

Extract the entire ZIP into a fresh local directory and use the matching file:

- RUN-FIDELITY-NONESU.cmd: Windows 7 non-ESU tier with Windows PowerShell 5.1.
- RUN-FIDELITY-ESU.cmd: Windows 7 ESU tier with Windows PowerShell 5.1.
- RUN-FIDELITY-LEGACY.cmd: Windows 7 legacy tier with Windows PowerShell 2.0.

The first two default to C:\Program Files\PowerShell\7\pwsh.exe and require
engine 7.2.24. Pass -PowerShell7Path followed by the quoted executable path
to a CMD launcher if the installation differs. Tier C uses its installed
PowerShell 2.0; PowerShell 7 cases are explicitly NotApplicable there.

The runner validates every packaged file and the unchanged runtime hashes.
It checks application/module hashes before and after the run, and records
OS/update identities, console modes/code pages and console fonts,
state/cursor/attribute comparisons, timing, controlled terminal bytes,
fixture snapshots, failures and cleanup. Keep the complete results directory.

Run the package's tests on x64 Windows using Windows PowerShell. The target
runner supports PowerShell 2.0 and does not require KB3191566. Local build and
packaging commands use the existing development tools and modern PowerShell.

## Interpreting results

Harness version 3 has 37 entries: the state/child-oracle unit suite, a portable
JSON round-trip oracle, 25 workflow observations and ten deliberate negative controls. Controls pass only when
their exact injected failure is detected and owned processes are cleaned up.
The native control record therefore says Fail while the containing control
entry says Pass; both remain visible.

Normal workflow failures are preserved as Fail. A missing/wrong pinned shell
is NotRun. The runner exits nonzero if required evidence is missing, the harness
fails, or a workflow fails. A nonzero result is useful baseline evidence;
return the complete records rather than changing statuses to Pass.

CharacterizationComplete means the implemented initial cases were observed
with a healthy harness. PendingCoverage separately lists required work not yet
implemented. MilestoneAccepted remains false. No failure is automatically
classified as a Windows 7 backend defect based on a newer development console.

The reader models the emitted sequence subset and uses declared corpus width
rules for combining text, CJK samples and the selected emoji. It does not
implement general Unicode width policy or terminal reflow. Resize mismatches
need review against the recorded stream, snapshots and consumer resize policy.
Text and cursor-width results are separate. RawUtf16CellDifferences is a
diagnostic count and is not the normalized Unicode text oracle.

Automation uses clean shell profiles. Installed Windows PowerShell explicitly
removes PSReadLine in the test session and verifies native ConsoleHost editing.
This prevents ambient module discovery from silently changing the editor.
PowerShell 7 imports its bundled PSReadLine
with Windows editing, prediction disabled, and session-local history configured
to SaveNothing. The loaded engine/editor versions, module path and editing/history
configuration are captured through the tested session.
Ordinary profiles, full key/mouse/layout coverage, distinct Ctrl+Break, the
broader code-page matrix, automatic buffer restoration and target performance
budgets remain listed in PendingCoverage.

## Active-child and cooked-read recipes

Harness version 3 includes nine F02 cases, three for each shell:

| Suffix | Child input/control policy | Expected native completion |
| --- | --- | --- |
| CHILD_HANDLED | Processed Ctrl+C; installed handler observes exactly one CTRL_C_EVENT | Fixture writes final output and deliberately exits 37 |
| CHILD_DEFAULT | Processed Ctrl+C with the default handler and inherited ignore cleared | Native exit status 0xC000013A; no final-output marker required |
| CHILD_UNPROCESSED | Ordered Ctrl+C and z key records; no control event, released Ctrl modifier | Fixture writes final output and deliberately exits 37 |

Every case validates the private pipe peer, executable image and direct parent
before adopting process cleanup ownership. It records initial/latest native packets
(default-handler termination has no final packet),
actual input mode, handler/key observations, independently queried process exit
status, the shell's reported exit code, final output where applicable, return to
the prompt and execution of the next command through a file barrier. Missing
interruption is a Fail; the test asks only its owned fixture to stop and preserves
the failed observation. Each child has a 45-second fixture lifetime limit.
Harness startup has a 20-second shell deadline, interruption and return-to-prompt
each have a 5-second deadline, and the outer child case limit is 90 seconds.

READCONSOLE_TRANSPORT and READCONSOLE_EVENT isolate a pending cooked ReadConsoleW
call after an observed PROBE_TYPED echo barrier. The first sends VT byte 0x03
through the unchanged backend; the second invokes GenerateConsoleCtrlEvent
inside the owned fixture through its private test channel. Both separately
record handler delivery and read completion within two seconds. Cancellation
requires the read to return without the typed text. If it stays pending, the
test preserves that observation, then sends a boundary Enter and records what
the read actually returned. Recovery does not convert a failed cancellation
into a pass. The fixture owns and joins its read thread; cancellation during
cleanup never frees the read buffer before the thread has returned.

Two new fault controls mutate the child oracle's input control count and native
exit status respectively. Raw fixture observations remain unchanged. Independent
unit cases reject duplicate controls, wrong event types, unreleased modifiers,
extra/repeated keys, malformed packets and out-of-bounds text/key counts.

The seven remaining PendingCoverage groups still need broader recipes. This
extension does not claim Ctrl+Break, ordinary profiles, keyboard-layout coverage,
general Unicode width, sustained full-screen resize or performance qualification.

The JSON_ORACLE case runs under the same Windows PowerShell major version as
the target runner. It proves that nested dictionaries, Keys/Values/Count/Item
members, arrays, booleans, null, Unicode and unsigned control values survive the
portable report serializer. The source and round-trip JSON are preserved.

## Version 4 cancellation mechanism probes

Version 4 retains the 37 version 3 entries and adds two MechanismProbe entries,
for 39 total. These are experiments inside the owned fixture, not backend fixes.

READCONSOLE_RECORD_EVENT writes C down/up records with character 0x03 and
LEFT_CTRL_PRESSED, then generates one Ctrl+C control event. It records API
success/error and number of records written as well as the pending read result.
A later boundary Enter is recovery evidence if cancellation fails.

READCONSOLE_OWNED_CANCEL calls CancelSynchronousIo on the exact pending
ReadConsoleW worker owned by the fixture, then generates one Ctrl+C event.
The first read must return zero text with ERROR_OPERATION_ABORTED (995); its
BOOL result is recorded separately and may be TRUE. The test then joins that
worker and explicitly starts a second read. NEXT_PROBE followed by Enter must
produce exactly NEXT_PROBE plus CRLF, with no extra control event. The next
read has a five-second deadline and another five seconds for boundary Enter
recovery. The initial cancellation deadline remains two seconds and the outer
case deadline remains 60 seconds.

The fixture has no later operation on the canceled worker, and keeps all buffers
alive until join. It does not discover or cancel real shell threads. API success
or an empty first read cannot qualify the mechanism if the next line is lost.
Preserved metadata and binary snapshots distinguish the initial cancellation,
second-read readiness, normal completion and recovery.

The [cancellation investigation](INPUT_CANCELLATION.md) records mechanism
constraints and development findings. MechanismFailures is recorded separately from WorkflowFailures; a failing probe
also makes the runner return exit code 1. Harness/prerequisite errors and cleanup
failures in these probes make HarnessHealthy false. Both probes still require
Windows 7 measurement. Their results do not replace the rc3 target baseline or resolve F01.

## Recorded initial baseline

The [candidate review](validation/2026-10-12-step-1.1-candidate.md) identifies
step-1.1-rc1 and its development-host observations. The
[Windows 7 review](validation/2026-10-12-step-1.1-windows7-baseline.md) preserves
the same candidate's three target runs: NESSY and TURTLE each report 19 Pass /
4 Fail; LEOPARD reports 17 Pass / 4 Fail / 2 NotApplicable. All four failures
concern Command Prompt and installed Windows PowerShell prompt cancellation.
The controlled Unicode and resize cases pass on all three targets; PowerShell 7
passes on A/B. Oracles and fault controls pass, with no harness failures.

These are initial characterization results. Step 1.1 remains open for the
PendingCoverage recipes, and failing workflows have not been accepted.

## Current extension candidate

[Step-1.1-rc3](validation/2026-10-12-step-1.1-rc3-candidate.md) has completed its
[three-tier target characterization](validation/2026-10-12-step-1.1-rc3-windows7-baseline.md):
NESSY and TURTLE each report 31 Pass / 6 Fail; LEOPARD reports 26 Pass / 6 Fail /
5 NotApplicable. Every applicable active-child case passes. Both oracles and all
ten fault controls pass on every target, including portable JSON under PowerShell 2.0.

The same six cancellation cases fail on all targets: four native-editor prompt
cases and two cooked-read probes. Each probe delivers one Ctrl+C event but leaves
ReadConsole pending with retained typed text. These results supplement the
separate development-host comparison of 29 Pass / 8 Fail; bounded Unicode and
resize pass on Windows 7.

Rc2 was stopped before handoff because its aggregate serializer lost nested
child metadata. Rc3 corrects that and verifies all native/aggregate observations.
The target review preserves complete failed records as baseline evidence;
seven remaining coverage groups keep Step 1.1 open. Future candidates must
retain actual failures and obtain their own applicable target evidence.

## Development commands

From a clean checkout, build/verify and create an identified candidate:

    powershell.exe -NoProfile -File .\Package-FidelityBaseline.ps1 -Suffix step-1.2-mechanism-rc1

The command verifies the existing regression suite and new binary imports,
runs the independent oracle tests, and refuses to replace an existing candidate
or use a modified 0.5.0 runtime archive. Candidate ZIPs and checksums are under
artifacts\fidelity\packages. Matching harness PDBs are included; existing
0.5.0 symbols remain in the published symbols archive.

For a development-host comparison, run the extracted package with:

    powershell.exe -NoProfile -File .\tools\fidelity\Invoke-FidelityBaseline.ps1 -Tier DevelopmentHost

The development host may lack exact PowerShell 7.2.24; that stays NotRun.
The original 30-case suite and 120-minute Windows 7 release soak remain separate
regression/qualification gates. This short bundle does not replace them.
