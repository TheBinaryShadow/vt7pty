# VT7Pty

**A WinPTY-derived console backend for Windows 7 and later, built for VT7.**

I'm building VT7Pty to give [VT7](https://github.com/TheBinaryShadow/VT7) a more
faithful and reliable backend for Windows console applications. The project
runs entirely in user mode and targets Windows 7 SP1 and later on x64.

My starting point is [WinPTY](https://github.com/rprichard/winpty), created by
Ryan Prichard and its contributors. Microsoft's
[ConPTY and Windows Terminal work](https://github.com/microsoft/terminal)
provides a reference for behavior and API design. I want to build on that work
while keeping Windows 7 support practical.

## Where things stand

I've published the [0.5.0 baseline](https://github.com/TheBinaryShadow/vt7pty/releases/tag/v0.5.0).
It establishes the modern build, renamed native backend, diagnostics and test
system. The exact release passed all 30 acceptance cases and a two-hour soak
on each of my three Windows 7 test machines, including a PowerShell 2.0
installation without KB3191566. See the [release review](docs/validation/2026-10-02-0.5.0-release-acceptance.md).

I'm now working on console fidelity. Ctrl+C at CMD and native Windows PowerShell
prompts still leaves typed input behind. I've verified host identification and
a test-only dispatch channel on all three machines. The first
[cancellation prototype](docs/HOST_CANCELLATION_PROBE.md) now passes its core local
workflows on private desktops. Windows 7 testing and the remaining H3 guards
come before any runtime integration.

VT7 integration comes after the backend and its API are ready. VT7Pty does not
yet replace ConPTY for unmodified applications. I am not planning a kernel driver.

## Build, test or follow development

| Start here | What you'll find |
| --- | --- |
| [Building](BUILDING.md) | Tools, local commands and debugging |
| [Testing](docs/TESTING.md) | Local checks and Windows 7 acceptance |
| [Roadmap](ROADMAP.md) | Completed milestones, current work and next steps |
| [Documentation](docs/README.md) | Architecture, guides, design notes and results |
| [Compatibility](docs/COMPATIBILITY.md) | Platform targets and what I've actually tested |

## How it works

The client DLL starts an agent with a hidden Windows console. The agent launches
the application, translates terminal input into console events, and turns
screen-buffer changes into VT output.

~~~text
Terminal host <--> VT7Pty.dll <--> VT7Pty-Agent.exe
                                         |
                                  Hidden console
                                         |
                                 Console application
~~~

This retains the console bridge that made WinPTY useful on older Windows.
I'm improving it through measured application behavior and regression tests.
The [architecture guide](docs/ARCHITECTURE.md) explains the boundaries.

## Contributing

Read [Contributing](CONTRIBUTING.md) before opening a pull request. Bugs and
proposals belong in the [issue tracker](https://github.com/TheBinaryShadow/vt7pty/issues).
The [Code of Conduct](CODE_OF_CONDUCT.md) applies to project spaces; use the
[Security Policy](SECURITY.md) for private vulnerability reports.

## Credits and license

I'm grateful to **Ryan Prichard and the WinPTY contributors**, and to
**Microsoft's Console, ConPTY and Windows Terminal teams and community
contributors**. Their engineering makes this project possible.
[Credits](CREDITS.md) and [Upstream provenance](UPSTREAM.md) distinguish inherited
code from design references.

VT7Pty uses the [MIT License](LICENSE). Original copyright notices and upstream
history are retained. This is an independent project, not an official Microsoft
product or an upstream WinPTY release.
