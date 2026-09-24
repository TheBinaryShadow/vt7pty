# VT7Pty

**A modern WinPTY-derived console backend for Windows 7 and later, built for VT7.**

VT7Pty is a user-mode PTY project whose goal is to improve the fidelity and
reliability of Windows console applications and prepare a better backend for
[VT7](https://github.com/TheBinaryShadow/VT7).

Our foundation is [WinPTY](https://github.com/rprichard/winpty), created by
Ryan Prichard and developed with its contributors. Microsoft's
[ConPTY and Windows Terminal work](https://github.com/microsoft/terminal)
provides architectural inspiration and a reference for modern terminal
behavior. We are building on years of engineering by both communities.

## Project status

VT7Pty is in early development at version `0.5.0-dev`. Milestone 0 Steps 0.1
through 0.4 established the inherited baseline, the supported VS2022/MSBuild
workflow, a native-only active tree, and the accepted Windows 7+ platform
implementation. Step 0.5 established and physically accepted the technical
VT7Pty identity. Step 0.6 modernized and physically accepted the retained
native code with warning-clean C++20 build and analysis gates. Step 0.7
modernized and physically accepted the diagnostic and security boundaries on
both Windows 7 tiers. The permanent test system proceeds in Step 0.8.
Its expanded local and portable test candidate awaits both Windows 7 results.

The [development foundation](docs/FOUNDATION.md) and
[roadmap](ROADMAP.md) record the approved product and engineering direction.
The [diagnostic collection guide](docs/DIAGNOSTICS.md) and
[native boundary review](docs/SECURITY_REVIEW.md) define the Step 0.7 support
and security baseline.

Milestone 0 will replace the complete inherited toolchain, remove the Unix
adapter and pre-Windows 7 compatibility, establish the native VT7Pty 0.5.x
identity, modernize the retained C++ code, and create the permanent local build,
test, diagnostic, and release systems.

The Cygwin/MSYS adapter and superseded GYP, GNU Make, Python 2 packaging, and
hosted-build infrastructure have been removed. Active binaries, headers,
exports, environment variables, named endpoints, diagnostics, and tests use
the VT7Pty identity. The [technical naming map](docs/NAMING.md) records every
intentional compatibility break.

VT7Pty targets x64 Windows 7 SP1 and later. Windows XP, Windows Vista, and x86
are outside the maintained product. Historical WinPTY support claims and
release notes describe upstream behavior; they are not acceptance results for
future VT7Pty builds. Milestone and release candidates will be validated on
physical non-ESU and ESU Windows 7 SP1 x64 machines.

## What we are working toward

- More faithful console output, keyboard input, Unicode handling, resizing,
  and screen restoration.
- Predictable process and session lifetimes, clean teardown, and useful
  diagnostics.
- A modern Visual Studio/MSBuild and C++20 codebase with reproducible local
  build, verification, packaging, and physical Windows 7 acceptance.
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
    | VT7Pty C API
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

See [Building](BUILDING.md) for the supported local build, verify, and package
commands and the remaining transition limitations. The
[original WinPTY README](docs/UPSTREAM_WINPTY_README.md) is preserved for
historical build, adapter, embedding, and debugging details.

Useful starting points:

- [Roadmap and milestone acceptance criteria](ROADMAP.md)
- [Approved development foundation](docs/FOUNDATION.md)
- [Inherited component inventory](docs/INVENTORY.md)
- [Accepted inherited native baseline](docs/validation/2026-09-21-upstream-baseline.md)
- [Accepted MSBuild transition validation](docs/validation/2026-09-21-msbuild-transition.md)
- [Accepted Step 0.2 completion validation](docs/validation/2026-09-22-step-0.2-completion.md)
- [Accepted Step 0.3 completion validation](docs/validation/2026-09-22-step-0.3-completion.md)
- [Step 0.4 platform candidate](docs/validation/2026-09-22-step-0.4-candidate.md)
- [Accepted Step 0.4 completion validation](docs/validation/2026-09-22-step-0.4-completion.md)
- [Step 0.5 technical identity candidate](docs/validation/2026-09-22-step-0.5-candidate.md)
- [Accepted Step 0.5 completion validation](docs/validation/2026-09-22-step-0.5-completion.md)
- [Step 0.6 native modernization candidate](docs/validation/2026-09-22-step-0.6-candidate.md)
- [Accepted Step 0.6 completion validation](docs/validation/2026-09-23-step-0.6-completion.md)
- [Architecture and current data flows](docs/ARCHITECTURE.md)
- [Testing strategy and evidence requirements](docs/TESTING.md)
- [Windows 7 physical acceptance procedure](docs/WINDOWS7_ACCEPTANCE.md)
- [Compatibility targets and verified status](docs/COMPATIBILITY.md)
- [Versioning policy](docs/VERSIONING.md)
- [Technical naming map](docs/NAMING.md)
- [Public VT7Pty header](src/include/vt7pty.h)
- [Client library](src/libvt7pty)
- [Console agent](src/agent)
- [Tests](tests)
- [Upstream baseline and attribution policy](UPSTREAM.md)
- [Historical WinPTY release notes](RELEASES.md)
- [VT7Pty changelog](CHANGELOG.md)
- [Manual release process](docs/RELEASING.md)

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
