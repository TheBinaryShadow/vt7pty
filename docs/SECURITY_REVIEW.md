# Native Boundary Security Review

This review records the Milestone 0 Step 0.7 audit of VT7Pty's application,
client, agent, child-process, and diagnostic boundaries. It describes the
current `0.5.0-dev` architecture; it is not a claim that arbitrary code in the
same Windows logon session is isolated from the user who owns that session.

## Named pipes and peer identity

| Boundary | Validation and permissions | Disposition |
| --- | --- | --- |
| Client to agent control pipe | An unpredictable name, `FILE_FLAG_FIRST_PIPE_INSTANCE`, `PIPE_REJECT_REMOTE_CLIENTS`, and a DACL for owner, LocalSystem, and administrators. After connection, the client requires the pipe client PID to equal the PID returned by its own `CreateProcessW` call. | Accepted. A wrong or unverifiable PID aborts session creation. |
| Agent data pipes | Unpredictable per-session names, one instance, remote-client rejection, and the same restricted DACL. | Accepted. The public API intentionally returns these names so an application-selected connector can open them; an exact PID restriction would break that contract. Possession of the name is the session capability inside the permitted logon boundary. |
| Diagnostic pipe | One local instance, remote-client rejection, and the restricted owner/System/administrators DACL. Clients request only `SECURITY_IDENTIFICATION`, preventing server impersonation. | Hardened. The inherited Everyone-write option was removed. Exact PID validation is not applicable because the collector accepts records from the DLL, agent, and test processes. `--self-test` validates the live descriptor. |

No product pipe uses the default process DACL. Pipe names are not written to
default diagnostics.

## Process creation and handles

The client resolves `VT7Pty-Agent.exe` beside the loaded VT7Pty DLL, passes
that absolute path as `lpApplicationName`, quotes the same path in the command
line, and disables handle inheritance. It does not search the current
directory or `PATH` for the agent. The agent control pipe name contains no
spaces or shell metacharacters, and all remaining agent arguments are numeric.
No command shell interprets this command line.

Child application names, command lines, working directories, and environment
blocks are application-owned API inputs. They are passed directly to
`CreateProcessW`; VT7Pty does not invoke a shell or reinterpret quoting. If
`appname` is omitted, Windows applies its documented executable resolution to
the caller's command line. This is an explicit caller-controlled behavior,
not an internal lookup.

Handle inheritance is disabled for ordinary child creation. The `CONERR`
mode enables inheritance so the child can receive the console standard input,
standard output, and the deliberately inheritable secondary console buffer.
The audit found no other inheritable agent-owned handle: named pipes and event
objects use null security attributes, and handles duplicated back to the
client are explicitly non-inheritable. This invariant must be re-audited if a
new inheritable handle is introduced.

## Executable and DLL lookup

The agent path is anchored to the loaded DLL and supplied explicitly to
`CreateProcessW`. Package verification confirms that maintained binaries have
only the documented system-DLL imports and no delay imports. VT7Pty runtime
code does not create or execute files in a temporary directory. Build,
verification, packaging, acceptance, and diagnostic scripts use explicit
repository, package, result, or caller-selected output directories.

## Environment and command data

The public C API requires valid readable pointers, as documented by its ABI.
The environment-block scan is bounded at the Windows `CreateProcessW` limit of
32,767 UTF-16 characters and requires a double NUL terminator. Environment
values and application command lines are serialized to the agent but are not
written to default diagnostics. Agent creation failures report a Windows error
code without copying the internal command line into the error text or log.

## IPC parsing and allocation

The existing protocol version remains 1 because valid messages retain their
wire encoding. Both peers now enforce a 1 MiB total control-packet maximum
before allocation. Outgoing packets are checked against the same limit.
String decoding checks multiplication and remaining-buffer boundaries;
integer narrowing uses checked conversion helpers.

The agent validates message type, spawn-flag mask, Boolean encodings, packet
completion, and positive resize dimensions. A malformed length, field, or
unknown message closes the control pipe and shuts down the agent instead of
reaching an assertion or continuing on a desynchronized stream. The client
closes its control handle after any failed RPC through `RpcOperation`.

Data-pipe queues remain bounded operationally by the single-session producer
and consumer flow but do not yet implement a fixed terminal-output ceiling.
That traffic carries terminal data rather than trusted protocol objects and
backpressure is applied by the pipe workers. Output-volume and throughput
stress belongs to the Step 0.8 test matrix.

## Shutdown and cancellation

Client control I/O is overlapped, waits on both the I/O event and agent
process, cancels timed-out I/O on the issuing thread, and waits for completion
before its stack-owned `OVERLAPPED` object is destroyed. Agent data-pipe
workers cancel pending operations and wait for cancellation completion before
closing their events and handles. A closed control pipe initiates agent
shutdown. These ownership rules avoid use-after-close of an `OVERLAPPED`
object in the reviewed paths.

The diagnostic collector uses synchronous one-message transactions, flushes
complete records, disconnects each client, rejects oversize messages, and
places a fixed bound on file growth.

## Diagnostic content

Every default record is bounded and identifies its exact build. Default
call sites log state transitions, geometry, platform details, and numeric
errors. The review removed agent command lines, API error messages, pipe names,
and raw pipe handles from default records. Keyboard and mouse content is
reachable only through the explicit `VT7PTY_DEBUG=trace,input` opt-in and is
flagged by the diagnostic bundle collector.

## Review result

No Step 0.7 security blocker remains. The accepted dispositions are the
application-controlled `CreateProcessW` resolution when `appname` is null,
name-capability authentication for public data pipes, and the narrow CONERR
handle-inheritance requirement. Each follows an existing public behavior and
has a documented trust boundary. Future work that changes process launch,
pipe consumers, or inheritable handles must update this review and its tests.
