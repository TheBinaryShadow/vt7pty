// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../src/include/vt7pty.h"

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::wstring fixturePath() {
    wchar_t path[MAX_PATH] = {};
    const DWORD count = GetModuleFileNameW(nullptr, path, MAX_PATH);
    require(count > 0 && count < MAX_PATH, "cannot find test executable");
    std::wstring result(path, count);
    result.resize(result.find_last_of(L"\\/") + 1);
    return result + L"SessionFixture.exe";
}

DWORD handleCount() {
    DWORD count = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0,
        "cannot count process handles");
    return count;
}

std::string normalize(const std::vector<unsigned char> &bytes) {
    std::string result;
    for (size_t index = 0; index < bytes.size(); ++index) {
        if (bytes[index] == '\r') {
            continue;
        }
        if (bytes[index] == '\x1b' && index + 1 < bytes.size() &&
                bytes[index + 1] == '[') {
            index += 2;
            while (index < bytes.size() &&
                    !(bytes[index] >= 'A' && bytes[index] <= 'Z') &&
                    !(bytes[index] >= 'a' && bytes[index] <= 'z')) {
                ++index;
            }
            continue;
        }
        result.push_back(static_cast<char>(bytes[index]));
    }
    return result;
}

void checkOutput(const std::string &output) {
    const size_t begin = output.find("BEGIN");
    const size_t alpha = output.find("ALPHA");
    const size_t omega = output.find("OMEGA");
    const size_t end = output.find("END");
    require(begin != std::string::npos && alpha != std::string::npos &&
        omega != std::string::npos && end != std::string::npos,
        "missing or truncated output marker");
    require(begin < alpha && alpha < omega && omega < end,
        "output markers were reordered");
}

struct Session {
    vt7pty_t *pty = nullptr;
    HANDLE input = INVALID_HANDLE_VALUE;
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE child = nullptr;

    Session() {
        vt7pty_error_ptr_t error = nullptr;
        vt7pty_config_t *config = vt7pty_config_new(0, &error);
        require(config != nullptr && error == nullptr, "config creation failed");
        vt7pty_config_set_initial_size(config, 100, 30);
        pty = vt7pty_open(config, &error);
        vt7pty_config_free(config);
        if (error != nullptr) {
            vt7pty_error_free(error);
        }
        require(pty != nullptr, "agent startup failed");
        input = CreateFileW(vt7pty_conin_name(pty), GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, 0, nullptr);
        output = CreateFileW(vt7pty_conout_name(pty), GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, 0, nullptr);
        require(input != INVALID_HANDLE_VALUE && output != INVALID_HANDLE_VALUE,
            "console pipe connection failed");
    }

    ~Session() {
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
        if (pty != nullptr) vt7pty_free(pty);
        if (child != nullptr) CloseHandle(child);
    }

    void spawn(const char *mode) {
        const std::wstring program = fixturePath();
        std::wstring wideMode;
        for (const char *ptr = mode; *ptr != 0; ++ptr) {
            wideMode.push_back(static_cast<wchar_t>(*ptr));
        }
        const std::wstring command = L"\"" + program + L"\" " + wideMode;
        vt7pty_error_ptr_t error = nullptr;
        vt7pty_spawn_config_t *config = vt7pty_spawn_config_new(
            VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN, program.c_str(), command.c_str(),
            nullptr, nullptr, &error);
        require(config != nullptr && error == nullptr, "spawn config failed");
        const BOOL started = vt7pty_spawn(pty, config, &child, nullptr,
            nullptr, &error);
        vt7pty_spawn_config_free(config);
        if (error != nullptr) vt7pty_error_free(error);
        require(started && child != nullptr, "fixture spawn failed");
    }

    std::string readOutput() {
        std::vector<unsigned char> bytes;
        unsigned char block[4096];
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(output, block, sizeof(block), &read, nullptr) ||
                    read == 0) {
                break;
            }
            bytes.insert(bytes.end(), block, block + read);
            require(bytes.size() <= 1024 * 1024, "output exceeded test bound");
        }
        return normalize(bytes);
    }

    DWORD exitCode() const {
        require(WaitForSingleObject(child, 5000) == WAIT_OBJECT_0,
            "fixture did not exit");
        DWORD code = 0;
        require(GetExitCodeProcess(child, &code) != 0,
            "cannot read fixture exit status");
        return code;
    }

    void resize(int columns, int rows) {
        vt7pty_error_ptr_t error = nullptr;
        const BOOL succeeded = vt7pty_set_size(pty, columns, rows, &error);
        if (error != nullptr) vt7pty_error_free(error);
        require(succeeded != 0, "resize failed");
    }

    void closeAndCheckAgent() {
        HANDLE agent = nullptr;
        require(DuplicateHandle(GetCurrentProcess(), vt7pty_agent_process(pty),
            GetCurrentProcess(), &agent, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0,
            "cannot duplicate agent handle");
        CloseHandle(input);
        input = INVALID_HANDLE_VALUE;
        CloseHandle(output);
        output = INVALID_HANDLE_VALUE;
        vt7pty_free(pty);
        pty = nullptr;
        const DWORD result = WaitForSingleObject(agent, 10000);
        CloseHandle(agent);
        require(result == WAIT_OBJECT_0, "agent remained after session close");
    }
};

