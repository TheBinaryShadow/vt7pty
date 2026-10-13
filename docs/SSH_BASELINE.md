# Optional Console-SSH Baseline

Status: deferred optional scenario; not a local Milestone 1 acceptance dependency.
Updated: 2026-10-12.

I've deferred console-SSH and remote full-screen testing while I focus on
local shells, native children and controlled fixtures in the
[Milestone 1 fidelity plan](FIDELITY.md). A remote endpoint and console SSH
client are not required for that milestone.

This scenario would launch a Windows console SSH client inside VT7Pty and
exercise its input/output path against remote applications such as Vim or
htop. VT7's built-in direct SSH transport bypasses VT7Pty and is a separate
consumer path.

## Prerequisites if activated later

Before this scenario becomes a gate, record:

- an exact Windows 7-compatible console SSH client binary, version, license
  and SHA-256 that launches inside VT7Pty;
- a controlled remote host/OS, server version, shell, authentication method,
  locale, TERM and application versions;
- host-key verification, credential-safe setup and a disposable test account;
- geometry, input recipe, expected screen state, resize, cancellation, exit
  and cleanup criteria; and
- designated Windows 7 acceptance tiers and matching exact-candidate evidence.

Until activated and executed, the scenario is deferred and NotRun. It is not
a backend pass or failure, and does not block local fidelity acceptance.
Local full-screen results do not imply remote SSH fidelity.
