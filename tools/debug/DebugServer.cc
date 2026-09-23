// Copyright (c) 2015 Ryan Prichard
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

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>

#include "GenVersion.h"
#include "../../src/include/vt7pty_version.h"
#include "../../src/shared/DebugClient.h"
#include "../../src/shared/Exception.h"
#include "../../src/shared/OwnedHandle.h"
#include "../../src/shared/Protocol.h"
#include "../../src/shared/WindowsSecurity.h"

namespace {

constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\VT7Pty-Debug-v1";
constexpr unsigned long long kDefaultMaximumFileBytes = 8ULL * 1024 * 1024;
constexpr unsigned long long kMaximumFileBytes = 64ULL * 1024 * 1024;

struct Options {
    std::wstring outputPath;
    unsigned long long maximumFileBytes = kDefaultMaximumFileBytes;
    unsigned long maximumMessages = 0;
    bool selfTest = false;
};

[[noreturn]] void usage(const wchar_t *program, int code) {
    std::fwprintf(code == 0 ? stdout : stderr,
        L"Usage: %ls [--output PATH] [--max-bytes BYTES] "
        L"[--max-messages COUNT] [--self-test] [--version]\n\n"
        L"Collects bounded JSON-line records from %ls. The pipe accepts only "
        L"the owner, LocalSystem, and administrators, and rejects remote clients.\n"
        L"The default file limit is %llu bytes; the maximum is %llu bytes.\n",
        program, kPipeName, kDefaultMaximumFileBytes, kMaximumFileBytes);
    std::exit(code);
}

unsigned long long parseUnsigned(
        const wchar_t *text, unsigned long long maximum, const wchar_t *name) {
    wchar_t *end = nullptr;
    errno = 0;
    const unsigned long long value = std::wcstoull(text, &end, 10);
    if (errno != 0 || end == text || *end != L'\0' || value > maximum) {
        std::fwprintf(stderr, L"error: invalid %ls value: %ls\n", name, text);
        std::exit(1);
    }
    return value;
}

OwnedHandle createServerPipe() {
    const auto descriptor = createPipeSecurityDescriptorOwnerFullControl();
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = descriptor.get();
    HANDLE pipe = CreateNamedPipeW(
        kPipeName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        static_cast<DWORD>(VT7PTY_MAX_TRACE_RECORD_BYTES),
        static_cast<DWORD>(VT7PTY_MAX_TRACE_RECORD_BYTES),
        10 * 1000,
        &attributes);
    if (pipe == INVALID_HANDLE_VALUE) {
        throwWindowsError(L"CreateNamedPipeW for diagnostics failed");
    }
    return OwnedHandle(pipe);
}

void validateServerPipeSecurity(HANDLE pipe) {
    const auto descriptor = getObjectSecurityDescriptor(pipe);
    const std::wstring text = sdToString(descriptor.get());
    if (text.find(L"(A;;") == std::wstring::npos ||
            text.find(L";;;WD)") != std::wstring::npos ||
            text.find(L";;;AN)") != std::wstring::npos) {
        throw Exception(L"diagnostic pipe security descriptor is too broad");
    }
}

} // anonymous namespace

