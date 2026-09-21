# Credits and acknowledgements

VT7Pty exists because others solved difficult console and terminal problems
before us. We want their work to remain visible as this project develops.

## WinPTY

**Ryan Prichard** created [WinPTY](https://github.com/rprichard/winpty), the
implementation from which VT7Pty is derived. Its hidden-console agent, input
translation, screen scraping, resizing, and compatibility work provide our
starting point.

The inherited Git history through
[`7e59fe2`](https://github.com/rprichard/winpty/commit/7e59fe2d09adf0fa2aa606492e7ca98efbc5184e)
records contributions by:

- Ryan Prichard
- Jared Kells
- Josh Holtrop
- Sebastian Schuberth
- Uwe Stieber
- Christian Howe
- Haoming Zhu
- Johannes Schindelin
- Qian Hong
- Tereza Tomcova
- jackyzy823

This list reflects recorded commit authors, not every person who helped the
project. Thank you also to its reviewers, issue reporters, testers, downstream
maintainers, and users. The preserved history and
[upstream contributor page](https://github.com/rprichard/winpty/graphs/contributors)
provide further attribution.

WinPTY's original MIT copyright and license notices remain in
[LICENSE](LICENSE) and the inherited source files.

## Microsoft Console, ConPTY, and Windows Terminal

Thank you to **Microsoft's Console and Windows Terminal teams and the wider
open-source contributor community** for their work on ConPTY, the console host,
terminal parsing, text handling, and terminal behavior.

The [Microsoft Terminal repository](https://github.com/microsoft/terminal)
and [its contributors](https://github.com/microsoft/terminal/graphs/contributors)
are important references for VT7Pty's direction. ConPTY is currently a source
of design inspiration and behavioral guidance; this rebranding does not copy
its implementation or claim compatibility with it. If code is adopted later,
its exact provenance and required notices will be recorded at that time.

## VT7 and future VT7Pty contributors

[VT7](https://github.com/TheBinaryShadow/VT7) provides the motivation and primary
integration target for this work. Its community documentation informed the
structure and tone of VT7Pty's contribution, conduct, and security guidance.

Thank you to everyone who contributes code, documentation, reproductions,
Windows 7 testing, reviews, and constructive discussion. New work will remain
traceable through the repository history.

VT7Pty is an independent project. These acknowledgements do not imply upstream
endorsement or transfer ownership of anyone else's work.
