// Copyright (c) 2011-2012 Ryan Prichard
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

#include "EventLoop.h"

#include <algorithm>

#include "NamedPipe.h"
#include "../shared/DebugClient.h"
#include "../shared/Assert.h"

EventLoop::~EventLoop() = default;

// Enter the event loop.  Runs until the I/O or timeout handler calls exit().
void EventLoop::run()
{
    std::vector<HANDLE> waitHandles;
    ULONGLONG lastTime = GetTickCount64();
    while (!m_exiting) {
        bool didSomething = false;

        // Attempt to make progress with the pipes.
        waitHandles.clear();
        for (const auto &pipe : m_pipes) {
            if (pipe->serviceIo(&waitHandles)) {
                onPipeIo(*pipe);
                didSomething = true;
            }
        }

        // Call the timeout if enough time has elapsed.
        if (m_pollInterval > 0) {
            const ULONGLONG elapsed = GetTickCount64() - lastTime;
            if (elapsed >= static_cast<ULONGLONG>(m_pollInterval)) {
                onPollTimeout();
                lastTime = GetTickCount64();
                didSomething = true;
            }
        }

        if (didSomething)
            continue;

        // If there's nothing to do, wait.
        DWORD timeout = INFINITE;
        if (m_pollInterval > 0) {
            const ULONGLONG elapsed = GetTickCount64() - lastTime;
            timeout = elapsed >= static_cast<ULONGLONG>(m_pollInterval)
                ? 0
                : static_cast<DWORD>(
                    static_cast<ULONGLONG>(m_pollInterval) - elapsed);
        }
        if (waitHandles.size() == 0) {
            ASSERT(timeout != INFINITE);
            if (timeout > 0)
                Sleep(timeout);
        } else {
            DWORD result = WaitForMultipleObjects(
                                                  static_cast<DWORD>(waitHandles.size()),
                                                  waitHandles.data(),
                                                  FALSE,
                                                  timeout);
            ASSERT(result != WAIT_FAILED);
        }
    }
}

NamedPipe &EventLoop::createNamedPipe()
{
    auto pipe = std::make_unique<NamedPipe>();
    auto &result = *pipe;
    m_pipes.push_back(std::move(pipe));
    return result;
}

void EventLoop::setPollInterval(int ms)
{
    m_pollInterval = ms;
}

void EventLoop::shutdown()
{
    m_exiting = true;
}
