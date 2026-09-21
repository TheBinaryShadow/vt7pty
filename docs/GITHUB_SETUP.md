# GitHub Repository Setup

These are maintainer actions for the repository page and the approved
Milestone 0 foundation. Local documentation edits do not change GitHub settings.

## Suggested About description

> WinPTY-derived user-mode console backend for Windows 7+, focused on fidelity, reliability, and future VT7 integration. Inspired by ConPTY.

The repository can keep its current `vt7pty` URL; the project display name is
**VT7Pty**. A repository rename is not required.

Suggested topics:

```text
windows-7 winpty pty pseudoconsole console terminal conpty vt7 cpp
```

An optional website link can point to [VT7](https://github.com/TheBinaryShadow/VT7)
until VT7Pty has a dedicated site. Do not describe VT7Pty as a working ConPTY
replacement or qualified binary release before evidence supports that claim.

## Publish the development foundation

The approved foundation becomes the implementation starting point when its
documentation commit is on the default branch and the exact commit is marked
with the annotated tag `milestone-0-start`.

After reviewing the local commit:

```powershell
git push origin main
git tag -a milestone-0-start -m "Mark the approved VT7Pty Milestone 0 foundation"
git push origin milestone-0-start
```

Create the tag only after the foundation commit is on `main`. The tag records
planning approval; it is not a release and should have no binary assets.

## Community and repository settings

1. Edit the repository **About** description and topics using the text above.
2. Check that Issues are enabled, since project documentation directs ordinary
   reports there.
3. Check the repository community profile. The root `CONTRIBUTING.md`,
   `CODE_OF_CONDUCT.md`, `SECURITY.md`, and `LICENSE` files provide the expected
   community documents.
4. Consider branch protection for `main` if it fits the maintainer workflow.

## Private vulnerability reporting

Enable **Private vulnerability reporting** if it is not already enabled:

1. Open repository **Settings**.
2. Under **Security and quality**, open **Advanced Security**.
3. Enable **Private vulnerability reporting**.
4. Check that **Report a vulnerability** appears under security advisories and
   configure notifications so reports reach the maintainer.

See [GitHub's instructions](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).
The security policy provides `r@binaryshadow.hr` as a fallback. Confirm that
this inbox is monitored for security and conduct reports.

## Build and release presentation

The old README badge reported the upstream WinPTY AppVeyor project and is
intentionally absent. VT7Pty will not add a build-status badge because the
approved workflow uses local verification and physical acceptance.

Published versions use manual GitHub Releases and the process in
[Releasing](RELEASING.md). Do not create a `0.5.0` release or tag until
Milestone 0 acceptance and its separate publication review are complete.

A social preview image, Discussions, issue templates, and additional repository
presentation can be configured later if useful.
