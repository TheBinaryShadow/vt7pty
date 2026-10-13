# VT7Pty documentation

I keep the [roadmap](../ROADMAP.md) as the source of truth for progress.
These guides explain how to use the repository, how the backend works, and
what I've learned from testing it.

## Build and use the project

| Guide | Purpose |
| --- | --- |
| [Building](../BUILDING.md) | Set up Visual Studio, build, verify and debug |
| [Testing](TESTING.md) | Choose the right test suite and understand its limits |
| [Windows 7 acceptance](WINDOWS7_ACCEPTANCE.md) | Run a portable release candidate on the test machines |
| [Diagnostics](DIAGNOSTICS.md) | Collect logs and failure bundles |
| [Releasing](RELEASING.md) | Package, review and publish a release |
| [Compatibility](COMPATIBILITY.md) | Platform targets, test machines and verified support |

## Understand the backend

| Guide | Purpose |
| --- | --- |
| [Architecture](ARCHITECTURE.md) | Components, input/output flow and resource ownership |
| [Versioning](VERSIONING.md) | Package, API and client-agent protocol versions |
| [Naming](NAMING.md) | Migration from WinPTY names to VT7Pty |
| [Security review](SECURITY_REVIEW.md) | Native boundaries and remaining limitations |

## Follow current development

I'm working on input cancellation within Milestone 1. Host identity and dispatch
are verified; the first private-desktop cancellation prototype is ready for target testing.

| Guide | Purpose |
| --- | --- |
| [Fidelity plan](FIDELITY.md) | Required applications, measurements and remaining coverage |
| [Fidelity test bundle](FIDELITY_BASELINE.md) | Run and interpret the shell and fixture tests |
| [Cancellation investigation](INPUT_CANCELLATION.md) | Failed approaches and what they established |
| [Console-host design](HOST_CANCELLATION_DESIGN.md) | Identity, dispatch, cancellation and production stages |
| [Host identity probe](HOST_IDENTITY_PROBE.md) | Run the H1 research package |
| [Host dispatch probe](HOST_DISPATCH_PROBE.md) | Run the H2 research package |
| [Host cancellation probe](HOST_CANCELLATION_PROBE.md) | Run the first H3 prototype and see its remaining limits |
| [Optional SSH testing](SSH_BASELINE.md) | Deferred remote-test prerequisites |

## Results and project history

I keep failed experiments and original machine records alongside successful
runs. Each dated review describes the state at that time; the roadmap describes
the current state.

- [Validation index](validation/README.md): candidates, test reviews and raw evidence.
- [0.5.0 release notes](releases/0.5.0.md) and [changelog](../CHANGELOG.md).
- [Completed Milestone 0 checklist](MILESTONE_0.md).
- [Development foundation](FOUNDATION.md) and [component inventory](INVENTORY.md).
- [Upstream provenance](../UPSTREAM.md), [original WinPTY README](UPSTREAM_WINPTY_README.md),
  [upstream release notes](../RELEASES.md) and [historical research](historical/README.md).

## Project maintenance

[Contributing](../CONTRIBUTING.md), [conduct](../CODE_OF_CONDUCT.md),
[security reporting](../SECURITY.md) and [GitHub setup](GITHUB_SETUP.md)
cover the repository workflow. I keep documentation in first person for my
decisions and reviews, use direct language for technical facts, and link to
detailed evidence instead of repeating it. Raw test records keep their original
bytes and results.
