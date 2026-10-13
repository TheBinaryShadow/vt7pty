# Native console fixtures

I use these small executables as controlled child processes for argument,
input, output, Unicode and console API tests. They came from WinPTY's
`misc` directory. Moving them here did not make their interactive tests
suitable for unattended execution.

| Fixture | Purpose |
| --- | --- |
| `fixture-console-color-grid` | Writes every legacy foreground/background attribute combination. |
| `fixture-output-lines` | Produces bounded, deterministic numbered output for volume and drain tests. |
| `fixture-show-argv` | Prints the raw command line and parsed arguments. |
| `fixture-show-console-input` | Reports input records read from the console. |
| `fixture-utf16-echo` | Writes specified UTF-16 code units through the console API. |
| `fixture-win32-echo1` | Echoes console input with `ReadConsole` and `WriteConsole`. |
| `fixture-win32-echo2` | Echoes key input obtained through `_getch`. |
| `fixture-win32-write1` | Exercises console geometry and character output. |
| `fixture-write-console` | Decodes escapes and writes through `WriteConsole`. |

The solution builds every fixture with Debug and Release symbols. The main
verifier checks their architecture, Windows 7 subsystem floor, imports,
version identity, and presence. It runs `fixture-show-argv` as a deterministic
quoting check and `fixture-output-lines` as a bounded output check. The other
fixtures require a real console or specific input and will be driven by focused
test cases in later roadmap steps.
