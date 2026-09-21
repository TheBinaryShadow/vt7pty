// Copyright (c) 2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "Version.h"

#include <stdio.h>
#include <string.h>

#include "DebugClient.h"

// The maintained MSBuild target generates this header below build/generated
// and adds that directory to the include path.
#include "GenVersion.h"
#include "../include/vt7pty_version.h"
#include "Protocol.h"

void dumpVersionToStdout() {
    printf("VT7Pty version %s\n", GenVersion_Version);
    printf("commit %s\n", GenVersion_Commit);
    printf("API version %d.%d\n",
        VT7PTY_API_VERSION_MAJOR,
        VT7PTY_API_VERSION_MINOR);
    printf("protocol version %d\n", VT7PTY_PROTOCOL_VERSION);
}

void dumpVersionToTrace() {
    trace("VT7Pty version %s (commit %s, API %d.%d, protocol %d)",
        GenVersion_Version,
        GenVersion_Commit,
        VT7PTY_API_VERSION_MAJOR,
        VT7PTY_API_VERSION_MINOR,
        VT7PTY_PROTOCOL_VERSION);
}