void outputCase(const char *fixtureMode) {
    Session session;
    session.spawn(fixtureMode);
    const std::string output = session.readOutput();
    checkOutput(output);
    require(session.exitCode() == 37, "wrong fixture exit status");
    session.closeAndCheckAgent();
}

void unicodeCase() {
    Session session;
    session.spawn("UNICODE");
    const std::string output = session.readOutput();
    require(output.find("UNICODE:") != std::string::npos,
        "Unicode prefix missing");
    require(output.find("\xe4\xb8\xad\xe6\x96\x87") != std::string::npos,
        "CJK output was not preserved as UTF-8");
    require(session.exitCode() == 0, "Unicode fixture failed");
}

void inputCase() {
    Session session;
    session.spawn("INPUT");
    const char input[] = "ping\r";
    DWORD written = 0;
    require(WriteFile(session.input, input, sizeof(input) - 1, &written,
        nullptr) != 0 && written == sizeof(input) - 1,
        "input write failed");
    require(session.readOutput().find("INPUT_OK") != std::string::npos,
        "child did not receive input");
    require(session.exitCode() == 0, "input fixture failed");
}

void resizeCase() {
    Session session;
    session.spawn("LOAD");
    for (int index = 0; index < 100; ++index) {
        session.resize(index % 2 == 0 ? 80 : 120,
            index % 2 == 0 ? 25 : 40);
        Sleep(5);
    }
    const std::string output = session.readOutput();
    require(output.find("ROW:000") != std::string::npos,
        "load output beginning missing");
    require(output.find("ROW:199") != std::string::npos &&
        output.find("LOAD_END") != std::string::npos,
        "load output drain incomplete");
    require(session.exitCode() == 0, "load fixture failed");
}

void repeatCase() {
    for (int index = 0; index < 5; ++index) {
        outputCase("OUTPUT");
    }
    const DWORD initialHandles = handleCount();
    for (int index = 0; index < 500; ++index) {
        outputCase("OUTPUT");
    }
    const DWORD finalHandles = handleCount();
    if (finalHandles > initialHandles + 4) {
        fprintf(stderr, "Session handles: before=%lu after=%lu\n",
            initialHandles, finalHandles);
    }
    require(finalHandles <= initialHandles + 4,
        "client process handle count grew after repeated sessions");
}

void soakCase(int minutes) {
    require(minutes >= 1 && minutes <= 120,
        "soak duration must be between 1 and 120 minutes");
    for (int index = 0; index < 5; ++index) outputCase("OUTPUT");
    const DWORD initialHandles = handleCount();
    const ULONGLONG deadline = GetTickCount64() +
        static_cast<ULONGLONG>(minutes) * 60000;
    int cycles = 0;
    do {
        Session session;
        session.spawn("IDLE");
        Sleep(5000);
        session.resize(80, 25);
        session.resize(120, 40);
        session.closeAndCheckAgent();
        require(WaitForSingleObject(session.child, 10000) == WAIT_OBJECT_0,
            "soak left child running");
        outputCase("OUTPUT");
        require(handleCount() <= initialHandles + 4,
            "soak client handles grew");
        ++cycles;
    } while (GetTickCount64() < deadline);
    printf("Soak cycles: %d\n", cycles);
}

