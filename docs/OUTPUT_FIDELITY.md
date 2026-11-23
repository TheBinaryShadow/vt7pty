# Step 1.3 output characterization

I compare the released 0.5.0 DLL and agent with the accepted input rc2 runtime.
Both use the same native fixture and session flags of zero. The 13 cases run
once against each runtime. I use their differences to decide where to fix
output reconstruction.

I completed all three Windows 7 runs in rc2. The paired evidence reproduces
column differences for a combining mark and a CJK ideograph. The procedure
below reproduces that matrix.
[Windows 7 review](validation/2026-11-23-step-1.3-output-rc2-windows7.md)

## Run the candidate

I extract the complete VT7Pty-output-0.5.1-dev-rc2.zip into a short local folder
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

## Build and review

I run Package-OutputFidelity.ps1 from committed source on the build machine.
It verifies the source archives by SHA-256, builds the harness and copies the
original runtime binaries beside identical copies of the harness. The paired
run validates its file inventory and the independent VT state checks. I use
tools/output/Test-OutputEvidence.ps1 with the complete result directory and
exact extracted package to check the evidence before changing runtime code.
