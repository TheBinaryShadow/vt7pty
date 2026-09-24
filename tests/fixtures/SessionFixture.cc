// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 2;
    }
    if (strcmp(argv[1], "OUTPUT") == 0) {
        fputs("BEGIN\nALPHA\nOMEGA\nEND\n", stdout);
        fflush(stdout);
        return 37;
    }
    if (strcmp(argv[1], "UNICODE") == 0) {
        const wchar_t text[] = L"UNICODE: \u4e2d\u6587 \U0001f642\n";
        DWORD written = 0;
        if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text,
                static_cast<DWORD>(wcslen(text)), &written, nullptr)) {
            return 3;
        }
        return static_cast<size_t>(written) == wcslen(text) ? 0 : 4;
    }
    if (strcmp(argv[1], "INPUT") == 0) {
        wchar_t input[32] = {};
        DWORD count = 0;
        if (!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), input, 31,
                &count, nullptr)) {
            return 5;
        }
        if (count < 5 || wcsncmp(input, L"ping", 4) != 0) {
            return 6;
        }
        puts("INPUT_OK");
        return 0;
    }
    if (strcmp(argv[1], "LOAD") == 0) {
        for (int index = 0; index != 200; ++index) {
            printf("ROW:%03d\n", index);
            fflush(stdout);
            Sleep(2);
        }
        puts("LOAD_END");
        return 0;
    }
    if (strcmp(argv[1], "BROKEN_OUTPUT") == 0) {
        puts("BEGIN\nWRONG\nEND");
        return 37;
    }
    if (strcmp(argv[1], "BROKEN_EXIT") == 0) {
        puts("BEGIN\nALPHA\nOMEGA\nEND");
        return 38;
    }
    if (strcmp(argv[1], "BROKEN_ORDER") == 0) {
        puts("BEGIN\nOMEGA\nALPHA\nEND");
        return 37;
    }
    if (strcmp(argv[1], "BROKEN_TRUNCATED") == 0) {
        puts("BEGIN\nALPHA\nOMEGA");
        return 37;
    }
    if (strcmp(argv[1], "WAIT") == 0) {
        puts("WAIT_READY");
        fflush(stdout);
        Sleep(30000);
        return 0;
    }
    if (strcmp(argv[1], "IDLE") == 0) {
        puts("IDLE_READY");
        fflush(stdout);
        Sleep(120000);
        return 0;
    }
    return 2;
}
