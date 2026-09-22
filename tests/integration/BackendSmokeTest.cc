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

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

#include "../../src/include/vt7pty.h"
#include "../../src/shared/DebugClient.h"

static std::vector<unsigned char> filterContent(
        const std::vector<unsigned char> &content) {
    std::vector<unsigned char> result;
    auto it = content.begin();
    const auto itEnd = content.end();
    while (it < itEnd) {
        if (*it == '\r') {
            // Filter out carriage returns.  Sometimes the output starts with
            // a single CR; other times, it has multiple CRs.
            it++;
        } else if (*it == '\x1b' && (it + 1) < itEnd && *(it + 1) == '[') {
            // Filter out escape sequences.  They have no interior letters and
            // end with a single letter.
            it += 2;
            while (it < itEnd && !isalpha(*it)) {
                it++;
            }
            it++;
        } else {
            // Let everything else through.
            result.push_back(*it);
            it++;
        }
    }
    return result;
}

// Read bytes from the non-overlapped file handle until the file is closed or
// until an I/O error occurs.
static std::vector<unsigned char> readAll(HANDLE handle) {
    unsigned char buf[1024];
    std::vector<unsigned char> result;
    while (true) {
        DWORD amount = 0;
        BOOL ret = ReadFile(handle, buf, sizeof(buf), &amount, nullptr);
        if (!ret || amount == 0) {
            break;
        }
        result.insert(result.end(), buf, buf + amount);
    }
    return result;
}

