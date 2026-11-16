// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "HostIdentity.h"
namespace vt7pty::internal::host
{
struct DesktopState
{
    std::wstring station, caller, input;
    DWORD error = ERROR_SUCCESS;
    bool eligible() const
    {
        return error == ERROR_SUCCESS && _wcsicmp(station.c_str(), L"WinSta0") == 0 &&
               !caller.empty() && _wcsicmp(caller.c_str(), input.c_str()) == 0;
    }
};
inline DesktopState desktopState()
{
    DesktopState state;
    state.station = objectName(GetProcessWindowStation());
    state.caller = objectName(GetThreadDesktop(GetCurrentThreadId()));
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input)
    {
        state.error = GetLastError();
        return state;
    }
    try { state.input = objectName(input); }
    catch (...) { CloseDesktop(input); throw; }
    CloseDesktop(input);
    return state;
}
} // namespace vt7pty::internal::host