int wmain(int argc, wchar_t *argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--output" && i + 1 < argc) {
            options.outputPath = argv[++i];
        } else if (argument == L"--max-bytes" && i + 1 < argc) {
            options.maximumFileBytes = parseUnsigned(
                argv[++i], kMaximumFileBytes, L"--max-bytes");
        } else if (argument == L"--max-messages" && i + 1 < argc) {
            options.maximumMessages = static_cast<unsigned long>(parseUnsigned(
                argv[++i], std::numeric_limits<unsigned long>::max(),
                L"--max-messages"));
        } else if (argument == L"--self-test") {
            options.selfTest = true;
        } else if (argument == L"--version") {
            std::printf("VT7Pty version %s\ncommit %s\nAPI version %d.%d\n"
                "protocol version %d\n",
                GenVersion_Version, GenVersion_Commit,
                VT7PTY_API_VERSION_MAJOR, VT7PTY_API_VERSION_MINOR,
                VT7PTY_PROTOCOL_VERSION);
            return 0;
        } else if (argument == L"-h" || argument == L"--help") {
            usage(argv[0], 0);
        } else {
            usage(argv[0], 1);
        }
    }

    try {
        OwnedHandle serverPipe = createServerPipe();
        validateServerPipeSecurity(serverPipe.get());
        if (options.selfTest) {
            std::puts("VT7Pty diagnostic security self-test passed.");
            return 0;
        }

        FILE *output = nullptr;
        if (!options.outputPath.empty() &&
                _wfopen_s(&output, options.outputPath.c_str(), L"wb") != 0) {
            std::fwprintf(stderr, L"error: cannot open output file: %ls\n",
                options.outputPath.c_str());
            return 1;
        }
        unsigned long long fileBytes = 0;
        bool limitReported = false;
        unsigned long messages = 0;
        char buffer[VT7PTY_MAX_TRACE_RECORD_BYTES + 1] = {};

        while (options.maximumMessages == 0 ||
                messages < options.maximumMessages) {
            const BOOL connected = ConnectNamedPipe(serverPipe.get(), nullptr);
            if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
                std::fprintf(stderr, "error: ConnectNamedPipe failed: %lu\n",
                    static_cast<unsigned long>(GetLastError()));
                if (output != nullptr) std::fclose(output);
                return 1;
            }

            DWORD bytesRead = 0;
            const BOOL read = ReadFile(
                serverPipe.get(), buffer,
                static_cast<DWORD>(VT7PTY_MAX_TRACE_RECORD_BYTES),
                &bytesRead, nullptr);
            const DWORD readError = read ? ERROR_SUCCESS : GetLastError();
            const bool invalidRecord = !read || bytesRead < 2 ||
                bytesRead >= VT7PTY_MAX_TRACE_RECORD_BYTES ||
                buffer[0] != '{' || buffer[bytesRead - 1] != '}' ||
                std::memchr(buffer, '\0', bytesRead) != nullptr ||
                std::memchr(buffer, '\r', bytesRead) != nullptr ||
                std::memchr(buffer, '\n', bytesRead) != nullptr;
            if (invalidRecord) {
                const char response[] = "REJECTED";
                DWORD written = 0;
                WriteFile(serverPipe.get(), response,
                    static_cast<DWORD>(sizeof(response) - 1), &written, nullptr);
                DisconnectNamedPipe(serverPipe.get());
                if (readError != ERROR_MORE_DATA) {
                    std::fprintf(stderr,
                        "warning: rejected diagnostic message (error %lu)\n",
                        static_cast<unsigned long>(readError));
                }
                continue;
            }

            buffer[bytesRead] = '\0';
            std::fwrite(buffer, 1, bytesRead, stdout);
            std::fputc('\n', stdout);
            std::fflush(stdout);

            if (output != nullptr) {
                const unsigned long long recordBytes =
                    static_cast<unsigned long long>(bytesRead) + 1;
                if (recordBytes <= options.maximumFileBytes - fileBytes) {
                    std::fwrite(buffer, 1, bytesRead, output);
                    std::fputc('\n', output);
                    std::fflush(output);
                    fileBytes += recordBytes;
                } else if (!limitReported) {
                    std::fprintf(stderr,
                        "warning: diagnostic file reached its %llu-byte limit\n",
                        options.maximumFileBytes);
                    limitReported = true;
                }
            }

            const char response[] = "OK";
            DWORD written = 0;
            WriteFile(serverPipe.get(), response,
                static_cast<DWORD>(sizeof(response) - 1), &written, nullptr);
            DisconnectNamedPipe(serverPipe.get());
            ++messages;
        }
        if (output != nullptr) std::fclose(output);
        return 0;
    } catch (const Exception &exception) {
        std::fwprintf(stderr, L"error: %ls\n", exception.what());
        return 1;
    }
}
