# GitHub repository setup

These are maintainer actions for the repository page. Editing local files does
not change GitHub settings. No settings were changed during the rebranding pass.

## Suggested About description

> WinPTY-derived user-mode console backend for Windows 7, focused on fidelity, reliability, and future VT7 integration. Inspired by ConPTY.

The repository can keep its current `vt7pty` URL; the project display name in
documentation is **VT7Pty**. A repository rename is not required.

Suggested topics:

```text
windows-7 winpty pty pseudoconsole console terminal conpty vt7 cpp
```

An optional website link can point to [VT7](https://github.com/TheBinaryShadow/VT7)
until VT7Pty has a dedicated site. Do not label the project as a working ConPTY
replacement or a qualified release yet.

## Publishing and community files

1. Review, commit, and push the rebranding changes to the default branch, or
   merge them through a pull request. Local edits are not yet visible on GitHub.
2. Edit the repository's **About** description and topics using the text above.
3. Check that Issues are enabled under repository settings, since the README
   and contribution guide direct ordinary reports there. Enable them if needed.
4. Check the repository's community profile after publication. The root
   `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, and `LICENSE` files
   provide the contribution, conduct, security, and license documents. These
   do not require separate plugins or duplicate files created through the UI.

## Private vulnerability reporting

Enable **Private vulnerability reporting** if it is not already enabled:

1. Open repository **Settings**.
2. Under **Security and quality**, open **Advanced Security**.
3. Enable **Private vulnerability reporting**.
4. Check that **Report a vulnerability** appears under security advisories,
   and configure notifications so reports reach the maintainer.

See [GitHub's current instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
The security policy also provides `r@binaryshadow.hr` as a fallback. Confirm
that this inbox is monitored for both security and conduct reports.

## Build status and optional presentation

The old README badge reported the upstream WinPTY AppVeyor project's status.
It is omitted from the new README because it did not validate this fork.
The inherited `appveyor.yml` and all build scripts remain unchanged. A new
badge should be added only after a VT7Pty CI job exists and has been verified.

A social preview image, Discussions, issue templates, and branch protection
can be configured later if useful. No default-branch rename, history rewrite,
new release, or tag is required for this documentation rebranding.
