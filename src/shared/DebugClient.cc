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

#include "DebugClient.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

#include "GenVersion.h"
#include "Narrow.h"
#include "Protocol.h"
#include "../include/vt7pty_version.h"

namespace {

constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\VT7Pty-Debug-v1";
constexpr DWORD kPipeWaitMilliseconds = 100;
constexpr size_t kFormattedMessageBytes = 1024;

class PreserveLastError {
public:
    PreserveLastError() : m_lastError(GetLastError()) {}
    ~PreserveLastError() { SetLastError(m_lastError); }
private:
    DWORD m_lastError;
};

const char *severityName(TraceSeverity severity) {
    switch (severity) {
    case TraceSeverity::Debug: return "debug";
    case TraceSeverity::Info: return "info";
    case TraceSeverity::Warning: return "warning";
    case TraceSeverity::Error: return "error";
    }
    return "unknown";
}

const char *subsystemName(TraceSubsystem subsystem) {
    switch (subsystem) {
    case TraceSubsystem::General: return "general";
    case TraceSubsystem::Api: return "api";
    case TraceSubsystem::Agent: return "agent";
    case TraceSubsystem::Console: return "console";
    case TraceSubsystem::Ipc: return "ipc";
    case TraceSubsystem::Platform: return "platform";
    case TraceSubsystem::Process: return "process";
    case TraceSubsystem::Security: return "security";
    }
    return "unknown";
}

std::string jsonEscape(const char *input, size_t maximumBytes) {
    std::string result;
    result.reserve(maximumBytes);
    for (const unsigned char *p =
            reinterpret_cast<const unsigned char *>(input);
            *p != '\0' && result.size() < maximumBytes; ++p) {
        const char *escape = nullptr;
        switch (*p) {
        case '\\': escape = "\\\\"; break;
        case '"': escape = "\\\""; break;
        case '\r': escape = "\\r"; break;
        case '\n': escape = "\\n"; break;
        case '\t': escape = "\\t"; break;
        default: break;
        }
        if (escape != nullptr) {
            if (result.size() + 2 > maximumBytes) break;
            result.append(escape);
        } else if (*p >= 0x20 && *p < 0x7f) {
            result.push_back(static_cast<char>(*p));
        } else {
            if (result.size() + 6 > maximumBytes) break;
            char encoded[7] = {};
            std::snprintf(encoded, sizeof(encoded), "\\u%04x", *p);
            result.append(encoded);
        }
    }
    return result;
}

std::string moduleBaseName() {
    char moduleName[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(
        nullptr, moduleName, static_cast<DWORD>(std::size(moduleName)));
    if (length == 0 || length >= std::size(moduleName)) return "unknown";
    const char *baseName = std::strrchr(moduleName, '\\');
    return baseName == nullptr ? moduleName : baseName + 1;
}

void sendToDebugServer(const char *message) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    HANDLE tracePipe = CreateFileW(
        kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (tracePipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY &&
            WaitNamedPipeW(kPipeName, kPipeWaitMilliseconds)) {
        tracePipe = CreateFileW(
            kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                FILE_FLAG_OVERLAPPED,
            nullptr);
    }
    if (tracePipe != INVALID_HANDLE_VALUE) {
        DWORD newMode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(tracePipe, &newMode, nullptr, nullptr);
        OVERLAPPED operation = {};
        operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (operation.hEvent != nullptr) {
            char response[16] = {};
            DWORD actual = 0;
            const BOOL complete = TransactNamedPipe(
                tracePipe, const_cast<char *>(message),
                vt7pty::internal::checkedNarrow<DWORD>(std::strlen(message)),
                response, sizeof(response), &actual, &operation);
            if (!complete && GetLastError() == ERROR_IO_PENDING &&
                    WaitForSingleObject(operation.hEvent,
                        kPipeWaitMilliseconds) != WAIT_OBJECT_0) {
                CancelIo(tracePipe);
                GetOverlappedResult(tracePipe, &operation, &actual, TRUE);
            }
            CloseHandle(operation.hEvent);
        }
        CloseHandle(tracePipe);
    }
}

const char *getDebugConfig() {
    static const std::string config = [] {
        PreserveLastError preserve;
        char buffer[256] = {};
        const DWORD actualSize = GetEnvironmentVariableA(
            "VT7PTY_DEBUG", buffer, static_cast<DWORD>(std::size(buffer)));
        if (actualSize == 0 || actualSize >= std::size(buffer)) {
            return std::string();
        }
        return std::string(buffer, actualSize);
    }();
    return config.c_str();
}

void traceV(TraceSeverity severity, TraceSubsystem subsystem,
        const char *format, va_list arguments) {
    if (!isTracingEnabled()) return;
    PreserveLastError preserve;
    char message[kFormattedMessageBytes] = {};
    const int count = std::vsnprintf(message, sizeof(message), format, arguments);
    if (count < 0 || static_cast<size_t>(count) >= sizeof(message)) {
        message[sizeof(message) - 1] = '\0';
    }
    const std::string record = makeTraceRecord(severity, subsystem, message);
    sendToDebugServer(record.c_str());
}

} // anonymous namespace

bool isTracingEnabled() {
    static const bool enabled = hasDebugFlag("trace") || hasDebugFlag("1");
    return enabled;
}

bool hasDebugFlag(const char *flag) {
    if (std::strchr(flag, ',') != nullptr) std::abort();
    const char *const configCStr = getDebugConfig();
    if (configCStr[0] == '\0') return false;
    PreserveLastError preserve;
    const std::string config = "," + std::string(configCStr) + ",";
    const std::string flagStr = "," + std::string(flag) + ",";
    return config.find(flagStr) != std::string::npos;
}

std::string makeTraceRecord(
        TraceSeverity severity, TraceSubsystem subsystem, const char *message) {
    SYSTEMTIME time = {};
    GetSystemTime(&time);
    char prefix[1536] = {};
    const std::string process = jsonEscape(moduleBaseName().c_str(), 256);
    std::snprintf(
        prefix, sizeof(prefix),
        "{\"timestamp\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\","
        "\"severity\":\"%s\",\"subsystem\":\"%s\","
        "\"product\":\"VT7Pty\",\"version\":\"%s\","
        "\"commit\":\"%s\",\"api\":\"%d.%d\",\"protocol\":%d,"
        "\"process\":\"%s\",\"pid\":%lu,\"tid\":%lu,\"message\":\"",
        static_cast<unsigned>(time.wYear), static_cast<unsigned>(time.wMonth),
        static_cast<unsigned>(time.wDay), static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute), static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds), severityName(severity),
        subsystemName(subsystem), GenVersion_Version, GenVersion_Commit,
        VT7PTY_API_VERSION_MAJOR, VT7PTY_API_VERSION_MINOR,
        VT7PTY_PROTOCOL_VERSION, process.c_str(),
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()));
    prefix[sizeof(prefix) - 1] = '\0';

    std::string result(prefix);
    constexpr size_t suffixBytes = 2;
    const size_t available = result.size() + suffixBytes <
            VT7PTY_MAX_TRACE_RECORD_BYTES - 1
        ? VT7PTY_MAX_TRACE_RECORD_BYTES - 1 - result.size() - suffixBytes
        : 0;
    result += jsonEscape(message == nullptr ? "" : message, available);
    result += "\"}";
    return result;
}

void traceEvent(TraceSeverity severity, TraceSubsystem subsystem,
        const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    traceV(severity, subsystem, format, arguments);
    va_end(arguments);
}

void trace(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    traceV(TraceSeverity::Debug, TraceSubsystem::General, format, arguments);
    va_end(arguments);
}
