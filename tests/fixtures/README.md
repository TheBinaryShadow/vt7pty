# Native console fixtures

These small executables give the maintained test suite controlled child
processes for argument, input, output, Unicode, and console API behavior. They
were inherited from WinPTY's `misc` directory and moved here without turning
interactive behavior into unattended tests.

| Fixture | Purpose |
| --- | --- |
| `fixture-show-argv` | Prints the raw command line and parsed arguments. |
| `fixture-show-console-input` | Reports input records read from the console. |
| `fixture-utf16-echo` | Writes specified UTF-16 code units through the console API. |
| `fixture-win32-echo1` | Echoes console input with `ReadConsole` and `WriteConsole`. |
| `fixture-win32-echo2` | Echoes key input obtained through `_getch`. |
| `fixture-win32-write1` | Exercises console geometry and character output. |
| `fixture-write-console` | Decodes escapes and writes through `WriteConsole`. |

The solution builds every fixture with Debug and Release symbols. The main
verifier checks their architecture, Windows 7 subsystem floor, imports,
version identity, and presence. It also runs `fixture-show-argv` as a
deterministic quoting check. The other fixtures require a real console or
specific input and will be driven by focused test cases in later roadmap
steps.
