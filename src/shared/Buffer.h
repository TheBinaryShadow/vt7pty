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

#ifndef VT7PTY_SHARED_BUFFER_H
#define VT7PTY_SHARED_BUFFER_H

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>
#include <string>

#include "Exception.h"

class WriteBuffer {
private:
    std::vector<char> m_buf;

public:
    WriteBuffer() = default;

    template <typename T> void putRawValue(const T &t) {
        putRawData(std::as_bytes(std::span { &t, size_t { 1 } }));
    }
    template <typename T> void replaceRawValue(size_t pos, const T &t) {
        replaceRawData(
            pos, std::as_bytes(std::span { &t, size_t { 1 } }));
    }

    void putRawData(std::span<const std::byte> data);
    void replaceRawData(size_t pos, std::span<const std::byte> data);
    void putInt32(int32_t i);
    void putInt64(int64_t i);
    void putWString(const wchar_t *str, size_t len);
    void putWString(const wchar_t *str) {
        putWString(str, std::char_traits<wchar_t>::length(str));
    }
    void putWString(const std::wstring &str)    { putWString(str.data(), str.size()); }
    std::vector<char> &buf()                    { return m_buf; }

    WriteBuffer(WriteBuffer &&other) = default;
    WriteBuffer &operator=(WriteBuffer &&other) = default;
};

class ReadBuffer {
public:
    class DecodeError : public Exception {
    public:
        DecodeError() : Exception(L"DecodeError: RPC message decoding error") {}
    };

private:
    std::vector<char> m_buf;
    size_t m_off = 0;

public:
    explicit ReadBuffer(std::vector<char> &&buf) : m_buf(std::move(buf)) {}

    template <typename T> T getRawValue() {
        T ret = {};
        getRawData(std::as_writable_bytes(std::span { &ret, size_t { 1 } }));
        return ret;
    }

    void getRawData(std::span<std::byte> data);
    int32_t getInt32();
    int64_t getInt64();
    std::wstring getWString();
    void assertEof();

    ReadBuffer(ReadBuffer &&other) = default;
    ReadBuffer &operator=(ReadBuffer &&other) = default;
};

#endif // VT7PTY_SHARED_BUFFER_H
