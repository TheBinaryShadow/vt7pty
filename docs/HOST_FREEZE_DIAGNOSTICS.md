# Scraper and cancellation experiment

I'm testing whether the agent's selection-based screen-scraping freeze can
consume a cancellation key, and whether its later Escape changes a pending
read. The [reassessment](validation/2026-11-13-step-1.2-cancellation-reassessment.md)
defines the questions. This package collects evidence; H3 remains open.

The [Windows 7 review](validation/2026-11-13-step-1.2-host-freeze-diagnostics-rc1-windows7.md)
finds passing quiet/coordinated phases in all 54 attempts and failed delivery in
all 27 selected challenges. It supports a coordinated research prototype. Other
case failures remain, and the long-lived neighbors' recorders need periodic
draining to prevent the observed loss of their early trace history.

## Run on Windows 7

Extract **VT7Pty-host-freeze-diagnostics-0.5.0-rc1.zip** into a fresh local folder,
preferably a short path such as C:\VT7Pty-tests\freeze-rc1. Keep the directory
tree together and run as the ordinary test user from the normal desktop.

| Machine | Launcher |
| --- | --- |
| NESSY | RUN-HOST-FREEZE-DIAGNOSTICS-NONESU.cmd |
| TURTLE | RUN-HOST-FREEZE-DIAGNOSTICS-ESU.cmd |
| LEOPARD | RUN-HOST-FREEZE-DIAGNOSTICS-LEGACY.cmd |

Installed x64 Windows PowerShell is sufficient, including PowerShell 2.0 on
LEOPARD without KB3191566. PowerShell 7 is unnecessary for this experiment.
Allow several minutes and keep every result, including failures. Run once on
each machine; do not repeat failed trials just to obtain green results.

The runner executes three groups, each containing:

1. The unchanged original H3 rc1 control, with 12 cases.
2. The unchanged first diagnostic trace reference, with 13 cases.
3. The new freeze experiment, with 22 cases and rotated condition order.

That gives **nine trials and 141 case records per machine**. Limits are
210 seconds for a control, 300 for a trace reference and 480 for an experiment.
The result directory records its machine, tier, package and both old/new sources.

## Reading the result

An outcome of Fail can be useful diagnostic evidence. In particular, deliberately
selected cases retain a cancellation failure when the key produces no event.
The same-fixture bracket also retains that failed middle operation even when
both positive controls succeed. Those observations are never relabeled Pass.

CollectionStatus describes whether the native aggregate records were collected.
It is separate from each original Pass/Fail outcome and from the independent
review of trace coverage and experimental state. The runner returns exit code
1 if any trial fails or collection is incomplete. Keep the entire results folder.

## Builds kept together

- control contains all 18 original H3 rc1 files without rebuilding them.
- reference contains all 35 files from the first issued diagnostic ZIP unchanged.
- experiment contains the new probe, hook and a research agent, alongside the
  exact accepted 0.5.0 runtime DLL.

The research agent has its own original filename, file description, hash and
clean source identity. Its packaged filename is VT7Pty-Agent.exe because that
is how the existing runtime DLL locates its colocated agent. Keep each bin
directory intact. The original control/reference agents remain unchanged.

The outer manifest verifies every component. The runner also checks the pinned
original control and reference manifests independently. The new research
protocol is version 3; the separate agent gate protocol is version 1. The public
runtime API/protocol and release version remain unchanged.

## Controlled conditions

The new build retains the 13 existing traced cases and adds completed-read and
pending-read contrasts:

| Condition | Work during the single key operation |
| --- | --- |
| quiet | Agent acknowledges an unfrozen interval and excludes scrape/resize freezes |
| selected | Agent deliberately establishes selection, retaining its cached frozen state until release |
| coordinated | A held unfrozen interval while a fixture writes output; a resize is queued until release |
| delay-control | The same declared observation delay and queued resize, while periodic scraping remains active |

Each condition uses the same declared 50 ms pre-send observation delay. Actual
timestamps are retained; these intervals are not claimed to be identical. A
second completed-read case brackets a selected challenge with positive delivery
controls in the same fixture. Condition order rotates across the three groups.

The agent grants a bounded 20-second lease only between complete agent operations.
The controller obtains it before entering the host callback. Both explicit
release and expiry resume work; expiry and lost peers are recorded. Resize
requests made during the experimental lease are recorded as queued, then
applied on release. The test observes the applied dimensions and restores 80x25.

Cancellation remains exactly one synchronous WM_KEYDOWN C with temporary Ctrl
state and full restoration. There is no retry or appended control event.
Two-second original deadline observations remain failures even if a later
five-second observation sees delivery. Recovery Enter only permits cleanup.

## Measurements and limits

The host recorder keeps entry/return for SelectAll, Mark, C and WM_CHAR Escape.
The agent records scrape/resize and freeze transitions, generations, cached
state, grants, release and expiry. Each recorder uses a fixed 4,096-event ring,
paged in chunks of 32. Any overwritten records are counted explicitly.
Host-message recording performs no file I/O, blocking IPC or console queries.

Fixture read/handler timelines, selection/input-mode samples and keyboard
snapshots retain their earlier limits. A public selection sample is not atomic
with host key entry. Character zero in the raw host-key probe is preserved;
that probe does not establish equivalence to the existing unprocessed transport.
The receipt-only control still intentionally lacks a final acknowledgment.

The new build strengthens shell measurements consistently: complete marker
content, command-specific CMD continuation prompts, prepared PowerShell prompt
generations and separate file-existence/content timestamps. A fresh prompt is
still not proof that a shell is blocked in ReadConsole. Changes to the shell
oracle do not prove a cancellation fix.

The [development reviewer](../tools/research/Test-HostFreezeEvidence.ps1) requires
PowerShell 5.1 or later and writes its review outside the raw result directory.
The portable runner supports PowerShell 2.0. All trials retain private desktops,
owned-process cleanup and neighboring-session checks. Ordinary-desktop production
use, the wider active-child/PowerShell 7 matrix and H4 remain separate work.
