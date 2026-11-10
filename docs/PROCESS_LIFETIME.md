# Process lifetime and cleanup

Status: Planned under [Step 1.5](../ROADMAP.md#step-15-validate-lifecycle-and-resource-behavior-under-real-workloads);
required before 1.0.
Recorded: 2026-11-10.

I need cleanup that works under Windows 7's job-object rules and preserves
the lifetime contract I expose to VT7. This plan covers the research harness
and future runtime integration. It adds required coverage; it records no new
implementation or passing results.

## Why this needs explicit coverage

[VSoft.Process PR #18](https://github.com/VSoftTechnologies/VSoft.Process/pull/18),
merged on November 9, addresses missing cleanup when a child cannot be assigned
to a job. It adds conditional breakaway and a process-tree fallback using a
retained root handle. The [review discussion](https://github.com/VSoftTechnologies/VSoft.Process/pull/18#issuecomment-6674863063)
also identifies the fallback's limits. The author
[had not tested the Windows 7 breakaway path](https://github.com/VSoftTechnologies/VSoft.Process/pull/18#issuecomment-6675721611).

On Windows 7, a process can belong to only one job. Children normally inherit
their parent's job, so assigning them to a second job fails. Nested jobs
arrived in Windows 8. Creating a new console or private desktop does not
remove job membership. Microsoft's
[job-object guide](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
and [assignment reference](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject)
define these constraints.

## Current H3 boundary

The H3 fixture creates each controlled shell suspended, assigns it to a new
job with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, then resumes it. Only that shell
is explicitly assigned; its descendants normally inherit the job. The host,
agent and fixture are not assigned to this cleanup job.

If the shell inherits an enclosing job, assignment can fail on Windows 7.
The fixture treats that as a failure and attempts to terminate the suspended
shell through its retained handle. It does not continue without containment.
The current candidate neither requests explicit breakaway nor records a
dedicated job-context report. Its fixture-death control checks one owned
shell, not a complete descendant-cleanup matrix.

I'll continue the current [rc1 procedure](HOST_CANCELLATION_PROBE.md) from the
normal desktop launchers and retain all failures. An assignment failure needs
separate diagnosis from the cancellation result. The published 0.5.0 runtime
does not itself create cleanup jobs; this finding does not demonstrate a
production runtime defect.

## Required cases before 1.0

I'll build controlled fixtures for these cases and record exact expectations
before running them on the development host and all three Windows 7 tiers.

| Scenario | Required check |
| --- | --- |
| No enclosing job | Normal launch, final output, exit status and close meet the lifetime contract; controlled clients leave no unexplained survivors |
| Explicit breakaway allowed | Query and record the enclosing job's limits; verify the chosen policy, new child membership and cleanup without changing the enclosing job |
| Silent breakaway allowed | Verify actual child membership and cleanup without assuming explicit breakaway was needed |
| Breakaway denied | Preserve the restriction; fail or use a separately qualified supported path, with bounded cleanup and no false success |
| Application creates its own job | Exercise that workflow on Windows 7; any containment policy must account for its effect on application-owned jobs |
| Owner exits abruptly | Cover controller, agent and fixture death, including a path where destructors cannot run; check owned children, handles and observed host exit |
| Intermediate parent exits | Keep a controlled grandchild alive after its parent exits; verify cleanup against retained identities rather than assuming a later tree snapshot can find it |
| Access or assignment fails | Capture the original error before cleanup; exercise real denial as well as injected failure and verify the suspended child is handled |
| Concurrent sessions and shutdown | Close or fail one session while another remains live; verify the survivor, output drain, exit status, bounded shutdown and recreation |

## Ownership and acceptance

I'll specify which processes VT7Pty owns, which must end on session close and
which may outlive it before freezing the integration API. That includes whether
breakaway is requested and how failure is reported. Blanket breakaway can
change a caller's lifetime guarantees; a restrictive job can also prevent
applications from using their own jobs on Windows 7.

Retained handles and creation identities remain the basis for cleanup.
A process-tree walk is not equivalent to kill-on-job-close: it can miss
descendants after an intermediate parent exits and depends on cleanup code
actually running. Any fallback needs its own ownership, race and abrupt-exit
tests. Console hosts must never be terminated as a cleanup shortcut.

I'll record job membership, relevant limits, creation/assignment flags, original
errors, retained process identities and observed exits with the exact candidate
identity. A passing run must prove the selected cleanup contract and preserve
neighboring sessions. Missing observations, unexpected survivors or emergency
cleanup remain failures.

These cases are tracked in Step 1.5 and remain a pre-1.0 requirement regardless
of the H3 rc1 result. Decisions about loading, isolation and lifetime also feed
[H4](HOST_CANCELLATION_DESIGN.md) and the Milestone 2 API contract.
