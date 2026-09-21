/*
 * Copyright (c) 2011-2016 Ryan Prichard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef VT7PTY_H
#define VT7PTY_H

#include <windows.h>

#include "vt7pty_constants.h"
#include "vt7pty_version.h"

/* On 32-bit Windows, VT7Pty functions have the default __cdecl (not __stdcall)
 * calling convention.  (64-bit Windows has only a single calling convention.)
 * When compiled with __declspec(dllexport), the VT7Pty functions are
 * unadorned--no underscore prefix or '@nn' suffix--so GetProcAddress can be
 * used easily. */
#ifdef COMPILING_VT7PTY_DLL
#define VT7PTY_API __declspec(dllexport)
#else
#define VT7PTY_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The VT7Pty API uses wide characters, instead of UTF-8, to avoid conversion
 * complications related to surrogates.  Windows generally tolerates unpaired
 * surrogates in text, which makes conversion to and from UTF-8 ambiguous and
 * complicated.  (There are different UTF-8 variants that deal with UTF-16
 * surrogates differently.) */



/*****************************************************************************
 * Error handling. */

/* All the APIs have an optional vt7pty_error_t output parameter.  If a
 * non-NULL argument is specified, then either the API writes NULL to the
 * value (on success) or writes a newly allocated vt7pty_error_t object.  The
 * object must be freed using vt7pty_error_free. */

/* An error object. */
typedef struct vt7pty_error_s vt7pty_error_t;
typedef vt7pty_error_t *vt7pty_error_ptr_t;

/* An error code -- one of VT7PTY_ERROR_xxx. */
typedef DWORD vt7pty_result_t;

/* Gets the error code from the error object. */
VT7PTY_API vt7pty_result_t vt7pty_error_code(vt7pty_error_ptr_t err);

/* Returns a textual representation of the error.  The string is freed when
 * the error is freed. */
VT7PTY_API LPCWSTR vt7pty_error_msg(vt7pty_error_ptr_t err);

/* Free the error object.  Every error returned from the VT7Pty API must be
 * freed. */
VT7PTY_API void vt7pty_error_free(vt7pty_error_ptr_t err);



/*****************************************************************************
 * Configuration of a new agent. */

/* The vt7pty_config_t object is not thread-safe. */
typedef struct vt7pty_config_s vt7pty_config_t;

/* Allocate a vt7pty_config_t value.  Returns NULL on error.  There are no
 * required settings -- the object may immediately be used.  agentFlags is a
 * set of zero or more VT7PTY_FLAG_xxx values.  An unrecognized flag results
 * in an assertion failure. */
VT7PTY_API vt7pty_config_t *
vt7pty_config_new(UINT64 agentFlags, vt7pty_error_ptr_t *err /*OPTIONAL*/);

/* Free the cfg object after passing it to vt7pty_open. */
VT7PTY_API void vt7pty_config_free(vt7pty_config_t *cfg);

VT7PTY_API void
vt7pty_config_set_initial_size(vt7pty_config_t *cfg, int cols, int rows);

/* Set the mouse mode to one of the VT7PTY_MOUSE_MODE_xxx constants. */
VT7PTY_API void
vt7pty_config_set_mouse_mode(vt7pty_config_t *cfg, int mouseMode);

/* Amount of time to wait for the agent to startup and to wait for any given
 * agent RPC request.  Must be greater than 0.  Can be INFINITE. */
VT7PTY_API void
vt7pty_config_set_agent_timeout(vt7pty_config_t *cfg, DWORD timeoutMs);



/*****************************************************************************
 * Start the agent. */

/* The vt7pty_t object is thread-safe. */
typedef struct vt7pty_s vt7pty_t;

/* Starts the agent.  Returns NULL on error.  This process will connect to the
 * agent over a control pipe, and the agent will open data pipes (e.g. CONIN
 * and CONOUT). */
VT7PTY_API vt7pty_t *
vt7pty_open(const vt7pty_config_t *cfg,
            vt7pty_error_ptr_t *err /*OPTIONAL*/);

/* A handle to the agent process.  This value is valid for the lifetime of the
 * vt7pty_t object.  Do not close it. */
VT7PTY_API HANDLE vt7pty_agent_process(vt7pty_t *wp);



/*****************************************************************************
 * I/O pipes. */

