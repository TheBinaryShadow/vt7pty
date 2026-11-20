# PowerShell profile tests

I use this Step 1.2 package to check shell input with controlled configurations
and ordinary PowerShell profiles. The accepted input rc2 runtime stays unchanged:
0.5.1-dev, API 1.1, protocol 2, source 0ca6e3fa. Only the test harness is new.
Windows 7 qualification is pending until I review the returned runs. I retain
rc1's failed local qualification: its history command ran, but a result-file
reader blocked publication of the updated proof. Rc2 changes only the harness
reader's file sharing and adds a held-reader publication check.

## Run on all three machines

I extract the complete VT7Pty-profiles-0.5.1-dev-rc2.zip into a new local folder
on each machine. I keep Windows unlocked and use a normal, unelevated console.
I run the automated launcher first, then its matching manual launcher after
all five automated phases pass.

| Machine | Automated launcher | Ordinary-profile launcher |
| --- | --- | --- |
| NESSY, non-ESU | RUN-PROFILES-NONESU.cmd | RUN-PROFILES-MANUAL-NONESU.cmd |
| TURTLE, ESU | RUN-PROFILES-ESU.cmd | RUN-PROFILES-MANUAL-ESU.cmd |
| LEOPARD, legacy | RUN-PROFILES-LEGACY.cmd | RUN-PROFILES-MANUAL-LEGACY.cmd |

NESSY and TURTLE need PowerShell 7.2.24 x64 at
C:\Program Files\PowerShell\7\pwsh.exe. An alternate path can be supplied to
both launchers with -PowerShell7Path "full path to pwsh.exe". LEOPARD uses its
installed Windows PowerShell 2.0 and CLR 2.0. It needs neither KB3191566 nor
PowerShell 7.

I keep both complete result directories under the extracted package's results
folder and copy them back together. The manual report links to the matching
successful automated run on that same machine. I keep partial runs too.
Exit 0 means the collected phase passed, 1 means a failed case, and 2 means
collection or prerequisites are incomplete. No report accepts Step 1.2 automatically.

## Automated coverage

Each run keeps the frozen input rc2 guard and input executables. Its order is
H4 guards, profile fixtures, input controls, profile fixtures, profile fixtures.
The guards have 41 cases on NESSY/TURTLE and 33 on LEOPARD; the input controls
have all 43 accepted recipes. These controls are expected to pass.

Each profile trial also runs three deterministic file-publication checks,
including replacement while a reader remains open. The new controlled cases
run three times:

| Case | Editor | Configuration |
| --- | --- | --- |
| Windows PowerShell, clean | Native ConsoleHost | No personal profiles or PSReadLine |
| Windows PowerShell, layered | Native ConsoleHost | Four ordered inline fixtures and a multiline prompt |
| PowerShell 7, clean | Bundled PSReadLine | Windows bindings, predictions disabled |
| PowerShell 7, layered | Bundled PSReadLine | Emacs bindings, history predictions and a custom F6 binding |

LEOPARD runs the two Windows PowerShell cases. Each case checks exact Unicode
editing results, history recall, Tab completion, a multiline here-string,
cancellation of an unexecuted line, the next command, a native child interrupted
once by Ctrl+C, resize readback and natural exit 37. Layered fixtures also check
precedence; the PowerShell 7 fixture checks its custom binding.

These fixtures use -NoProfile and controlled inline script blocks. I do not
install them as global profile files or count them as ordinary-profile tests.
Controlled PSReadLine runs use a private history path with SaveNothing.
The fixture enables insert mode on its own console, matching the runtime's
input setup. It does not change the normal runtime or the frozen controls.

## Ordinary profiles and physical input

The separate manual launcher starts Windows PowerShell, followed by PowerShell
7 on NESSY/TURTLE. LEOPARD starts only Windows PowerShell. Actual shells start
with -NoLogo and their normal profiles, execution policy, prompt and editor
configuration. I allow their usual startup actions to finish before testing.
This phase deliberately executes those existing profile scripts. It never
creates or overwrites personal profiles or changes global policy or settings.
Test commands may enter the shell's normal history.

