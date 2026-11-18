# H4 manual checks

I use the manual-only follow-up to finish US/Croatian keyboard and lock/unlock
qualification on NESSY, TURTLE and LEOPARD. The product binaries are unchanged
from the qualified rc4 archive. I do not need another automated collection,
four-hour soak pair or elevated-caller run. The
[rc4 review](validation/2026-11-18-step-1.2-h4-runtime-rc4-windows7.md)
retains those passes and the original failed keyboard attempts.

## Before starting

Extract VT7Pty-h4-manual-0.5.1-dev-rc1.zip into a fresh local folder on each
machine. Close other VT7Pty debug collectors. Use the ordinary Windows desktop
and a classic console, with US and Croatian keyboard layouts available.
Use the physical keyboard, without paste, macros or remote input.

Double-click RUN-H4-MANUAL.cmd as the ordinary user. It uses the installed x64
Windows PowerShell, including 2.0 on LEOPARD. PowerShell 7 and KB3191566 are not
required. The checks run in order: US, Croatian, then lock/unlock. Allow about
15 minutes. I keep a new machine-named result directory for every attempt.

## Physical input

I show the required and current console layouts before starting. Select the
requested layout on that console and press Enter. An incorrect layout keeps
the check at this prompt. F12 stops the current keyboard check.

The **local preview** shows typing immediately, including Backspace corrections.
The separate **Backend returned** line shows what the application actually read;
<CR><LF> makes the line ending visible. I check the backend's exact result,
independently of the local preview.

For US, type `AZaz09[]{}\|` and press Enter. Use Shift for uppercase letters.
For Croatian, type the displayed Croatian letters and punctuation using Shift
and AltGr. The next prompt asks for dead-key composition of á, then Enter.
On the standard Croatian layout, use the acute-accent dead key, then a.
Keep the requested layout selected throughout that check.

Each layout then asks for OLD followed by Ctrl+C without Enter, NEXT followed
by Enter, and raw Ctrl+C followed by lowercase z without Enter. Release Ctrl
before typing z. I display the cancellation and readback results at each stage.
Each input prompt has a three-minute deadline. Keep all partial files if a
step stops or fails. The launcher stops there so its failure stays visible.

This bridge forwards bytes from console key records into the VT7Pty input pipe.
It qualifies the backend with physical keyboard input. VT7's own keyboard UI
will need its separate integration tests.

## Lock and unlock

The final check opens a pending line and asks me to press Win+L, then sign back
in within five minutes. Leave it locked for several seconds. Only lock Windows
when this prompt appears, after both keyboard checks have passed.

I observe the desktop change and refusal of new opt-in sessions while inactive.
After unlock, I check the pending line, cancellation, the exact next read,
a newly opened session and cleanup. The runner does not lock Windows, switch
the input desktop, sign out or change screensaver settings itself.

## Return the evidence

Keep every complete directory under results, including failed or partial
attempts. Each contains report.json, key records, actual readbacks, UI text,
runtime trace, retained session identities and a SHA-256 inventory. Copy the
new directories from all three machines to K:\VT7_work\results for review.
H4 stays open until I review all required physical results.

For a focused repeat from this extracted folder, I can use:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\research\Invoke-H4Manual.ps1 -Check US

The other focused choices are HR and Session. I preserve earlier attempts.
The separate RUN-H4-ACCESS.cmd in the original rc4 package remains available
for elevated-caller checks; its passes on all three machines still stand.

## Runner verification

I verify the corrected display in an owned hidden console before issuing the
package. These checks inject synthetic key records and explicitly identify
themselves as synthetic; they cannot count as physical acceptance. They cover
visible typing before Enter, corrections, wrong-layout refusal, Unicode,
cooked and raw Ctrl+C, and caller-desktop refusal/recovery without locking
Windows. The physical runner always queries the actual console layout.

The manifest records the new harness source separately from rc4 runtime source
54b0d23146dfde7528c9e4db1aa92d12e662f883. I copy the DLL, agent, host module and
debug collector directly from that exact rc4 ZIP and verify their original
hashes. No product code, API or protocol changes are part of this follow-up.
