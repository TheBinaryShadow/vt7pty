// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "WinTest.h"

namespace fidelity {

enum class ScreenExit { Explicit, Implicit, Resize };

struct ScreenCase {
    std::wstring_view name;
    bool cmd;
    ScreenExit exit;
};

inline constexpr ScreenCase screenCases[] = {
    {L"SCREEN_CMD_EXPLICIT", true, ScreenExit::Explicit},
    {L"SCREEN_CMD_EXIT", true, ScreenExit::Implicit},
    {L"SCREEN_PS_EXIT", false, ScreenExit::Implicit},
    {L"SCREEN_CMD_RESIZE", true, ScreenExit::Resize},
    {L"SCREEN_PS_RESIZE", false, ScreenExit::Resize},
};

inline const ScreenCase *findScreenCase(std::wstring_view name) {
    for (const auto &scenario : screenCases)
        if (scenario.name == name) return &scenario;
    return nullptr;
}

inline constexpr COORD screenInitialSize{80, 25};
inline constexpr COORD screenGrownSize{100, 30};
inline constexpr uint32_t screenResizePairs = 4;
inline constexpr uint32_t viewportResizePairs = 2;
inline constexpr DWORD screenExitCode = 37;
inline constexpr std::wstring_view screenPrimary = L"SCREEN_PRIMARY";
inline constexpr std::wstring_view screenAlternate = L"SCREEN_ALTERNATE";
inline constexpr std::wstring_view screenCorner = L"CORNER";

enum class PrimaryMarkerPlacement { Original, Reflowed, Unexpected };

// The fixture reads these dimensions from its own screen buffer.
struct ScreenActivity {
    uint32_t columns = 0, rows = 0, bufferColumns = 0, bufferRows = 0;
    uint32_t windowLeft = 0, windowTop = 0;
    uint32_t draws = 0, windowEvents = 0, eventColumns = 0, eventRows = 0;
};

inline bool screenTextAt(const Packet &packet, uint32_t x, uint32_t y, std::wstring_view text, WORD color) {
    if (y >= packet.rows || x > packet.columns || text.size() > packet.columns - x) return false;
    for (const wchar_t character : text) {
        const auto &cell = packet.cells[y * packet.columns + x++];
        if (cell.Char.UnicodeChar != character || cell.Attributes != color) return false;
    }
    return true;
}

inline PrimaryMarkerPlacement primaryMarkerPlacement(const Packet &packet, bool resized) {
    const bool original = screenTextAt(packet, 2, 1, screenPrimary, 0x1e);
    // On growth the full-width first row can join the next row.
    const bool reflowed = resized && screenTextAt(packet, screenInitialSize.X + 2u, 0, screenPrimary, 0x1e);
    if (original == reflowed) return PrimaryMarkerPlacement::Unexpected;
    return original ? PrimaryMarkerPlacement::Original : PrimaryMarkerPlacement::Reflowed;
}

} // namespace fidelity
