# H4 manual checks

I use these checks after the automated H4 collection has finished. I need both
result directories from each machine: the normal manual run and the separate
elevated-caller run. H4 stays open until I review the underlying evidence.

## Known rc4 display problem

The [Windows 7 review](validation/2026-11-18-step-1.2-h4-runtime-rc4-windows7.md)
confirms that RUN-H4-MANUAL.cmd hides typed input even when the backend receives
it. I have not qualified the US/Croatian or lock/unlock workflows. I need to repair
the display before requesting another manual run; do not repeat blind typing.
The automated runs and separate elevated-caller checks passed on all three machines.
The rc4 instructions below are retained for reference.

## Before starting

Extract the complete rc4 ZIP into a fresh local folder. Run the automated
launcher for NESSY, TURTLE or LEOPARD first. Close other VT7Pty debug collectors.
Use the ordinary Windows desktop and a classic console, with US and Croatian
keyboard layouts available. Use the physical keyboard, without paste, macros,
remote input or accessibility key substitution.

RUN-H4-MANUAL.cmd opens the installed x64 Windows PowerShell, including 2.0 on
LEOPARD. It does not require PowerShell 7 for these checks. It asks for three
checks in order: US, Croatian, then lock/unlock. Allow about 15 minutes.

## Physical input

Double-click RUN-H4-MANUAL.cmd as the ordinary user. Follow the displayed text
exactly. Change the layout on that console when asked. I record the console's
actual layout identifier and physical input records, including modifiers.

For US, type `AZaz09[]{}\|` and press Enter. Use Shift for uppercase letters.
For Croatian, type the displayed Croatian letters and punctuation using Shift
and AltGr. The next prompt asks for dead-key composition of á, followed by Enter.
On the standard Croatian layout, use the acute-accent dead key, then a.

Each layout then asks for OLD followed by Ctrl+C, NEXT followed by Enter, and
raw Ctrl+C followed by lowercase z without Enter. Release Ctrl before typing z.
Do not paste the strings. Each prompt has a three-minute deadline. The current
step fails visibly if the input differs; keep its complete partial evidence.

This small test bridge forwards bytes from actual console key records into the
VT7Pty input pipe. It checks the application-facing backend with real keyboard
input. It does not qualify VT7's keyboard UI or replace later VT7 integration
tests.

## Lock and unlock

The final normal-user check opens a pending line and asks me to press Win+L,
then sign back in within five minutes. Leave it locked for several seconds.
Do this only at that prompt, after the automated collection has finished.

The test observes the desktop change, checks that a new opt-in session is
refused while inactive, and checks the existing line, cancellation, next read
and a newly opened session after unlock. It never locks Windows, switches
desktops, signs out or changes screensaver settings itself.

## Elevated caller

After the normal manual run, right-click RUN-H4-ACCESS.cmd and choose
**Run as administrator**. I expect opt-in startup to refuse this high-integrity
caller with the specific medium-integrity message. Flagless startup and normal
host cleanup must still work. The launcher never elevates itself.

This qualifies the actual elevated-caller boundary. It does not establish
support for arbitrary restricted tokens, service sessions or other users.

## Return the evidence

Keep every directory under results, including failed or partial attempts.
The manual launcher creates a fresh machine-named directory each time, with
report.json and a SHA-256 file inventory. Send the complete directories with the
automated collection from all three machines.

For a focused repeat, the PowerShell runner accepts -Check US, HR, Session or
Elevated. I still need the entire requested manual matrix before accepting H4.
