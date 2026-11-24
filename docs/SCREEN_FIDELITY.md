# Full-screen restoration and resize

I use screen rc3 to verify Step 1.4 resize and restoration on Windows 7
SP1 x64.
The same fixture runs with the released 0.5.0 runtime, the accepted input
rc2 runtime and the fixed candidate. All sessions use flags 0.

## Run the tests

Extract the entire ZIP to a local folder such as `C:\VT7Screen`. Run the
launcher for the machine from an unlocked desktop. The tests send their own
input. The Legacy launcher uses the installed PowerShell 2.0 on LEOPARD.

| Machine | Launcher |
| --- | --- |
| NESSY | `RUN-SCREEN-NONESU.cmd` |
| TURTLE | `RUN-SCREEN-ESU.cmd` |
| LEOPARD | `RUN-SCREEN-LEGACY.cmd` |

Copy the complete new `results\<run-id>` folder to `K:\VT7_work\results`.
I use the snapshots, stream and receipts together. Each run attempts 24
cases across the three runtimes. I expect the two controls to reproduce the
hidden cursor difference. I check all eight cases on the fixed candidate.

## What I measure

- **F06 controls and restoration.** I keep the original switch test and
  exercise an alternate buffer with a hidden cursor. I check the cursor
  position while hidden and check that the cursor reappears when CMD
  restores the primary buffer. In separate CMD and native Windows PowerShell
  sessions, the app exits while the alternate buffer is active.
- **F07 viewport and fixed rows.** I capture the native viewport before
  repaint and compare the stable repaint using its measured origin. Any
  native rows outside the reader's comparison must be blank. The fixed-row
  fixture leaves the rightmost column unpainted and checks that its marker
  stays at the same buffer position through the resize.
- **F07 live resize.** I alternate 100×30 and 80×25 eight times while the
  app reads edited input and draws. I check its dimensions after each
  request and send a final 100×30 request next to app exit.
- **Shell return.** I reopen the active buffer in a new child, check the primary
  marker and compare the entire native viewport with the VT reconstruction.
  The resize cases check the original marker position or the measured
  full-width reflow position, including its color. I verify the exit status
  and submit an edited command through the restored prompt.

I use `CONOUT$` to check the active buffer. I record both the buffer size
and viewport because the primary buffer can have scrollback. The stable frames
use ASCII with fixed positions and colors.
[Console screen buffers](https://learn.microsoft.com/en-us/windows/console/console-screen-buffers) ·
[Window and screen buffer size](https://learn.microsoft.com/en-us/windows/console/window-and-screen-buffer-size)

## Build and review

From a clean, committed VT7Pty checkout with the two pinned runtime ZIPs,
I package with `Package-OutputFidelity.ps1 -Suite Screen -WithCandidate -Suffix rc3`.
I use `tools/output/Test-ScreenEvidence.ps1 -Directory <run-directory>
-PackageRoot <extracted-zip>` to check the returned receipts and file hashes.
The characterization records both matches and differences. Step 1.4 stays
open until I review the three Windows 7 runs and address required behavior.