void shutdownOnce() {
    Session session;
    session.spawn("WAIT");
    const DWORD childPid = GetProcessId(session.child);
    bool childAttached = false;
    for (int attempt = 0; attempt < 50 && !childAttached; ++attempt) {
        int processIds[16] = {};
        vt7pty_error_ptr_t listError = nullptr;
        const int processCount = vt7pty_get_console_process_list(session.pty,
            processIds, 16, &listError);
        if (listError != nullptr) vt7pty_error_free(listError);
        childAttached = processCount > 0 && processCount <= 16 &&
            std::find(processIds, processIds + processCount,
                static_cast<int>(childPid)) != processIds + processCount;
        if (!childAttached) Sleep(100);
    }
    require(childAttached,
        "child absent from console process list");
    HANDLE agent = nullptr;
    require(DuplicateHandle(GetCurrentProcess(), vt7pty_agent_process(session.pty),
        GetCurrentProcess(), &agent, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0,
        "cannot duplicate agent handle");
    CloseHandle(session.input);
    session.input = INVALID_HANDLE_VALUE;
    CloseHandle(session.output);
    session.output = INVALID_HANDLE_VALUE;
    vt7pty_free(session.pty);
    session.pty = nullptr;
    const DWORD agentResult = WaitForSingleObject(agent, 10000);
    const DWORD childResult = WaitForSingleObject(session.child, 10000);
    CloseHandle(agent);
    if (agentResult != WAIT_OBJECT_0 || childResult != WAIT_OBJECT_0) {
        fprintf(stderr, "Shutdown waits: agent=%lu child=%lu\n",
            agentResult, childResult);
    }
    require(agentResult == WAIT_OBJECT_0 && childResult == WAIT_OBJECT_0,
        "shutdown left agent or child running");
}

void shutdownCase() {
    for (int index = 0; index < 20; ++index) {
        shutdownOnce();
    }
}

void spawnFailureCase() {
    Session session;
    const wchar_t missing[] = L"Z:\\VT7Pty-missing-fixture.exe";
    vt7pty_error_ptr_t error = nullptr;
    vt7pty_spawn_config_t *config = vt7pty_spawn_config_new(
        VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN, missing, missing,
        nullptr, nullptr, &error);
    require(config != nullptr && error == nullptr,
        "cannot create missing-process spawn config");
    HANDLE child = nullptr;
    DWORD win32Error = 0;
    const BOOL started = vt7pty_spawn(session.pty, config, &child,
        nullptr, &win32Error, &error);
    vt7pty_spawn_config_free(config);
    const bool correctFailure = !started && child == nullptr &&
        error != nullptr &&
        vt7pty_error_code(error) == VT7PTY_ERROR_SPAWN_CREATE_PROCESS_FAILED &&
        (win32Error == ERROR_FILE_NOT_FOUND ||
            win32Error == ERROR_PATH_NOT_FOUND);
    if (!correctFailure) {
        fprintf(stderr, "Spawn failure observed: started=%d child=%p api=%lu win32=%lu\n",
            started, child,
            error != nullptr ? vt7pty_error_code(error) : 0, win32Error);
    }
    if (error != nullptr) vt7pty_error_free(error);
    require(correctFailure, "missing child did not report CreateProcess failure");
}

void concurrentCase() {
    std::vector<std::unique_ptr<Session>> sessions;
    for (int index = 0; index < 4; ++index) {
        auto session = std::make_unique<Session>();
        session->spawn("OUTPUT");
        sessions.push_back(std::move(session));
    }
    for (const auto &session : sessions) {
        checkOutput(session->readOutput());
        require(session->exitCode() == 37,
            "concurrent fixture exit status incorrect");
    }
}

void leakControl() {
    const DWORD before = handleCount();
    HANDLE leak = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (leak == nullptr) {
        throw std::runtime_error("cannot create leak control handle");
    }
    const DWORD after = handleCount();
    CloseHandle(leak);
    require(after <= before, "deliberate handle leak detected");
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3 || (argc == 3 &&
            strcmp(argv[1], "SOAK") != 0)) {
        fputs("Usage: SessionContractTest CASE\n", stderr);
        return 2;
    }
    try {
        if (strcmp(argv[1], "OUTPUT") == 0) outputCase("OUTPUT");
        else if (strcmp(argv[1], "UNICODE") == 0) unicodeCase();
        else if (strcmp(argv[1], "INPUT") == 0) inputCase();
        else if (strcmp(argv[1], "RESIZE") == 0) resizeCase();
        else if (strcmp(argv[1], "REPEAT") == 0) repeatCase();
        else if (strcmp(argv[1], "SHUTDOWN") == 0) shutdownCase();
        else if (strcmp(argv[1], "SPAWN_FAILURE") == 0)
            spawnFailureCase();
        else if (strcmp(argv[1], "CONCURRENT") == 0) concurrentCase();
        else if (strcmp(argv[1], "SOAK") == 0) {
            if (argc != 3) return 2;
            soakCase(atoi(argv[2]));
        }
        else if (strcmp(argv[1], "NEGATIVE_OUTPUT") == 0)
            outputCase("BROKEN_OUTPUT");
        else if (strcmp(argv[1], "NEGATIVE_STATUS") == 0)
            outputCase("BROKEN_EXIT");
        else if (strcmp(argv[1], "NEGATIVE_ORDER") == 0)
            outputCase("BROKEN_ORDER");
        else if (strcmp(argv[1], "NEGATIVE_TRUNCATION") == 0)
            outputCase("BROKEN_TRUNCATED");
        else if (strcmp(argv[1], "NEGATIVE_LEAK") == 0)
            leakControl();
        else if (strcmp(argv[1], "NEGATIVE_TIMEOUT") == 0) {
            Session session;
            session.spawn("WAIT");
            session.readOutput();
            throw std::runtime_error("timeout control unexpectedly completed");
        }
        else return 2;
        printf("Session contract %s passed.\n", argv[1]);
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "Session contract %s failed: %s\n", argv[1],
            error.what());
        return 1;
    }
}
