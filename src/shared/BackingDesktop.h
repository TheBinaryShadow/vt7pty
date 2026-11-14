// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "GenRandom.h"
#include "HostIdentity.h"
#include <thread>
namespace vt7pty::internal
{
class BackingDesktop
{
    HDESK desktop_ = nullptr;
    std::wstring name_, application_;

  public:
    BackingDesktop()
    {
        host::noImpersonation();
        const host::Token token(GetCurrentProcess());
        if (token.integrity != SECURITY_MANDATORY_MEDIUM_RID)
            throwWindowsError(L"Host cancellation requires a medium-integrity interactive caller",
                              ERROR_ACCESS_DENIED);
        host::require(_wcsicmp(host::objectName(GetProcessWindowStation()).c_str(), L"WinSta0") ==
                          0,
                      L"Host cancellation window station");
        const auto original = host::objectName(GetThreadDesktop(GetCurrentThreadId()));
        HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        host::require(input != nullptr, L"Host cancellation input desktop");
        bool ordinary = false;
        try
        {
            ordinary = _wcsicmp(host::objectName(input).c_str(), original.c_str()) == 0;
        }
        catch (...)
        {
            CloseDesktop(input);
            throw;
        }
        CloseDesktop(input);
        host::require(ordinary, L"Host cancellation requires the ordinary caller desktop");
        application_ = L"WinSta0\\" + original;
        name_ = L"VT7Pty-Console-" + GenRandom().randomHexString(16);
        const auto descriptor = host::privateDescriptor();
        SECURITY_ATTRIBUTES sa = {sizeof(sa), descriptor.get(), FALSE};
        // CreateDesktop may assign its desktop to the calling thread. Keep
        // that operation off the consumer's UI/API thread entirely.
        DWORD desktopError = ERROR_SUCCESS;
        std::thread creator([&] {
            desktop_ =
                CreateDesktopW(name_.c_str(), nullptr, nullptr, 0,
                               DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_ENUMERATE |
                                   DESKTOP_HOOKCONTROL | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS,
                               &sa);
            desktopError = GetLastError();
        });
        creator.join();
        if (!desktop_)
            throwWindowsError(L"Private backing console desktop", desktopError);
    }
    ~BackingDesktop()
    {
        if (desktop_)
            CloseDesktop(desktop_);
    }
    BackingDesktop(const BackingDesktop &) = delete;
    BackingDesktop &operator=(const BackingDesktop &) = delete;
    std::wstring name() const
    {
        return L"WinSta0\\" + name_;
    }
    const std::wstring &application() const
    {
        return application_;
    }
};
} // namespace vt7pty::internal
