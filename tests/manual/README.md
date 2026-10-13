# Manual and research probes

I keep these WinPTY-derived probes for investigations that need an interactive
console, controlled timing, a particular Windows version or deliberately
disruptive behavior. They are research tools, not routine verification tests.

| Area | Programs |
| --- | --- |
| Resize and geometry | `BufferResizeTests`, `GetConsolePos`, `MoveConsoleWindow`, `SetBufferSize`, `SetBufInfo`, `SetCursorPos`, `SetWindowRect`, `Win10ResizeWhileFrozen` |
| Screen buffers and selection | `ChangeScreenBuffer`, `ClearConsole`, `FreezePerfTest`, `ScreenBufferFreezeInactive`, `ScreenBufferTest`, `ScreenBufferTest2`, `SelectAllTest` |
| Unicode, wrapping, and large reads | `UnicodeDoubleWidthTest`, `UnicodeEncodingTest`, `UnicodeWideTest1`, `UnicodeWideTest2`, `VeryLargeRead`, `Win10WrapTest1`, `Win10WrapTest2` |
| Fonts and platform observation | `FontSurvey`, `GetFont`, `IsNewConsole`, `SetFont` |
| Console and input behavior | `VkEscapeTest`, `Win32Test1`, `Win32Test2`, `Win32Test3` |
| Destructive Windows bug reproduction | `winbug-15048`, `WindowsBugCrashReader` |

`TestUtil.cc` and `TimeMeasurement.h` are inherited support files used by many
probes. Converting the included-implementation pattern into ordinary compiled
support code is work to do if a probe joins the maintained suite. Destructive
cases must run only in an isolated test group with explicit cleanup and timeouts.
