# Step 1.1 Initial Development-Host Evidence

Date: 2026-10-12. Platform: Windows 10 development host, not Windows 7 acceptance.

The developmenthost directory preserves the complete results from the independently
extracted step-1.1-rc1 archive, including case JSON, controlled streams and fixture
snapshots. The report records 19 Pass / 4 Fail, all eight fault controls detected,
stable application hashes and successful native owned-process cleanup.

Verification records for the final clean harness source are stored as
debug-verification.json and release-verification.json. Their existing 29 local
entries pass; the 120-minute Windows 7 soak was not rerun for this local comparison.

These records support the [candidate review](../../2026-10-12-step-1.1-candidate.md).
They do not close Step 1.1. Named Windows 7 tier evidence remains pending.
