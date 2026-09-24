# Windows 7 Platform Acceptance

This procedure validates a specific VT7Pty candidate on the two physical
Windows 7 SP1 x64 tiers defined in [COMPATIBILITY.md](COMPATIBILITY.md). It is
the target-side gate for Milestone 0 candidates and remains separate from
development-host verification.

## Candidate preparation

Produce one clean-tree Release candidate and its checksum:

```powershell
.\Package-VT7Pty.ps1 -Configuration Release -PackageSuffix step-0.8-candidate
```

Use the same ZIP on both machines. Copy the adjacent `.sha256` file with it
and verify the ZIP before extraction. The package manifest records the source
commit and hashes every file used by the acceptance runner.

## Target procedure

1. Copy the ZIP to a local NTFS volume and extract it to a new directory.
2. Open an ordinary Command Prompt in the extracted directory. Administrator
   rights are not required.
3. On the fully updated non-ESU machine, run:

   ```bat
   RUN-WINDOWS7-ACCEPTANCE.cmd NonESU
   ```

4. On the ESU-updated machine, run:

   ```bat
   RUN-WINDOWS7-ACCEPTANCE.cmd ESU
   ```

5. Allow the 120-minute idle/active soak to finish on each machine. A fast
   diagnostic run can use `-SoakMinutes 0` when invoking the PowerShell script
   directly, but it records `NotRun` and does not qualify Step 0.8.
6. Preserve and return the complete `results` directory from each machine.
   It includes the `.json` and `.txt` result pair plus any step-specific
   diagnostic logs and bundles.

The runner requires 64-bit Windows PowerShell on Windows 7 SP1. It verifies
the package manifest and every packaged file before running the native cases.
It records the declared tier, hardware, OS and service-pack identity,
PowerShell version, installed hotfixes, package identity, duration, output,
and result of each case.

## Maintained cases

The candidate performs these bounded checks:

- modern C++ formatting, checked-narrowing, protocol, and buffer unit coverage;
- client, agent, pipe, child-process, output, exit-status, and teardown flow;
- resize followed by real Command Prompt and Windows PowerShell sessions;
- embedded version and source-commit identity;
- debug-pipe ACL validation and diagnostic build identity;
- bounded structured diagnostic transport and target-side bundle creation;
- argument quoting; and
- deterministic bounded output;
- input and Unicode output, 100 resizes under load, 500 repeated sessions,
  20 live shutdowns, failed child creation, four concurrent sessions, and
  agent/child exit and handle-growth checks;
- deliberate faults in output, status, order, truncation, timeout, and handle
  state that the runner must detect; and
- a 120-minute idle/active soak with bounded per-cycle cleanup.

Any failed preflight, timeout, nonzero exit, missing expected output, missing
file, malformed diagnostic record, bundle mismatch, or hash mismatch makes the
run fail. The two target records must both pass before the corresponding
roadmap step can use them as physical Windows 7 evidence.

The target JSON and text records include per-case status and duration, package
and OS/update identity, and any dump or WER files found in the results
directory. Preserve generated diagnostic logs and bundles alongside the result
pair. The remote SSH/full-screen application baseline remains [an explicit
dependency](SSH_BASELINE.md) for Milestone 1.

## Result handling

Acceptance records copied into `docs/validation` must identify the candidate
ZIP and SHA-256, keep the target tiers distinct, and report failures or
deviations without converting them into passes. Machine names can be redacted
for publication as long as the hardware and OS/update evidence remains
unambiguous.
