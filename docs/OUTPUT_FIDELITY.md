# Step 1.3 output characterization

I compare the released 0.5.0 DLL and agent with the accepted input rc2 runtime.
Both use the same native fixture and session flags of zero. Rc3 runs the 13
rc2 controls and eight new width cases against each runtime. I use the
measurements to decide which placement differences the stream can preserve.

## Accepted output scope

I use the captured backing-console text as the source for UTF-8/VT output.
I preserve the observed Unicode code points in supported text runs as valid
UTF-8 and require the measured cursor position at settled checkpoints. The
receiving terminal applies its own glyph widths. I require exact cell placement, supported
attributes and dimensions in the measured cases where the native cells
and terminal use compatible widths.
[Output path](ARCHITECTURE.md#output-path-and-fidelity-boundary)

Where the widths differ, I keep the captured text and accept the recorded
combining and CJK column shifts. The cursor-move experiment adds a blank
after the combining sample and erases the ideograph in the narrow CJK
case under the reference reader. A fixed one-cell rule also shifts the
correctly placed CJK sample on WOLF. I preserve the native snapshots,
decoded text, final cursor and measured column differences separately.
The historical verdicts remain evidence, and a new text, cursor or placement
failure gets its own review.
[Width replays](validation/2026-11-24-step-1.3-output-rc3-windows7.md#code-pages-and-replay-results)

I accept the measured F04-F05 scope on all three Windows 7 tiers with
session flags set to zero. The 126 observations cover 36 matching text
and code-page cases, 24 matching screen operations, 48 documented width
differences, and 18 unavailable capability checks. CP936 and native VT
processing for application output were unavailable on these configurations.
I qualify the successfully applied pages and settled snapshots. WOLF
illustrates source text loss when the backing console already stores U+FFFD.
Writes overwritten between polls can escape a capture.
[Windows 7 rc3 review](validation/2026-11-24-step-1.3-output-rc3-windows7.md) ·
[Acceptance record](validation/results/step-1.3/step-1.3-output-scope-2026-11-24.json)

I apply this scope in the Milestone 2 standalone consumer tests. If an
actual VT7 workflow depends on native cell placement for divergent widths,
I use that workflow to evaluate a stronger consumer contract.
[Milestone 2](../ROADMAP.md#milestone-2-define-the-vt7-integration-api)

## Reproduce the accepted comparison

I extract the complete VT7Pty-output-0.5.1-dev-rc3.zip into a short local folder
on each machine. I run the matching launcher from an ordinary Windows session.

| Machine | Launcher |
| --- | --- |
| NESSY, non-ESU | RUN-OUTPUT-NONESU.cmd |
| TURTLE, ESU | RUN-OUTPUT-ESU.cmd |
| LEOPARD, legacy | RUN-OUTPUT-LEGACY.cmd |

The native fixtures drive both phases. The launcher uses the installed Windows
PowerShell, including 2.0 on LEOPARD. I keep the complete run directory under
results and return it to K:\VT7_work\results.

Exit 0 means the collection finished. A complete result can contain differences
or unavailable console features. Exit 2 means a prerequisite or evidence check
failed. I keep incomplete results for diagnosis.

## What I measure

| Case | Operation and independent check |
| --- | --- |
| F04, four Unicode writes | WriteConsoleW sends Croatian text, a decomposed accent, a wide ideograph and a supplementary character in four separate cases. A trailing bar and the cursor let me compare width separately from text. |
| F04, four byte writes | WriteConsoleA uses confirmed output pages 437, 1250 and 65001. The fourth case sends malformed bytes on confirmed page 65001 and records the native write result and observed text. |
| F05, five screen operations | I inspect positioned color, partial erase with colored blanks, right-edge wrap, a clipped scroll with a guard row, and raw VT mode when the console accepts it. |

I hold the child at an initial and an action checkpoint. At each checkpoint I
record the console independently of the VT stream and wait for the corresponding
screen marker in the test reader. A short quiet interval lets the accompanying
output settle after the marker. I retain a bounded observation when the marker
or decoded state differs. Raw VT mode and code-page setup each record the API
result, its error and the applied setting before any dependent write.

A case keeps both native snapshots, the raw output bytes and the comparison
record. For text I keep the intended and observed code points, cursor advances
and write counts. I record the trailing bar's column separately from the
final cursor. For screen operations I compare the cells, colors, geometry
and cursor with the fixture recipe and between the two views. The reader models
the widths of the declared samples. I review host differences before deciding
whether the console, the output stream or the reader explains them.

## Width investigation

I send the same two Unicode strings through WriteConsoleW at the default
output code page and at requested pages 437, 852, 936 and 65001. I check the
applied page before writing and keep the observed font and cell dimensions.
A rejected request records the host error and leaves that sample unrun.

I replay the two default width samples and the eight new cases from the
captured stream. One replay gives just U+0301 and U+4E2D one cell each. The
other uses the original reader and inserts a Cursor Horizontal Absolute
command before the trailing bar at its native column. I compare the bar,
cursor and complete row, plus the sequence of nonblank code points. The
nonblank comparison reveals whether a glyph survived when the replay placed
the bar correctly. I retain the original bytes and the tested insertion
offset. Both runtime binaries come from the same archives used for rc2.
[Cursor positioning](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#cursor-positioning)

## Build and review

I run Package-OutputFidelity.ps1 from committed source on the build machine.
It verifies the source archives by SHA-256, builds the harness and copies the
original runtime binaries beside identical copies of the harness. The paired
run validates its file inventory and the independent VT state checks. I run
tools/output/Test-OutputEvidence.ps1 from Windows PowerShell on the build host.
It checks the complete result directory against the exact extracted package
and repeats the width replays from the preserved bytes and snapshots.
