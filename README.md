# VT7Pty

**A WinPTY-derived console backend for Windows 7, built for VT7.**

VT7Pty is a user-mode PTY project whose goal is to improve the fidelity and
reliability of Windows console applications and prepare a better backend for
[VT7](https://github.com/TheBinaryShadow/VT7).

Our foundation is [WinPTY](https://github.com/rprichard/winpty), created by
Ryan Prichard and developed with its contributors. Microsoft's
[ConPTY and Windows Terminal work](https://github.com/microsoft/terminal)
provides architectural inspiration and a reference for modern terminal
behavior. We are building on years of engineering by both communities.

## Project status

VT7Pty is in early development. This repository currently contains the inherited
WinPTY `0.4.4-dev` implementation, with a new project identity and development
direction. A VT7Pty-specific API and backend improvements are planned work.

All inherited components are still present, including the Cygwin/MSYS Unix
adapter, tests, debugging tools, and build and packaging scripts. Binaries,
API symbols, environment variables, and build targets retain their existing
WinPTY names. The rebranding does not change runtime behavior.

Windows 7 SP1 x64 is the primary target for VT7Pty. Historical WinPTY support
claims and release notes describe upstream behavior; they are not acceptance
results for future VT7Pty builds. Changes must be validated on the target OS.

## What we are working toward

- More faithful console output, keyboard input, Unicode handling, resizing,
  and screen restoration.
- Predictable process and session lifetimes, clean teardown, and useful
  diagnostics.
- A reproducible native Windows build and tests that expose regressions and
  inherited limitations.
- A documented API that makes eventual VT7 integration straightforward,
  using ConPTY conventions where they fit the legacy-console backend.

VT7 is the primary intended consumer. Compatibility for other consumers,
including a possible ConPTY user-mode shim, is a later objective. VT7Pty does
not currently provide a drop-in ConPTY replacement. A kernel driver is not
part of the project direction.

## How the current backend works

The client library starts an agent that owns a hidden Windows console. The
agent launches console applications, translates terminal input into Windows
console events, and turns observed screen-buffer changes into VT output.

```text
Terminal host
    |
    | WinPTY API today / VT7Pty API planned
    v
Client DLL <---- control and I/O ----> Agent
                                      |
                                      v
                               Hidden Windows console
                                      |
                                      v
                               Console applications
```

This preserves the legacy-console foundation that makes WinPTY useful on
Windows 7. Backend improvements will be guided by reproducible application
failures and tests, with proven behavior retained where it meets our needs.

## Building and exploring

See [Building](BUILDING.md) for the inherited build entry points and their
limitations. The [original WinPTY README](docs/UPSTREAM_WINPTY_README.md)
is preserved for historical build, adapter, embedding, and debugging details.

Useful starting points:

- [Public WinPTY header](src/include/winpty.h)
- [Client library](src/libwinpty)
- [Console agent](src/agent)
- [Tests](src/tests)
- [Upstream baseline and attribution policy](UPSTREAM.md)
- [Historical WinPTY release notes](RELEASES.md)

## Contributing and community

Read [Contributing](CONTRIBUTING.md) before opening a pull request. Report
ordinary bugs and discuss proposals in the
[VT7Pty issue tracker](https://github.com/TheBinaryShadow/vt7pty/issues).

The [Code of Conduct](CODE_OF_CONDUCT.md) applies to project spaces. For
vulnerabilities, use the private reporting instructions in our
[Security Policy](SECURITY.md).

## Credits and license

Thank you to **Ryan Prichard and the WinPTY contributors** for the legacy
console bridge on which this project is based, and to **Microsoft's Console,
ConPTY, and Windows Terminal teams and community contributors** for the work
that informs its future direction. See [Credits](CREDITS.md) for acknowledgements
and the distinction between inherited code and design references.

VT7Pty is distributed under the [MIT License](LICENSE). Original copyright
notices and upstream history are retained. VT7Pty is an independent project
and is not an official Microsoft product or an upstream WinPTY release.
