# SSH and Full-Screen Baseline Dependency

Status: unresolved dependency for Milestone 1. Updated: 2026-09-24.

Step 0.8 defines the remote application baseline instead of claiming an
unmeasured result. The current VT7Pty test package verifies local Command
Prompt, Windows PowerShell, console input and output, Unicode, resize, and
lifecycle behavior. It does not include a Windows 7-qualified console SSH
client or a controlled remote endpoint. VT7's direct SSH transport is a
different path and does not exercise VT7Pty's application-facing backend.

Before SSH, Vim, htop, or another full-screen remote application becomes a
Milestone 1 regression gate, fix and record all of the following:

- an exact Windows 7-compatible console SSH client binary, version, license,
  and SHA-256 hash that launches inside VT7Pty;
- a controlled remote host and OS, server version, shell, authentication method,
  locale, `TERM`, and application versions;
- credential-safe setup, host-key verification, and a disposable test account;
- terminal geometry, input sequence, observable screen-state expectations,
  resize sequence, cancellation, exit, and cleanup criteria; and
- matching non-ESU, ESU, and legacy PowerShell 2.0 Windows 7 result records for
  the same candidate.

Until these dependencies are fixed, the remote baseline is **NotRun**, not a
backend pass or failure. Local full-screen fixture behavior can be added to
the maintained package without implying SSH fidelity.
