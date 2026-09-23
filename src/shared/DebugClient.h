// Copyright (c) 2011-2012 Ryan Prichard
// Copyright (c) 2026 VT7Pty contributors
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

#ifndef DEBUGCLIENT_H
#define DEBUGCLIENT_H

#include <cstddef>
#include <string>

enum class TraceSeverity { Debug, Info, Warning, Error };

enum class TraceSubsystem {
    General, Api, Agent, Console, Ipc, Platform, Process, Security,
};

constexpr size_t VT7PTY_MAX_TRACE_RECORD_BYTES = 4096;

bool isTracingEnabled();
bool hasDebugFlag(const char *flag);
std::string makeTraceRecord(
    TraceSeverity severity, TraceSubsystem subsystem, const char *message);
void traceEvent(
    TraceSeverity severity, TraceSubsystem subsystem, const char *format, ...);
void trace(const char *format, ...);

#define TRACE_EVENT(severity, subsystem, format, ...)                 \
    do {                                                              \
        if (isTracingEnabled()) {                                     \
            traceEvent((severity), (subsystem), (format)              \
                __VA_OPT__(,) __VA_ARGS__);                           \
        }                                                             \
    } while (false)

#define TRACE(format, ...)                                            \
    TRACE_EVENT(TraceSeverity::Debug, TraceSubsystem::General,        \
        (format) __VA_OPT__(,) __VA_ARGS__)

#endif // DEBUGCLIENT_H
