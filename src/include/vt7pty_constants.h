/*
 * Copyright (c) 2016 Ryan Prichard
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

#ifndef VT7PTY_CONSTANTS_H
#define VT7PTY_CONSTANTS_H

/*
 * You may want to include vt7pty.h instead, which includes this header.
 *
 * This file is split out from vt7pty.h so that the agent can access the
 * VT7Pty flags without also declaring the client-library APIs.
 */

/*****************************************************************************
 * Error codes. */

#define VT7PTY_ERROR_SUCCESS                        0
#define VT7PTY_ERROR_OUT_OF_MEMORY                  1
#define VT7PTY_ERROR_SPAWN_CREATE_PROCESS_FAILED    2
#define VT7PTY_ERROR_LOST_CONNECTION                3
#define VT7PTY_ERROR_AGENT_EXE_MISSING              4
#define VT7PTY_ERROR_UNSPECIFIED                    5
#define VT7PTY_ERROR_AGENT_DIED                     6
#define VT7PTY_ERROR_AGENT_TIMEOUT                  7
#define VT7PTY_ERROR_AGENT_CREATION_FAILED          8
#define VT7PTY_ERROR_AGENT_INCOMPATIBLE              9



/*****************************************************************************
 * Configuration of a new agent. */

/* Create a new screen buffer (connected to the "conerr" terminal pipe) and
 * pass it to child processes as the STDERR handle.  This flag also prevents
 * the agent from reopening CONOUT$ when it polls -- regardless of whether the
 * active screen buffer changes, VT7Pty continues to monitor the original
 * primary screen buffer. */
#define VT7PTY_FLAG_CONERR              0x1ull

/* Don't output escape sequences. */
#define VT7PTY_FLAG_PLAIN_OUTPUT        0x2ull

/* Do output color escape sequences.  These escapes are output by default, but
 * are suppressed with VT7PTY_FLAG_PLAIN_OUTPUT.  Use this flag to reenable
 * them. */
#define VT7PTY_FLAG_COLOR_ESCAPES       0x4ull

#define VT7PTY_FLAG_MASK (0ull \
    | VT7PTY_FLAG_CONERR \
    | VT7PTY_FLAG_PLAIN_OUTPUT \
    | VT7PTY_FLAG_COLOR_ESCAPES \
)

/* QuickEdit mode is initially disabled, and the agent does not send mouse
 * mode sequences to the terminal.  If it receives mouse input, though, it
 * still writes MOUSE_EVENT_RECORD values into CONIN. */
#define VT7PTY_MOUSE_MODE_NONE          0

/* QuickEdit mode is initially enabled.  As CONIN enters or leaves mouse
 * input mode (i.e. where ENABLE_MOUSE_INPUT is on and ENABLE_QUICK_EDIT_MODE
 * is off), the agent enables or disables mouse input on the terminal.
 *
 * This is the default mode. */
#define VT7PTY_MOUSE_MODE_AUTO          1

/* QuickEdit mode is initially disabled, and the agent enables the terminal's
 * mouse input mode.  It does not disable terminal mouse mode (until exit). */
#define VT7PTY_MOUSE_MODE_FORCE         2



/*****************************************************************************
 * VT7Pty agent RPC call: process creation. */

/* If the spawn is marked "auto-shutdown", then the agent shuts down console
 * output once the process exits.  The agent stops polling for new console
 * output, and once all pending data has been written to the output pipe, the
 * agent closes the pipe.  (At that point, the pipe may still have data in it,
 * which the client may read.  Once all the data has been read, further reads
 * return EOF.) */
#define VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN 1ull

/* After the agent shuts down output, and after all output has been written
 * into the pipe(s), exit the agent by closing the console.  If there any
 * surviving processes still attached to the console, they are killed.
 *
 * Note: With this flag, an RPC call (e.g. vt7pty_set_size) issued after the
 * agent exits will fail with an I/O or dead-agent error. */
#define VT7PTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN 2ull

/* All the spawn flags. */
#define VT7PTY_SPAWN_FLAG_MASK (0ull \
    | VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN \
    | VT7PTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN \
)



#endif /* VT7PTY_CONSTANTS_H */
