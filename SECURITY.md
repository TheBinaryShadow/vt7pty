# VT7Pty Security Policy

VT7Pty welcomes reports about vulnerabilities in its console backend, including
issues inherited from WinPTY.

## Report a vulnerability privately

Please do not open a public issue or pull request containing vulnerability or
exploit details.

Use [GitHub private vulnerability reporting](https://github.com/TheBinaryShadow/vt7pty/security/advisories/new)
when it is available. If the form is unavailable or you prefer email, contact
[r@binaryshadow.hr](mailto:r@binaryshadow.hr) with the subject
`VT7Pty security report`.

Include what you safely can:

- The affected commit or release and how the binaries were obtained or built.
- Windows version, architecture, and update level.
- A description of the issue, impact, and required conditions.
- Reproduction steps or a minimal proof of concept.
- Relevant logs, stack traces, or dumps with secrets and personal data removed.
- Whether the issue also reproduces in upstream WinPTY, if known.
- Any suggested mitigation and your preferred contact method.

Do not send credentials or unrelated private terminal/session content. Reports
are reviewed as maintainer availability permits, without a guaranteed response
deadline. We aim to investigate privately and coordinate disclosure, fixes or
mitigations, and reporter credit with the people involved. Tell us if you prefer
not to be named.

## Supported versions

VT7Pty is in early development and has no supported stable release series.
The inherited `0.4.4-dev` version identifies the WinPTY baseline; it is not a
VT7Pty security-support commitment.

The approved product target is Windows 7 SP1 and later on x64. XP, Vista, and
x86 are outside the maintained VT7Pty compatibility policy. This project policy
does not provide operating-system vendor support.

| Version | Status |
| --- | --- |
| Current VT7Pty development branch | Reports welcome; no stability or response-time guarantee |
| Historical upstream WinPTY versions | No separate VT7Pty maintenance commitment; report issues affecting our baseline |
| Third-party or modified builds | Reproduction against identifiable repository source may be needed |

This policy will be revised when VT7Pty publishes a supported release.
Current platform and application claims are tracked separately in the
[compatibility contract](docs/COMPATIBILITY.md).

## Scope

Relevant areas include console input parsing, output handling, named pipes,
agent/client communication, process creation, handle inheritance, session
cleanup, and build or distribution behavior. Report suspected boundary or
privilege problems even when they require a local attacker.

Issues in VT7 itself should follow
[VT7's security policy](https://github.com/TheBinaryShadow/VT7/security/policy).
A VT7Pty fix does not establish security or vendor support for the operating
system. OS vulnerabilities should follow the relevant vendor disclosure process.