static void parentTest() {
    wchar_t program[1024];
    wchar_t cmdline[1024];
    GetModuleFileNameW(nullptr, program, 1024);

    const int commandLength = swprintf_s(
        cmdline, _countof(cmdline), L"\"%ls\" CHILD", program);
    assert(commandLength > 0);

    auto agentCfg = vt7pty_config_new(0, nullptr);
    assert(agentCfg != nullptr);
    auto pty = vt7pty_open(agentCfg, nullptr);
    assert(pty != nullptr);
    vt7pty_config_free(agentCfg);

    HANDLE conin = CreateFileW(
        vt7pty_conin_name(pty),
        GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE conout = CreateFileW(
        vt7pty_conout_name(pty),
        GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(conin != INVALID_HANDLE_VALUE);
    assert(conout != INVALID_HANDLE_VALUE);

    auto spawnCfg = vt7pty_spawn_config_new(
            VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN, program, cmdline,
            nullptr, nullptr, nullptr);
    assert(spawnCfg != nullptr);
    HANDLE process = nullptr;
    BOOL spawnSuccess = vt7pty_spawn(
        pty, spawnCfg, &process, nullptr, nullptr, nullptr);
    assert(spawnSuccess && process != nullptr);

    auto content = readAll(conout);
    content = filterContent(content);

    std::vector<unsigned char> expectedContent = {
        'H', 'I', '\n', 'X', 'Y', '\n'
    };
    DWORD exitCode = 0;
    assert(GetExitCodeProcess(process, &exitCode) && exitCode == 42);
    CloseHandle(process);
    CloseHandle(conin);
    CloseHandle(conout);
    assert(content == expectedContent);
    vt7pty_free(pty);
}

static void childTest() {
    printf("HI\nXY\n");
    exit(42);
}

static std::wstring systemProgram(const wchar_t *relativePath) {
    wchar_t systemDirectory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    assert(length > 0 && length < MAX_PATH);
    return std::wstring(systemDirectory, length) + L"\\" + relativePath;
}

static bool containsText(
        const std::vector<unsigned char> &content,
        const char *expectedText) {
    const auto expectedBegin = reinterpret_cast<const unsigned char *>(expectedText);
    const auto expectedEnd = expectedBegin + strlen(expectedText);
    return std::search(
        content.begin(), content.end(), expectedBegin, expectedEnd) != content.end();
}

static void applicationTest(
        const std::wstring &program,
        const std::wstring &arguments,
        const char *expectedText) {
    const std::wstring commandLine = L"\"" + program + L"\" " + arguments;

    vt7pty_error_ptr_t error = nullptr;
    auto agentCfg = vt7pty_config_new(0, &error);
    assert(agentCfg != nullptr && error == nullptr);
    vt7pty_config_set_initial_size(agentCfg, 80, 25);
    auto pty = vt7pty_open(agentCfg, &error);
    vt7pty_config_free(agentCfg);
    assert(pty != nullptr && error == nullptr);

    HANDLE conin = CreateFileW(
        vt7pty_conin_name(pty),
        GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE conout = CreateFileW(
        vt7pty_conout_name(pty),
        GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(conin != INVALID_HANDLE_VALUE);
    assert(conout != INVALID_HANDLE_VALUE);
    assert(vt7pty_set_size(pty, 100, 30, &error) && error == nullptr);

    auto spawnCfg = vt7pty_spawn_config_new(
        VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN,
        program.c_str(), commandLine.c_str(), nullptr, nullptr, &error);
    assert(spawnCfg != nullptr && error == nullptr);
    HANDLE process = nullptr;
    BOOL spawnSuccess = vt7pty_spawn(
        pty, spawnCfg, &process, nullptr, nullptr, &error);
    vt7pty_spawn_config_free(spawnCfg);
    assert(spawnSuccess && process != nullptr && error == nullptr);

    auto content = filterContent(readAll(conout));
    DWORD exitCode = 0;
    assert(GetExitCodeProcess(process, &exitCode) && exitCode == 0);
    assert(containsText(content, expectedText));

    CloseHandle(process);
    CloseHandle(conin);
    CloseHandle(conout);
    vt7pty_free(pty);
}

static void applicationsTest() {
    applicationTest(
        systemProgram(L"cmd.exe"),
        L"/D /S /C \"echo VT7PTY_CMD_OK\"",
        "VT7PTY_CMD_OK");
    applicationTest(
        systemProgram(L"WindowsPowerShell\\v1.0\\powershell.exe"),
        L"-NoLogo -NoProfile -NonInteractive -Command "
        L"\"Write-Output 'VT7PTY_POWERSHELL_OK'\"",
        "VT7PTY_POWERSHELL_OK");
    printf("Command Prompt and Windows PowerShell sessions passed.\n");
}

static void expectedOpenFailure(
        vt7pty_result_t expectedCode,
        const wchar_t *expectedMessage) {
    vt7pty_error_ptr_t error = nullptr;
    auto agentCfg = vt7pty_config_new(0, &error);
    assert(agentCfg != nullptr && error == nullptr);
    auto pty = vt7pty_open(agentCfg, &error);
    vt7pty_config_free(agentCfg);

    assert(pty == nullptr);
    assert(error != nullptr);
    assert(vt7pty_error_code(error) == expectedCode);
    const wchar_t *message = vt7pty_error_msg(error);
    assert(message != nullptr && wcsstr(message, expectedMessage) != nullptr);
    wprintf(L"Expected VT7Pty failure: %ls\n", message);
    vt7pty_error_free(error);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        parentTest();
    } else if (argc == 2 && strcmp(argv[1], "APPLICATIONS") == 0) {
        applicationsTest();
    } else if (argc == 2 && strcmp(argv[1], "EXPECT_MISSING_AGENT") == 0) {
        expectedOpenFailure(
            VT7PTY_ERROR_AGENT_EXE_MISSING,
            L"VT7Pty agent executable is missing");
    } else if (argc == 2 &&
            strcmp(argv[1], "EXPECT_INCOMPATIBLE_AGENT") == 0) {
        expectedOpenFailure(
            VT7PTY_ERROR_AGENT_INCOMPATIBLE,
            L"VT7Pty agent");
    } else {
        childTest();
    }
    return 0;
}
