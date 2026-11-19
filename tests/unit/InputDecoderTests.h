// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "../../src/agent/CursorReportFilter.h"
#include "../../src/agent/UnicodeEncoding.h"
#include <cassert>
#include <string>
#include <vector>

inline void inputDecoderTests() {
    const std::string report = "\x1b[12;34R";
    // Every split of the reply, including bytewise delivery, preserves the
    // surrounding unfinished UTF-8 bytes instead of interpreting the reply.
    for (size_t split = 0; split <= report.size(); ++split) {
        CursorReportFilter filter; filter.request();
        std::string actual = filter.append("\xc4");
        actual += filter.append(report.substr(0, split));
        actual += filter.append(report.substr(split));
        actual += filter.append("\x8d" "z");
        assert(actual == "\xc4\x8d" "z" && !filter.outstanding() && !filter.pending());
    }
    CursorReportFilter bytewise; bytewise.request();
    std::string actual = bytewise.append("\x1b[");
    for (char byte : report) actual += bytewise.append(std::string(1, byte));
    actual += bytewise.append("D");
    assert(actual == "\x1b[D" && !bytewise.outstanding());
    for (const std::string ordinary : {std::string("\x1b[1;2R"), std::string("\x1b[D"),
            std::string("\x1b[?1;2R"), std::string("\x1b[12345678;1R"),
            std::string("\x1b[1;12345678R"), std::string("\x1b[;1R")}) {
        CursorReportFilter unsolicited;
        assert(unsolicited.append(ordinary) == ordinary);
        CursorReportFilter requested; requested.request();
        actual.clear();
        for (char byte : ordinary) actual += requested.append(std::string(1, byte));
        if (ordinary != "\x1b[1;2R") assert(actual == ordinary && requested.outstanding());
        else assert(actual.empty() && !requested.outstanding());
    }
    CursorReportFilter nested; nested.request();
    assert(nested.append("\x1b" + report) == "\x1b");
    CursorReportFilter timeout; timeout.request();
    assert(timeout.append("\x1b[").empty());
    assert(timeout.expire() == "\x1b[" && timeout.outstanding() && !timeout.pending());
    assert(timeout.append(report).empty() && !timeout.outstanding());
    CursorReportFilter duplicate; duplicate.request();
    assert(duplicate.append(report + report) == report && !duplicate.outstanding());
    CursorReportFilter bounded; bounded.request();
    assert(bounded.append("\x1b[9999999;9999999R").empty() && !bounded.pending());
    for (const std::string invalid : {std::string("\xc2z"),std::string("\xe2\x82z"),
            std::string("\xf0\x9f" "z"),std::string("\xc0\xaf"),
            std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),
            std::string("\xf5\x80\x80\x80"),std::string("\x80")}) {
        assert(scanUtf8Prefix(invalid.data(), static_cast<int>(invalid.size())) == 0);
    }
    // All Unicode scalars, all valid byte prefixes, including the Unicode
    // boundaries that reject overlong encodings, surrogates and >U+10FFFF.
    for (uint32_t code = 0; code <= 0x10ffff; ++code) {
        char bytes[4]; const int length = encodeUtf8(bytes, code);
        if (!length) { assert(code >= 0xd800 && code <= 0xdfff); continue; }
        assert(scanUtf8Prefix(bytes, length) == length && decodeUtf8(bytes) == code);
        for (int prefix = 0; prefix < length; ++prefix)
            assert(scanUtf8Prefix(bytes, prefix) == -1);
    }
}
