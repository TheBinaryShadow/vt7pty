# GitHub repository setup

I manage these settings on GitHub. Local documentation changes do not change
the repository's settings or publish a release.

## About and topics

My suggested About description is:

> WinPTY-derived user-mode console backend for Windows 7+, focused on fidelity, reliability, and future VT7 integration. Inspired by ConPTY.

The repository URL is vt7pty; the display name is **VT7Pty**. Suggested topics:

~~~text
windows-7 winpty pty pseudoconsole console terminal conpty vt7 cpp
~~~

An optional website link can point to [VT7](https://github.com/TheBinaryShadow/VT7).
I avoid describing VT7Pty as a working ConPTY replacement before tests support
that claim.

## Community settings

I keep Issues enabled for ordinary reports and check that GitHub recognizes
the root CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md and LICENSE files.
Branch protection can be configured to suit the repository workflow.

For private security reports, enable **Private vulnerability reporting** and
check that **Report a vulnerability** appears under security advisories.
[GitHub's setup instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository)
cover the current settings. The fallback address is r@binaryshadow.hr, also
used for conduct reports.

## Releases and history

I use local verification and manual [GitHub Releases](RELEASING.md), with no
CI status badge. The [0.5.0 publication record](validation/2026-10-02-0.5.0-publication.md)
records the published baseline.

The milestone-0-start tag is the original planning marker. It is separate
from release tags and carries no binary assets. Its exact commit is recorded
in the [inherited baseline](validation/2026-09-21-upstream-baseline.md).

A social preview, Discussions and issue templates can be added when useful.
