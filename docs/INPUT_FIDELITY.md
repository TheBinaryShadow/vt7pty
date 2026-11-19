# Step 1.2 input tests

I use this automated candidate to measure the remaining input behavior on
NESSY, TURTLE and LEOPARD. The DLL, agent, host module and collector are copied
unchanged from accepted H4 rc4. I preserve its
[acceptance](validation/2026-11-19-step-1.2-h4-manual-rc1-windows7.md).

This is a characterization candidate. It retains three development failures:
a delayed UTF-8 character can be lost, and cursor-position replies can disrupt
fragmented UTF-8 or escape input. These remain failed tests. They are not
intentional fault controls or expected passes. I need the Windows 7 observations
before deciding on the decoder/transport changes. Step 1.2 remains open.

## Run on the Windows 7 machines

Extract VT7Pty-input-0.5.1-dev-rc1.zip into a fresh local folder. Run as the
ordinary user, with Windows unlocked and other VT7Pty debug collectors closed.

| Machine | Launcher |
| --- | --- |
| NESSY, non-ESU | RUN-INPUT-NONESU.cmd |
| TURTLE, ESU | RUN-INPUT-ESU.cmd |
| LEOPARD, legacy PowerShell 2.0 | RUN-INPUT-LEGACY.cmd |

The launcher performs three input repetitions, interleaved with three unchanged
H4 guard runs. It continues collecting after a semantic test failure. Keep
every file under results, including failed or partial attempts, and return
the complete machine directories to K:\\VT7_work\\results.

No physical typing, lock/unlock, elevation or two-hour soak repeat is requested.
Keep the machines unlocked until the launcher finishes.

NESSY and TURTLE use the existing PowerShell 7.2.24 x64 installation at
C:\\Program Files\\PowerShell\\7\\pwsh.exe for their frozen H4 shell guards.
A different path can be supplied with -PowerShell7Path. LEOPARD runs the
33-case legacy guard inventory under installed Windows PowerShell 2.0 and
CLR 2.0, without KB3191566 or PowerShell 7.

Exit code 0 means the collected native suites pass. Code 1 means collection
finished with actual test failures; keep those results for review. Code 2
means prerequisite or collection work is incomplete. The result report never
accepts Step 1.2 automatically.

## What I measure

| Area | New coverage and retained guards |
| --- | --- |
| F01 prompt cancellation | Frozen H4 empty/partial CMD and PowerShell prompt cases, clean PSReadLine where applicable, next command and live shell |
| F02 events and children | Frozen H4 active-child guards; new processed/unprocessed Ctrl+C capture and a distinct native Windows Ctrl+Break reference |
| F03 key records | Text, Enter, Tab, Backspace, Delete, Insert, arrows, Home/End, Page Up/Down, explicit Shift/Ctrl/Alt chords and released modifier state |
| F03 editing | Cooked Unicode lines, insertion/deletion, Home/End, Tab, repeated input, multiline committed text and cancellation of a Unicode line |
| F03 stream handling | Repeated text, two/three/four-byte UTF-8, supplementary UTF-16 pairs, combined and bytewise writes, delayed fragments and live cursor replies |
| F03 mouse records | Negotiated SGR and X10 buttons, movement, vertical wheel, modifiers, coordinates and disabled-mode gating |

The new suite has 33 cases on every tier. Each frozen H4 guard run has 41 cases
on NESSY and TURTLE, and 33 on LEOPARD. I keep the two harness source identities
separate. The guard executable and all product files retain their original
rc4 hashes; only the input test executable is rebuilt.

I capture complete key-down/up records, scan codes, repeat counts, modifiers,
mouse records, input modes and actual cooked readbacks. Raw cases include a
distinct private-use completion character, then a bounded observation interval
for trailing records. I require balanced generated key state and an exact
ordinary read after each passing case. This interval is not a latency benchmark.

Mouse input starts after the agent's terminal-mode enable sequence has actually
been observed. An early prototype sent mouse frames before negotiation and lost
them; I preserve that harness failure separately. The corrected cases exercise
both burst and fragmented frames after the same readiness proof.

## Fragmented-input findings

The normal fragmented cases hold terminal replies until the logical input has
been written. Two additional cases service the terminal while each token is
still arriving. I retain the actual cursor replies, application input records
and emitted VT for comparison.

In development, a 1,200 ms gap between UTF-8 bytes loses the incomplete character.
The agent's general incomplete-input timer clears the byte queue. Shorter
fragmented Unicode arrives correctly when no reply interrupts its token.

With live replies, the cursor-position response can land inside the incomplete
token. I observe missing Unicode or literal escape/coordinate text in the
application's queue. These cases expose an input framing issue that needs an
explicit contract and a tested fix. The earlier H4 cancellation gate remains
accepted within its recorded scope.

## Ctrl+Break and remaining limits

I preserve API 1.1 and protocol 2 for this candidate, as approved. The public
byte input stream has no distinct Ctrl+Break operation. Byte 0x1C is tested as
a text/control-byte input; it is not relabeled as Break.

The separate native reference creates a retained owned child with
CREATE_NEW_PROCESS_GROUP inside the test's private backing console.
GenerateConsoleCtrlEvent targets that child's group. I require exactly one
CTRL_BREAK_EVENT, zero Ctrl+C events, no notification of the parent fixture,
the expected input mode and natural child exit. Both processed and unprocessed
modes are tested while the child is configured to ignore Ctrl+C. Microsoft
documents [group targeting](https://learn.microsoft.com/en-us/windows/console/generateconsolectrlevent)
and [control-handler semantics](https://learn.microsoft.com/en-us/windows/console/handlerroutine).

A passing native reference does not establish a VT7Pty Break transport. That
capability remains explicitly unsupported in the manifest and reports.
I also retain the ordinary-user-profile matrix as NotRun. This package uses
the controlled clean-shell and PSReadLine guards and does not execute personal
profile scripts. Clipboard UI, physical held-key behavior and IME UI remain
separate from the committed text and record delivery tested here.

## Build and review

Package-InputFidelity.ps1 requires clean committed source and the exact
accepted H4 rc4 ZIP. It verifies frozen hashes, builds the new harness, checks
its oracle and compares the compiled recipes with tools/input/cases.json.
The package includes symbols, provenance, credits and PowerShell 2.0-compatible
collection helpers. It adds no CI or target-side build requirement.

Test-InputEvidence.ps1 independently recomputes each outcome from the golden
recipes, raw records, actual reads and retained lifetimes. It checks modes,
repeat expansion, modifier release, mouse negotiation, UTF-16 order, native
Break identity, control counts and all 256 restored host keyboard bytes.
A complete failed collection is reviewed as failed, with its collection status
kept separate. Test-InputReviewer.ps1 proves that altered records and false
acceptance or capability claims are rejected.

The next decision follows the Windows 7 results: fix the measured stream
behavior, define the remaining control/profile contract, then qualify the
affected runtime and its H4 regressions before closing Step 1.2.
