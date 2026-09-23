// Copyright (c) 2011-2016 Ryan Prichard
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

#include "Buffer.h"

#include <stdint.h>

#include "DebugClient.h"
#include "Assert.h"
#include "Narrow.h"

[[noreturn]] static void throwDecodeError(const char *condition) {
    TRACE_EVENT(TraceSeverity::Warning, TraceSubsystem::Ipc,
        "RPC decode error: %s", condition);
    throw ReadBuffer::DecodeError();
}

#define READ_BUFFER_CHECK(cond) \
    ((cond) ? static_cast<void>(0) : throwDecodeError(#cond))

enum class Piece : uint8_t { Int32, Int64, WString };

void WriteBuffer::putRawData(std::span<const std::byte> data) {
    const auto first = reinterpret_cast<const char *>(data.data());
    m_buf.insert(m_buf.end(), first, first + data.size());
}

void WriteBuffer::replaceRawData(
        size_t pos, std::span<const std::byte> data) {
    ASSERT(pos <= m_buf.size() && data.size() <= m_buf.size() - pos);
    const auto first = reinterpret_cast<const char *>(data.data());
    std::copy(first, first + data.size(), m_buf.begin() + pos);
}

void WriteBuffer::putInt32(int32_t i) {
    putRawValue(Piece::Int32);
    putRawValue(i);
}

void WriteBuffer::putInt64(int64_t i) {
    putRawValue(Piece::Int64);
    putRawValue(i);
}

// len is in characters, excluding NUL, i.e. the number of wchar_t elements
void WriteBuffer::putWString(const wchar_t *str, size_t len) {
    putRawValue(Piece::WString);
    putRawValue(static_cast<uint64_t>(len));
    putRawData(std::as_bytes(std::span { str, len }));
}

void ReadBuffer::getRawData(std::span<std::byte> data) {
    ASSERT(m_off <= m_buf.size());
    READ_BUFFER_CHECK(data.size() <= m_buf.size() - m_off);
    const auto input = std::as_bytes(std::span {
        m_buf.data() + m_off, data.size() });
    std::copy(input.begin(), input.end(), data.begin());
    m_off += data.size();
}

int32_t ReadBuffer::getInt32() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::Int32);
    return getRawValue<int32_t>();
}

int64_t ReadBuffer::getInt64() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::Int64);
    return getRawValue<int64_t>();
}

std::wstring ReadBuffer::getWString() {
    READ_BUFFER_CHECK(getRawValue<Piece>() == Piece::WString);
    const uint64_t charLen = getRawValue<uint64_t>();
    READ_BUFFER_CHECK(charLen <= SIZE_MAX / sizeof(wchar_t));
    // To be strictly conforming, we can't use the convenient wstring
    // constructor, because the string in m_buf mightn't be aligned.
    std::wstring ret;
    if (charLen > 0) {
        ret.resize(vt7pty::internal::checkedNarrow<size_t>(charLen));
        getRawData(std::as_writable_bytes(std::span { ret.data(), ret.size() }));
    }
    return ret;
}

void ReadBuffer::assertEof() {
    READ_BUFFER_CHECK(m_off == m_buf.size());
}
