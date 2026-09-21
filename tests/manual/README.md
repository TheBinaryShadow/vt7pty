# Manual and research probes

These WinPTY-derived programs preserve focused investigations that need an
interactive console, controlled timing, a particular Windows version, or
deliberately disruptive behavior. They are source material for the permanent
test system in Roadmap Step 0.8 and are not part of routine verification yet.

| Area | Programs |
| --- | --- |
| Resize and geometry | `BufferResizeTests`, `GetConsolePos`, `MoveConsoleWindow`, `SetBufferSize`, `SetBufInfo`, `SetCursorPos`, `SetWindowRect`, `Win10ResizeWhileFrozen` |
| Screen buffers and selection | `ChangeScreenBuffer`, `ClearConsole`, `FreezePerfTest`, `ScreenBufferFreezeInactive`, `ScreenBufferTest`, `ScreenBufferTest2`, `SelectAllTest` |
| Unicode, wrapping, and large reads | `UnicodeDoubleWidthTest`, `UnicodeWideTest1`, `UnicodeWideTest2`, `VeryLargeRead`, `Win10WrapTest1`, `Win10WrapTest2` |
| Fonts and platform observation | `FontSurvey`, `GetFont`, `IsNewConsole`, `OSVersion`, `SetFont` |
| Console and input behavior | `VkEscapeTest`, `Win32Test1`, `Win32Test2`, `Win32Test3` |
| Destructive Windows bug reproduction | `winbug-15048`, `WindowsBugCrashReader` |

`TestUtil.cc` is the inherited shared helper included directly by many probes.
Converting that pattern into ordinary compiled support code belongs to their
Step 0.8 integration. The destructive cases must run only in an isolated test
group with explicit cleanup and timeouts.
