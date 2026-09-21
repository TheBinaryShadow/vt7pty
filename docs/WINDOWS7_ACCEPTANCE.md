# Windows 7 Platform Acceptance

This procedure validates a specific VT7Pty candidate on the two physical
Windows 7 SP1 x64 tiers defined in [COMPATIBILITY.md](COMPATIBILITY.md). It is
the target-side gate for Roadmap Step 0.4 and remains separate from
development-host verification.

## Candidate preparation

Produce one clean-tree Release candidate and its checksum:

```powershell
.\Package-VT7Pty.ps1 -Configuration Release -PackageSuffix step-0.4-candidate
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

5. Preserve the complete `results` directory from each machine and return
   both the `.json` and `.txt` files for repository validation evidence.

The runner requires 64-bit Windows PowerShell on Windows 7 SP1. It verifies
the package manifest and every packaged file before running the native cases.
It records the declared tier, hardware, OS and service-pack identity,
PowerShell version, installed hotfixes, package identity, duration, output,
and result of each case.

## Step 0.4 cases

The candidate performs these bounded checks:

- inherited string-builder unit coverage;
- client, agent, pipe, child-process, output, exit-status, and teardown flow;
- resize followed by real Command Prompt and Windows PowerShell sessions;
- embedded version and source-commit identity;
- argument quoting; and
- deterministic bounded output.

Any failed preflight, timeout, nonzero exit, missing expected output, missing
file, or hash mismatch makes the run fail. The two target records must both
pass before Step 0.4 can claim Windows 7 SP1 x64 as its demonstrated minimum.

## Result handling

Acceptance records copied into `docs/validation` must identify the candidate
ZIP and SHA-256, keep the target tiers distinct, and report failures or
deviations without converting them into passes. Machine names can be redacted
for publication as long as the hardware and OS/update evidence remains
unambiguous.
