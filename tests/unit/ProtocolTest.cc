// Copyright (c) 2026 VT7Pty contributors
// Licensed under the MIT License.

#include <assert.h>
#include <stdio.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../../src/include/vt7pty_version.h"
#include "../../src/shared/Buffer.h"
#include "../../src/shared/DebugClient.h"
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

static void testOversizedStringIsRejected() {
    std::vector<char> bytes(1 + sizeof(uint64_t));
    bytes[0] = 2; // Piece::WString on the private wire encoding.
    const uint64_t length = UINT64_MAX;
    std::memcpy(bytes.data() + 1, &length, sizeof(length));
    ReadBuffer reader(std::move(bytes));
    bool rejected = false;
    try {
        reader.getWString();
    } catch (const ReadBuffer::DecodeError &) {
        rejected = true;
    }
    assert(rejected);
}

static void testStructuredDiagnosticRecord() {
    const std::string record = makeTraceRecord(
        TraceSeverity::Warning,
        TraceSubsystem::Security,
        "line one\n\"line two\"");
    assert(record.size() < VT7PTY_MAX_TRACE_RECORD_BYTES);
    assert(record.find("\"timestamp\":") != std::string::npos);
    assert(record.find("\"severity\":\"warning\"") != std::string::npos);
    assert(record.find("\"subsystem\":\"security\"") != std::string::npos);
    assert(record.find("\"product\":\"VT7Pty\"") != std::string::npos);
    assert(record.find("\"version\":") != std::string::npos);
    assert(record.find("\"commit\":") != std::string::npos);
    assert(record.find("\"pid\":") != std::string::npos);
    assert(record.find("\"tid\":") != std::string::npos);
    assert(record.find("line one\\n\\\"line two\\\"") != std::string::npos);
    assert(record.find('\n') == std::string::npos);

    const std::string large(10000, 'x');
    const std::string bounded = makeTraceRecord(
        TraceSeverity::Debug, TraceSubsystem::Ipc, large.c_str());
    assert(bounded.size() < VT7PTY_MAX_TRACE_RECORD_BYTES);
    assert(bounded.ends_with("\"}"));
}

int main(int argc, char *argv[]) {
    static_assert(VT7PTY_API_VERSION_MAJOR == 1);
    static_assert(VT7PTY_API_VERSION_MINOR == 0);
    static_assert(VT7PTY_PROTOCOL_VERSION == 1);
    static_assert(VT7PTY_MAX_CONTROL_PACKET_BYTES == 1024 * 1024);

    if (argc == 2 && std::strcmp(argv[1], "EMIT_DIAGNOSTICS") == 0) {
        traceEvent(TraceSeverity::Info, TraceSubsystem::General,
            "diagnostic transport test 1");
        traceEvent(TraceSeverity::Warning, TraceSubsystem::Ipc,
            "diagnostic transport test 2");
        traceEvent(TraceSeverity::Error, TraceSubsystem::Security,
            "diagnostic transport test 3");
        puts("VT7Pty diagnostic transport records emitted");
        return 0;
    }

    testCompatibleHandshake();
    testMissingHandshake();
    testMalformedHandshake();
    testWrongIdentity();
    testOlderProtocol();
    testNewerProtocol();
    testOversizedStringIsRejected();
    testStructuredDiagnosticRecord();

    puts("VT7Pty protocol tests passed");
    return 0;
}
