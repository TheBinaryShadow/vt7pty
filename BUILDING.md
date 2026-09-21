# Building VT7Pty

The repository currently retains WinPTY's original build and packaging systems.
Rebranding has not renamed their targets or introduced a new build system.

## Native Windows library and tools

The native components include `winpty.dll`, `winpty-agent.exe`, and
`winpty-debugserver.exe`. They do not require the Cygwin/MSYS Unix adapter.
The current public interface is [winpty.h](src/include/winpty.h).

The inherited MSVC entry point is [vcbuild.bat](vcbuild.bat). Its prerequisites
include native Python 2, GYP, and MSBuild in a suitable Visual Studio developer
environment. The script attempts to download GYP when `build-gyp` is absent.
Read the script and [GYP configuration](src/configurations.gypi) before using
it; these are historical workflows, not a newly qualified modern build recipe.

The original instructions describe generating a solution from
[src/winpty.gyp](src/winpty.gyp). See the
[upstream README](docs/UPSTREAM_WINPTY_README.md#embedding-winpty--msvc-compilation)
for those details.

## Cygwin/MSYS adapter

The Unix adapter remains in [src/unix-adapter](src/unix-adapter). Its inherited
build uses `configure` and GNU Make, with separate toolchains for the native
Windows components and the Cygwin/MSYS adapter. Historical prerequisites and
commands are preserved in the
[upstream README](docs/UPSTREAM_WINPTY_README.md#cygwinmsys-adapter-winptyexe).

## Debugging and validation

The inherited debugger uses `winpty-debugserver.exe` and `WINPTY_DEBUG=trace`.
`WINPTY_SHOW_CONSOLE=1` makes the backing console visible for investigation.
See the [upstream debugging instructions](docs/UPSTREAM_WINPTY_README.md#debugging-winpty).

Existing tests are in [src/tests](src/tests). Report the exact compiler, SDK,
architecture, configuration, source revision, and tests run with a change.
Passing on a newer development OS does not establish Windows 7 compatibility.

A reproducible VT7Pty build, automated validation, and a Windows 7 acceptance
procedure are planned work. No new CI result or release qualification is
implied by this documentation pass. The inherited [AppVeyor configuration](appveyor.yml)
and [packaging scripts](ship) are preserved for review.
