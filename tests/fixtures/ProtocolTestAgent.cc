// Copyright (c) 2026 VT7Pty contributors
// Licensed under the MIT License.

#include <windows.h>

#include <stdint.h>
#include <wchar.h>

#include <string>

#include "../../src/shared/Buffer.h"
#include "../../src/shared/Protocol.h"

static void writeAll(HANDLE pipe, const void *data, size_t size) {
    const char *cursor = static_cast<const char *>(data);
    while (size != 0) {
        DWORD written = 0;
        if (!WriteFile(pipe, cursor, static_cast<DWORD>(size), &written, nullptr) ||
                written == 0) {
            ExitProcess(3);
        }
        cursor += written;
        size -= written;
    }
}

static std::wstring testMode() {
    wchar_t value[64] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"VT7PTY_PROTOCOL_TEST_MODE", value, _countof(value));
    if (length == 0 || length >= _countof(value)) {
        return L"malformed";
    }
    return std::wstring(value, length);
}

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        return 2;
    }

    HANDLE pipe = CreateFileW(
        argv[1], GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return 3;
    }

    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0);
    const std::wstring mode = testMode();
    if (mode == L"malformed") {
        packet.putWString(VT7PTY_AGENT_IDENTITY);
    } else if (mode == L"wrong-identity") {
        packet.putWString(L"Not-VT7Pty-Agent");
        packet.putInt32(VT7PTY_PROTOCOL_VERSION);
    } else if (mode == L"older") {
        packet.putWString(VT7PTY_AGENT_IDENTITY);
        packet.putInt32(VT7PTY_PROTOCOL_VERSION - 1);
    } else if (mode == L"newer") {
        packet.putWString(VT7PTY_AGENT_IDENTITY);
        packet.putInt32(VT7PTY_PROTOCOL_VERSION + 1);
    } else {
        CloseHandle(pipe);
        return 4;
    }

    packet.replaceRawValue<uint64_t>(0, packet.buf().size());
    writeAll(pipe, packet.buf().data(), packet.buf().size());
    FlushFileBuffers(pipe);
    CloseHandle(pipe);
    return 0;
}
