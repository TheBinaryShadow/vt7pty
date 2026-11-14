// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "../shared/HostCancellationProtocol.h"
#include "../shared/OwnedHandle.h"
#include <string>
#include <windows.h>
class Win32Console;
class HostCancellation
{
    Win32Console &console_;
    OwnedHandle host_, thread_, pipe_, completion_;
    HMODULE module_ = nullptr;
    HHOOK hook_ = nullptr;
    ATOM atom_ = 0;
    vt7pty::internal::cancel::Request binding_;
    uint64_t sequence_ = 0;
    bool ready_ = false, disabled_ = false, closeQueued_ = false;
    void notify();
    vt7pty::internal::cancel::Response receive(DWORD timeout);
    vt7pty::internal::cancel::Response request(vt7pty::internal::cancel::Operation operation);
    void cleanup() noexcept;

  public:
    explicit HostCancellation(Win32Console &console);
    ~HostCancellation();
    HostCancellation(const HostCancellation &) = delete;
    HostCancellation &operator=(const HostCancellation &) = delete;
    void cancel();
    void close() noexcept;
    bool consoleCloseQueued() const noexcept
    {
        return closeQueued_;
    }
};
