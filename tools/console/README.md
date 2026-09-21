# Console mode tools

`tool-conin-mode` and `tool-conout-mode` inspect or update the mode of the
current console input and output handle. They were inherited from WinPTY's
`misc` directory and are maintained diagnostics for reproducing console-mode
behavior.

Run either program without a mode argument to inspect the current flags. Pass
a hexadecimal mode value to update the matching console handle for that
process. These tools are built by `VT7Pty.sln`; routine verification validates
their binaries but does not change the invoking terminal's mode.
