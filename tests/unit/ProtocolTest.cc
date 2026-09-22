// Copyright (c) 2026 VT7Pty contributors
// Licensed under the MIT License.

#include <assert.h>
#include <stdio.h>

#include <utility>
#include <vector>

#include "../../src/include/vt7pty_version.h"
#include "../../src/shared/Buffer.h"
#include "../../src/shared/Protocol.h"

static ReadBuffer readerFrom(WriteBuffer &writer) {
    return ReadBuffer(std::move(writer.buf()));
}

static void testCompatibleHandshake() {
    WriteBuffer writer;
    writeAgentHandshake(writer);
    auto reader = readerFrom(writer);
    const auto handshake = readAgentHandshake(reader);
    reader.assertEof();
    assert(classifyAgentHandshake(handshake) ==
        AgentHandshakeStatus::Compatible);
}

static void testMissingHandshake() {
    WriteBuffer writer;
    auto reader = readerFrom(writer);
    bool rejected = false;
    try {
        readAgentHandshake(reader);
    } catch (const ReadBuffer::DecodeError &) {
        rejected = true;
    }
    assert(rejected);
}

static void testMalformedHandshake() {
    WriteBuffer writer;
    writer.putWString(VT7PTY_AGENT_IDENTITY);
    auto reader = readerFrom(writer);
    bool rejected = false;
    try {
        readAgentHandshake(reader);
    } catch (const ReadBuffer::DecodeError &) {
        rejected = true;
    }
    assert(rejected);
}

static void testWrongIdentity() {
    WriteBuffer writer;
    writer.putWString(L"Not-VT7Pty-Agent");
    writer.putInt32(VT7PTY_PROTOCOL_VERSION);
    auto reader = readerFrom(writer);
    const auto handshake = readAgentHandshake(reader);
    assert(classifyAgentHandshake(handshake) ==
        AgentHandshakeStatus::WrongIdentity);
}

static void testOlderProtocol() {
    WriteBuffer writer;
    writer.putWString(VT7PTY_AGENT_IDENTITY);
    writer.putInt32(VT7PTY_PROTOCOL_VERSION - 1);
    auto reader = readerFrom(writer);
    const auto handshake = readAgentHandshake(reader);
    assert(classifyAgentHandshake(handshake) ==
        AgentHandshakeStatus::UnsupportedVersion);
}

static void testNewerProtocol() {
    WriteBuffer writer;
    writer.putWString(VT7PTY_AGENT_IDENTITY);
    writer.putInt32(VT7PTY_PROTOCOL_VERSION + 1);
    auto reader = readerFrom(writer);
    const auto handshake = readAgentHandshake(reader);
    assert(classifyAgentHandshake(handshake) ==
        AgentHandshakeStatus::UnsupportedVersion);
}

int main() {
    static_assert(VT7PTY_API_VERSION_MAJOR == 1);
    static_assert(VT7PTY_API_VERSION_MINOR == 0);
    static_assert(VT7PTY_PROTOCOL_VERSION == 1);

    testCompatibleHandshake();
    testMissingHandshake();
    testMalformedHandshake();
    testWrongIdentity();
    testOlderProtocol();
    testNewerProtocol();

    puts("VT7Pty protocol tests passed");
    return 0;
}
