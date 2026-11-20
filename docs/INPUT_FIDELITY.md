# Step 1.2 input tests

I qualified the UTF-8 decoder and cursor-reply framing fixes in input rc2 on
all three Windows 7 tiers. The [Windows 7 review](validation/2026-11-19-step-1.2-input-rc2-windows7.md)
verifies 387/387 input cases, 345/345 H4 guards, 21 lifetime/fault guards and
42 regressions. All three collections are complete; no rerun is needed.
The frozen rc1 reference retains its original three failures on each machine.

API 1.1, protocol 2 and the H4 cancellation contract stay the same. The
[issuance review](validation/2026-11-19-step-1.2-input-rc2-candidate.md) records
local qualification and the exact archive. Step 1.2 remains open for its
remaining coverage and recorded limitations. The commands below reproduce
this accepted decoder matrix; they are not a request for another run.

## Run on the Windows 7 machines

Extract **VT7Pty-input-0.5.1-dev-rc2.zip** into a fresh local folder. Run as the
ordinary user, with Windows unlocked and other VT7Pty debug collectors closed.

| Machine | Launcher |
| --- | --- |
| NESSY, non-ESU | RUN-INPUT-NONESU.cmd |
| TURTLE, ESU | RUN-INPUT-ESU.cmd |
| LEOPARD, legacy PowerShell 2.0 | RUN-INPUT-LEGACY.cmd |

I collect one complete input run from the unchanged rc1 reference, followed
by three pairs of candidate H4 guards and candidate input tests. The reference
keeps its original failures. Candidate failures stay failures, and collection
continues so I can review every case.

The launcher then runs seven focused H4 lifetime/fault guards and 14 unit,
protocol, backend and flagless session regressions. Keep every file under
results and return the complete machine directories to K:\VT7_work\results.
No physical typing, lock/unlock, elevation or two-hour soak repeat is requested.
Keep the machines unlocked until the launcher finishes.

NESSY and TURTLE use PowerShell 7.2.24 x64 at
C:\Program Files\PowerShell\7\pwsh.exe for their H4 shell guards. A different
path can be supplied with -PowerShell7Path. LEOPARD uses installed Windows
PowerShell 2.0 and CLR 2.0, without KB3191566 or PowerShell 7. The native
runtime does not depend on PowerShell 7.

Exit code 0 means the candidate phases pass and the reference retains its
three known failures. Code 1 means a candidate failure or changed reference
outcome needs review. Code 2 means collection or prerequisites are incomplete.
No report accepts Step 1.2 automatically.

## What I measure

Each candidate input run has 43 cases. The original 33 recipes remain intact;
10 additions cover delayed three/four-byte characters, malformed UTF-8
recovery in combined and bytewise writes, standalone Escape with/without a
reply, bytewise replies inside fragmented keys, and fragmented Alt+Unicode.
I require an actual reply in each live-reply case and retain its write chunks.

The suite also covers cooked editing, queued lines, Unicode cancellation,
key-down/up order, modifiers, repeat counts, supplementary UTF-16 pairs,
committed text, negotiated SGR/X10 mouse records and disabled-mode gating.
Every passing case requires an exact ordinary read afterward. I retain modes,
raw records, output, process identities, desktop placement and cleanup.

Each candidate H4 run has 41 cases on NESSY/TURTLE or 33 on LEOPARD. Its
existing behavioral checks remain in place. The separate frozen rc1 harness
and product binaries retain their exact hashes and source identities.
The [H4 acceptance](validation/2026-11-19-step-1.2-h4-manual-rc1-windows7.md)
continues to describe that earlier runtime, not automatic acceptance of rc2.

## Input and reply contract

I keep the UTF-8 key stream separate from solicited cursor-position replies
before converting input to Windows records:

- A valid unfinished UTF-8 scalar, including Alt+Unicode, survives idle periods.
  The escape timer does not clear its bytes.
- Invalid lead, continuation, overlong, surrogate and out-of-range encodings
  are discarded without consuming the next valid character or control byte.
- One internal cursor request can be outstanding. Its ESC[row;columnR reply
  is consumed as a frame, even between bytes of an unfinished key token.
  Receiving a reply does not finish that token or refresh its input timer.
- A terminal host must serialize each complete reply relative to other input.
  Its bytes may span pipe writes, with less than one second between pieces.
  Interleaving a key inside a reply itself is ambiguous and unsupported.
- I retain the one-second escape ambiguity fallback. A standalone Escape now
  waits for that fallback rather than being flushed by a reply. Polling adds
  scheduling delay; this is a correctness bound, not a latency claim.
- A malformed or expired reply prefix is replayed through normal key decoding.
  Prefix storage is bounded. Unsolicited or duplicate reports also follow
  normal key decoding. While a request is outstanding, the standard reply
  syntax is reserved; identical modified-F3 encodings remain ambiguous on this
  byte-only interface.

The UTF-8 validation follows [RFC 3629](https://www.rfc-editor.org/info/rfc3629/).
Microsoft documents the [cursor report syntax](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#query-state).
The framing and timeout rules above are VT7Pty's current contract. Escape
latency and the reply/key ambiguity remain considerations for the later VT7 API.

## Remaining limits

I preserve the approved Ctrl+Break decision. API 1.1 has no distinct Break
operation. Byte 0x1C remains a text/control-byte test. Two native Windows
references target retained child process groups and require exactly one Break,
zero Ctrl+C, no parent notification and natural child exit. They establish
Windows behavior, not a VT7Pty transport.

Ordinary-user-profile coverage remains NotRun in these input packages. The
[profile rc2 package](PROFILE_TESTS.md) keeps the accepted input rc2 runtime unchanged
and is [qualified locally](validation/2026-11-20-step-1.2-profiles-rc2-candidate.md).
Its Windows 7 controlled and ordinary-profile phases are pending.
Clipboard, held physical keys and IME UI remain outside committed-text tests.

## Build and independent review

Package-InputFidelity.ps1 requires clean committed source, matching Release
verification and the exact rc1 ZIP. It checks every frozen artifact and compares
the rebuilt harness recipes against tools/input/decoder-cases.json. Symbols,
credits, provenance and PowerShell 2.0-compatible collection helpers are included.

Test-InputEvidence.ps1 recomputes each native input outcome from the golden
recipes, records, readbacks and lifetimes. Test-InputCandidateEvidence.ps1 checks
the complete rc2 collection, reference failures, H4 regressions, prerequisites
and inventories. Test-InputReviewer.ps1 rejects altered evidence and false
capability or acceptance claims. Local qualification, the exact archive and the completed Windows 7 review
are recorded in the validation index.
