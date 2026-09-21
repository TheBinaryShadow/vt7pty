// Copyright (c) 2011-2012 Ryan Prichard
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

#ifndef VT7PTY_SHARED_PROTOCOL_H
#define VT7PTY_SHARED_PROTOCOL_H

#include <stdint.h>
#include <string>

#include "Buffer.h"

constexpr wchar_t VT7PTY_AGENT_IDENTITY[] = L"VT7Pty-Agent";
constexpr int32_t VT7PTY_PROTOCOL_VERSION = 1;

struct AgentHandshake {
    std::wstring identity;
    int32_t protocolVersion;
};

enum class AgentHandshakeStatus {
    Compatible,
    WrongIdentity,
    UnsupportedVersion,
};

inline void writeAgentHandshake(WriteBuffer &packet) {
    packet.putWString(VT7PTY_AGENT_IDENTITY);
    packet.putInt32(VT7PTY_PROTOCOL_VERSION);
}

inline AgentHandshake readAgentHandshake(ReadBuffer &packet) {
    AgentHandshake result;
    result.identity = packet.getWString();
    result.protocolVersion = packet.getInt32();
    return result;
}

inline AgentHandshakeStatus classifyAgentHandshake(
        const AgentHandshake &handshake) {
    if (handshake.identity != VT7PTY_AGENT_IDENTITY) {
        return AgentHandshakeStatus::WrongIdentity;
    }
    if (handshake.protocolVersion != VT7PTY_PROTOCOL_VERSION) {
        return AgentHandshakeStatus::UnsupportedVersion;
    }
    return AgentHandshakeStatus::Compatible;
}

struct AgentMessage
{
    enum Type {
        StartProcess,
        SetSize,
        GetConsoleProcessList,
    };
};

enum class StartProcessResult {
    CreateProcessFailed,
    ProcessCreated,
};

#endif // VT7PTY_SHARED_PROTOCOL_H
