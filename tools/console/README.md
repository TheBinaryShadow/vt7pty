# Console mode tools

I use `tool-conin-mode` and `tool-conout-mode` to inspect or update the mode
of the current console input and output handle. These diagnostics came from
WinPTY's `misc` directory and help reproduce console-mode behavior.

Run either program without a mode argument to inspect the current flags. Pass
a hexadecimal mode value to update the matching console handle for that
process. These tools are built by `VT7Pty.sln`; routine verification validates
their binaries but does not change the invoking terminal's mode.

`IdentifyConsoleWindow.ps1` is a Windows PowerShell 5.1-compatible diagnostic
that distinguishes an attached console, a console without a visible window,
and a console window on another window station.