The live console displays the application and the current instruction. F2
inserts a test command without executing it; F8 verifies the checkpoint;
F12 stops the run. I press F8 only after the shell returns to a fresh prompt.
A failed checkpoint ends that run and preserves the evidence.

| Checkpoint | What I do |
| --- | --- |
| Startup | Finish normal startup prompts, then F8. The runner defines its temporary, nonce-named helpers and records metadata. |
| Editing | F2 inserts EDXT with Croatian text. I use Left, Backspace and I to change it to EDIT, then End, Enter and F8. |
| History | F2, Enter, then Up and Enter to recall the same command, followed by F8. |
| Completion | F2, Tab, Enter, then F8. The helper prefix is unique in this session. |
| Multiline | F2, Enter, then firstč, Enter, secondž, Enter, '@, Enter and a blank Enter, then F8. |
| Cancellation | F2 inserts a line that must never run. I press Ctrl+C, then F8 at the fresh prompt. |
| Native child | F2, Enter, wait for PROFILE_CHILD_READY, then Ctrl+C. I press F8 at the parent prompt. |
| Resize | I resize the console window, then F2, Enter and F8. |

After all eight checkpoints, I type exit 37 and press Enter. I repeat the
workflow in the next engine when the launcher starts it. I use a Croatian
layout for the two short Croatian strings; this is profile/editor coverage,
not a repeat of the accepted full US/Croatian keyboard suite.

If normal policy blocks a profile, startup reports errors, a profile asks for
credentials, or a custom editor prevents a checkpoint, I keep the incomplete
result and review that configuration before changing anything. I do not loosen
policy or reset the profile just to obtain a pass.

## What the result contains

I record engine and editor versions, editor module hashes, the five execution
policy scopes, language mode when exposed, window dimensions and the presence,
size and SHA-256 of each of the four profile scopes. The shell reports boolean
matches for the fixed test strings. Native identities and retained process
handles establish console association, event counts, natural exit and cleanup.

For ordinary shells, output stays in the live display. I do not save shell
output, screenshots, prompts, profile contents, history contents or typed text.
Only nonprinting key counts needed for the checks and resize dimensions are
recorded. Existing profile file hashes, policy and editor settings must remain
stable across the run. An absent profile is recorded as absent.

The CMD launcher remembers the original process execution-policy variable
before starting the collector. Test shells receive that original value, or no
process override when it was absent. The collector's own temporary Bypass
setting must not leak into ordinary shell startup.

## Build and review

Package-Profiles.ps1 requires clean committed source and the exact accepted
input rc2 ZIP. It verifies every frozen file, builds the new harness, compares
its compiled recipes with tools/profiles/cases.json and runs its logic checks.
The package includes symbols, provenance and Windows PowerShell 2.0-compatible
collectors. Test-ProfileEvidence.ps1 independently checks the returned inventory,
source identities, profile metadata, command proofs, host traces and cleanup.

Local synthetic manual smoke tests use -NoProfile and are explicitly marked.
They exercise the frontend without running my personal profiles and cannot
qualify ordinary-profile coverage. A local pass cannot qualify Windows 7.
No repeat of the accepted H4 lock/unlock suite or two-hour soaks is required
for this harness-only package.

The public Ctrl+Break transport gap, Escape latency and cursor-report/F3
ambiguity remain unchanged in the [input contract](INPUT_FIDELITY.md#input-and-reply-contract).
Output reconstruction and wider resize/lifecycle coverage remain on the
[roadmap](../ROADMAP.md).

Microsoft documents the [four profile scopes and their load order](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_profiles?view=powershell-5.1)
and [PSReadLine configuration](https://learn.microsoft.com/en-us/powershell/module/psreadline/about/about_psreadline?view=powershell-7.2).
