// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
namespace h4gate
{
constexpr uint32_t signature = 0x34475456;
constexpr wchar_t prefix[] = L"VT7Pty-H4-TestGate-";
constexpr wchar_t initName[] = L"VT7Pty.H4.TestGate.Init.v1";
struct State
{
    uint32_t magic = signature, bytes = sizeof(State), mode = 0;
    DWORD controller = 0, host = 0, thread = 0, agent = 0;
    uint64_t controllerCreated = 0, hostCreated = 0, window = 0, enteredTick = 0, releasedTick = 0;
    volatile LONG initialized = 0, entered = 0, finished = 0, cMessages = 0, error = 0;
    DWORD actualHost = 0, actualThread = 0;
    BOOL beforeValid = FALSE, duringValid = FALSE, afterValid = FALSE;
    BYTE before[256] = {}, during[256] = {}, after[256] = {};
};
} // namespace h4gate
