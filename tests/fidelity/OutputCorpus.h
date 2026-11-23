// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

namespace fidelity {

enum class OutputAction { WideText, ByteText, Position, Erase, Wrap, Scroll, RawVT };

struct OutputCase {
    std::wstring_view name;
    OutputAction action;
    std::wstring_view wide{};
    std::string_view bytes{};
    uint32_t codePage = 0;
    std::u32string_view text{};
    int width = 0;

    bool isText() const {
        return action == OutputAction::WideText || action == OutputAction::ByteText;
    }
};

inline constexpr OutputCase outputCases[] = {
    {L"OUTPUT_LATIN", OutputAction::WideText, L"\u010d\u0107\u017e\u0161\u0111|",
        {}, 0, U"\u010d\u0107\u017e\u0161\u0111|", 6},
    {L"OUTPUT_COMBINING", OutputAction::WideText, L"e\u0301|", {}, 0, U"e\u0301|", 2},
    {L"OUTPUT_WIDE", OutputAction::WideText, L"\u4e2d|", {}, 0, U"\u4e2d|", 3},
    {L"OUTPUT_SUPPLEMENTARY", OutputAction::WideText, L"\U0001f642|", {}, 0, U"\U0001f642|", 3},
    {L"OUTPUT_CP437", OutputAction::ByteText, {}, "\x80\x82|", 437, U"\u00c7\u00e9|", 3},
    {L"OUTPUT_CP1250", OutputAction::ByteText, {}, "\xe8\xe6\x9e\x9a\xf0|", 1250,
        U"\u010d\u0107\u017e\u0161\u0111|", 6},
    {L"OUTPUT_CP65001", OutputAction::ByteText, {}, "\xc4\x8d\xc4\x87\xc5\xbe\xc5\xa1\xc4\x91|", 65001,
        U"\u010d\u0107\u017e\u0161\u0111|", 6},
    {L"OUTPUT_CP65001_MALFORMED", OutputAction::ByteText, {}, "X\xc3(Y|", 65001},
    {L"OUTPUT_POSITION", OutputAction::Position},
    {L"OUTPUT_ERASE", OutputAction::Erase},
    {L"OUTPUT_WRAP", OutputAction::Wrap},
    {L"OUTPUT_SCROLL", OutputAction::Scroll},
    {L"OUTPUT_RAW_VT", OutputAction::RawVT, L"\x1b[9;4H\x1b[31;44mV\x1b[0m"},
};

inline const OutputCase *findOutputCase(std::wstring_view name) {
    for (const auto &entry : outputCases)
        if (entry.name == name) return &entry;
    return nullptr;
}

inline constexpr int outputColumns = 80;
inline constexpr int outputRows = 25;
inline constexpr int outputTextX = 2;
inline constexpr int outputTextY = 9;
inline constexpr int outputBarrierRow = 22;
inline constexpr uint32_t rawOutputMode = 0x0004;
inline constexpr std::wstring_view outputBarriers[] = {L"VT7PTY_PHASE1", L"VT7PTY_PHASE2"};

struct OutputOperation {
    uint32_t phase = 0;
    uint32_t requestedCP = 0;
    uint32_t requestedMode = 0;
    uint32_t setupSucceeded = 0;
    uint32_t setupError = 0;
    uint32_t writeAttempted = 0;
    uint32_t writeSucceeded = 0;
    uint32_t writeError = 0;
    uint32_t unitsRequested = 0;
    uint32_t unitsWritten = 0;
};

} // namespace fidelity