/* Returns the names of named pipes used for terminal I/O.  Each input or
 * output direction uses a different half-duplex pipe.  The agent creates
 * these pipes, and the client can connect to them using ordinary I/O methods.
 * The strings are freed when the vt7pty_t object is freed.
 *
 * vt7pty_conerr_name returns NULL unless VT7PTY_FLAG_CONERR is specified.
 *
 * N.B.: CreateFile does not block when connecting to a local server pipe.  If
 * the server pipe does not exist or is already connected, then it fails
 * instantly. */
VT7PTY_API LPCWSTR vt7pty_conin_name(vt7pty_t *wp);
VT7PTY_API LPCWSTR vt7pty_conout_name(vt7pty_t *wp);
VT7PTY_API LPCWSTR vt7pty_conerr_name(vt7pty_t *wp);



/*****************************************************************************
 * VT7Pty agent RPC call: process creation. */

/* The vt7pty_spawn_config_t object is not thread-safe. */
typedef struct vt7pty_spawn_config_s vt7pty_spawn_config_t;

/* vt7pty_spawn_config strings do not need to live as long as the config
 * object.  They are copied.  Returns NULL on error.  spawnFlags is a set of
 * zero or more VT7PTY_SPAWN_FLAG_xxx values.  An unrecognized flag results in
 * an assertion failure.
 *
 * env is a a pointer to an environment block like that passed to
 * CreateProcess--a contiguous array of NUL-terminated "VAR=VAL" strings
 * followed by a final NUL terminator.
 *
 * N.B.: If you want to gather all of the child's output, you may want the
 * VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN flag.
 */
VT7PTY_API vt7pty_spawn_config_t *
vt7pty_spawn_config_new(UINT64 spawnFlags,
                        LPCWSTR appname /*OPTIONAL*/,
                        LPCWSTR cmdline /*OPTIONAL*/,
                        LPCWSTR cwd /*OPTIONAL*/,
                        LPCWSTR env /*OPTIONAL*/,
                        vt7pty_error_ptr_t *err /*OPTIONAL*/);

/* Free the cfg object after passing it to vt7pty_spawn. */
VT7PTY_API void vt7pty_spawn_config_free(vt7pty_spawn_config_t *cfg);

/*
 * Spawns the new process.
 *
 * The function initializes all output parameters to zero or NULL.
 *
 * On success, the function returns TRUE.  For each of process_handle and
 * thread_handle that is non-NULL, the HANDLE returned from CreateProcess is
 * duplicated from the agent and returned to the VT7Pty client.  The client is
 * responsible for closing these HANDLES.
 *
 * On failure, the function returns FALSE, and if err is non-NULL, then *err
 * is set to an error object.
 *
 * If the agent's CreateProcess call failed, then *create_process_error is set
 * to GetLastError(), and the VT7PTY_ERROR_SPAWN_CREATE_PROCESS_FAILED error
 * is returned.
 *
 * vt7pty_spawn can only be called once per vt7pty_t object.  If it is called
 * before the output data pipe(s) is/are connected, then collected output is
 * buffered until the pipes are connected, rather than being discarded.
 *
 * N.B.: GetProcessId works even if the process has exited.  The PID is not
 * recycled until the NT process object is freed.
 * (https://blogs.msdn.microsoft.com/oldnewthing/20110107-00/?p=11803)
 */
VT7PTY_API BOOL
vt7pty_spawn(vt7pty_t *wp,
             const vt7pty_spawn_config_t *cfg,
             HANDLE *process_handle /*OPTIONAL*/,
             HANDLE *thread_handle /*OPTIONAL*/,
             DWORD *create_process_error /*OPTIONAL*/,
             vt7pty_error_ptr_t *err /*OPTIONAL*/);



/*****************************************************************************
 * VT7Pty agent RPC calls: everything else */

/* Change the size of the Windows console window. */
VT7PTY_API BOOL
vt7pty_set_size(vt7pty_t *wp, int cols, int rows,
                vt7pty_error_ptr_t *err /*OPTIONAL*/);

/* Gets a list of processes attached to the console. */
VT7PTY_API int
vt7pty_get_console_process_list(vt7pty_t *wp, int *processList, const int processCount,
                                vt7pty_error_ptr_t *err /*OPTIONAL*/);

/* Frees the vt7pty_t object and the OS resources contained in it.  This
 * call breaks the connection with the agent, which should then close its
 * console, terminating the processes attached to it.
 *
 * This function must not be called if any other threads are using the
 * vt7pty_t object.  Undefined behavior results. */
VT7PTY_API void vt7pty_free(vt7pty_t *wp);



/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* VT7PTY_H */
