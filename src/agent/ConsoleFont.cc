// Copyright (c) 2015 Ryan Prichard
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

#include "ConsoleFont.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <algorithm>
#include <string>
#include <tuple>

#include "../shared/DebugClient.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsVersion.h"
#include "../shared/WinptyAssert.h"
#include "../shared/StringFormatting.h"

namespace {

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

// See https://en.wikipedia.org/wiki/List_of_CJK_fonts
const wchar_t kLucidaConsole[] = L"Lucida Console";
const wchar_t kMSGothic[] = { 0xff2d, 0xff33, 0x0020, 0x30b4, 0x30b7, 0x30c3, 0x30af, 0 }; // 932, Japanese
const wchar_t kNSimSun[] = { 0x65b0, 0x5b8b, 0x4f53, 0 }; // 936, Chinese Simplified
const wchar_t kGulimChe[] = { 0xad74, 0xb9bc, 0xccb4, 0 }; // 949, Korean
const wchar_t kMingLight[] = { 0x7d30, 0x660e, 0x9ad4, 0 }; // 950, Chinese Traditional

struct FontSize {
    short size;
    int width;
};

struct Font {
    const wchar_t *faceName;
    unsigned int family;
    short size;
};

// Ideographs in East Asian languages take two columns rather than one.
// In the console screen buffer, a "full-width" character will occupy two
// cells of the buffer, the first with attribute 0x100 and the second with
// attribute 0x200.
//
// Windows does not correctly identify code points as double-width in all
// configurations.  It depends heavily on the code page, the font facename,
// and (somehow) even the font size.  In the 437 code page (MS-DOS), for
// example, no codepoints are interpreted as double-width.  When the console
// is in an East Asian code page (932, 936, 949, or 950), then sometimes
// selecting a "Western" facename like "Lucida Console" or "Consolas" doesn't
// register, or if the font *can* be chosen, then the console doesn't handle
// double-width correctly.  I tested the double-width handling by writing
// several code points with WriteConsole and checking whether one or two cells
// were filled.
//
// In the Japanese code page (932), Microsoft's default font is MS Gothic.
// MS Gothic double-width handling seems to be broken with console versions
// prior to Windows 10 (including Windows 10's legacy mode), and it's
// especially broken in Windows 8 and 8.1.
//
// Test with: fixture-utf16-echo A2 A3 2014 3044 30FC 4000
//
// The first three codepoints are always rendered as half-width with the
// Windows Japanese fonts.  (Of these, the first two must be half-width,
// but U+2014 could be either.)  The last three are rendered as full-width,
// and they are East_Asian_Width=Wide.
//
// Windows 7 fails by modeling all codepoints as full-width with font
// sizes 22 and above.
//
// Windows 8 gets U+00A2, U+00A3, U+2014, U+30FC, and U+4000 wrong, but
// using a point size not listed in the console properties dialog
// (e.g. "9") is less wrong:
//
//             |        code point               |
//  font       | 00A2 00A3 2014 3044 30FC 4000   | cell size
// ------------+---------------------------------+----------
//  8          |  F    F    F    F    H    H     |   4x8
//  9          |  F    F    F    F    F    F     |   5x9
//  16         |  F    F    F    F    H    H     |   8x16
// raster 6x13 |  H    H    H    F    F    H(*)  |   6x13
//
// (*) The Raster Font renders U+4000 as a white box (i.e. an unsupported
// character).
//

// See:
//  - docs/historical/Font-Report-June2016 for per-size details
//  - docs/historical/ConsoleFontNotes.txt
//  - tests/fixtures/Utf16Echo.cc and the font probes under tests/manual

const FontSize kLucidaFontSizes[] = {
    { 5, 3 },
    { 6, 4 },
    { 8, 5 },
    { 10, 6 },
    { 12, 7 },
    { 14, 8 },
    { 16, 10 },
    { 18, 11 },
    { 20, 12 },
    { 36, 22 },
    { 48, 29 },
    { 60, 36 },
    { 72, 43 },
};

// Japanese. Used on Windows 7.
const FontSize k932GothicWin7[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 13, 7 },
    { 15, 8 },
    { 17, 9 },
    { 19, 10 },
    { 21, 11 },
    // All larger fonts are more broken w.r.t. full-size East Asian characters.
};

// Japanese.  Used on Windows 8, 8.1, and the legacy 10 console.
const FontSize k932GothicWin8[] = {
    // All of these characters are broken w.r.t. full-size East Asian
    // characters, but they're equally broken.
    { 5, 3 },
    { 7, 4 },
    { 9, 5 },
    { 11, 6 },
    { 13, 7 },
    { 15, 8 },
    { 17, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Japanese.  Used on the new Windows 10 console.
const FontSize k932GothicWin10[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Chinese Simplified.
const FontSize k936SimSun[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Korean.
const FontSize k949GulimChe[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Chinese Traditional.
const FontSize k950MingLight[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

static std::string stringToCodePoints(const std::wstring &str) {
    std::string ret = "(";
    for (size_t i = 0; i < str.size(); ++i) {
        char tmp[32];
        formatString(tmp, "%X", str[i]);
        if (ret.size() > 1) {
            ret.push_back(' ');
        }
        ret += tmp;
    }
    ret.push_back(')');
    return ret;
}

static void dumpFontInfoEx(
        const CONSOLE_FONT_INFOEX &infoex,
        const char *prefix) {
    if (!isTracingEnabled()) {
        return;
    }
    std::wstring faceName(infoex.FaceName,
        wcsnlen(infoex.FaceName, COUNT_OF(infoex.FaceName)));
    trace("%snFont=%u dwFontSize=(%d,%d) "
        "FontFamily=0x%x FontWeight=%u FaceName=%s %s",
        prefix,
        static_cast<unsigned>(infoex.nFont),
        infoex.dwFontSize.X, infoex.dwFontSize.Y,
        infoex.FontFamily, infoex.FontWeight, utf8FromWide(faceName).c_str(),
        stringToCodePoints(faceName).c_str());
}

static void dumpCurrentFont(HANDLE conout, const char *prefix) {
    if (!isTracingEnabled()) {
        return;
    }
    CONSOLE_FONT_INFOEX infoex = {};
    infoex.cbSize = sizeof(infoex);
    if (!GetCurrentConsoleFontEx(conout, FALSE, &infoex)) {
        trace("GetCurrentConsoleFontEx call failed");
        return;
    }
    dumpFontInfoEx(infoex, prefix);
}

static bool setConsoleFont(
        HANDLE conout,
        const Font &font) {
    CONSOLE_FONT_INFOEX infoex = {};
    infoex.cbSize = sizeof(infoex);
    infoex.dwFontSize.Y = font.size;
    infoex.FontFamily = font.family;
    infoex.FontWeight = 400;
    winpty_wcsncpy_nul(infoex.FaceName, font.faceName);
    dumpFontInfoEx(infoex, "setConsoleFont: setting font to: ");
    if (!SetCurrentConsoleFontEx(conout, FALSE, &infoex)) {
        trace("setConsoleFont: SetCurrentConsoleFontEx call failed");
        return false;
    }
    memset(&infoex, 0, sizeof(infoex));
    infoex.cbSize = sizeof(infoex);
    if (!GetCurrentConsoleFontEx(conout, FALSE, &infoex)) {
        trace("setConsoleFont: GetCurrentConsoleFontEx call failed");
        return false;
    }
    if (wcsncmp(infoex.FaceName, font.faceName,
            COUNT_OF(infoex.FaceName)) != 0) {
        trace("setConsoleFont: face name was not set");
        dumpFontInfoEx(infoex, "setConsoleFont: post-call font: ");
        return false;
    }
    // We'd like to verify that the new font size is correct, but we can't
    // predict what it will be, even though we just set it to `pxSize` through
    // an apprently symmetric interface.  For the Chinese and Korean fonts, the
    // new `infoex.dwFontSize.Y` value can be slightly larger than the height
    // we specified.
    return true;
}

static Font selectSmallFont(int codePage, int columns, bool isNewW10) {
    // Iterate over a set of font sizes according to the code page, and select
    // one.

    const wchar_t *faceName = nullptr;
    unsigned int fontFamily = 0;
    const FontSize *table = nullptr;
    size_t tableSize = 0;

    switch (codePage) {
        case 932: // Japanese
            faceName = kMSGothic;
            fontFamily = 0x36;
            if (isNewW10) {
                table = k932GothicWin10;
                tableSize = COUNT_OF(k932GothicWin10);
            } else if (isWindows8OrGreater()) {
                table = k932GothicWin8;
                tableSize = COUNT_OF(k932GothicWin8);
            } else {
                table = k932GothicWin7;
                tableSize = COUNT_OF(k932GothicWin7);
            }
            break;
        case 936: // Chinese Simplified
            faceName = kNSimSun;
            fontFamily = 0x36;
            table = k936SimSun;
            tableSize = COUNT_OF(k936SimSun);
            break;
        case 949: // Korean
            faceName = kGulimChe;
            fontFamily = 0x36;
            table = k949GulimChe;
            tableSize = COUNT_OF(k949GulimChe);
            break;
        case 950: // Chinese Traditional
            faceName = kMingLight;
            fontFamily = 0x36;
            table = k950MingLight;
            tableSize = COUNT_OF(k950MingLight);
            break;
        default:
            faceName = kLucidaConsole;
            fontFamily = 0x36;
            table = kLucidaFontSizes;
            tableSize = COUNT_OF(kLucidaFontSizes);
            break;
    }

    size_t bestIndex = static_cast<size_t>(-1);
    std::tuple<int, int> bestScore = std::make_tuple(-1, -1);

    // We might want to pick the smallest possible font, because we don't know
    // how large the monitor is (and the monitor size can change).  We might
    // want to pick a larger font to accommodate console programs that resize
    // the console on their own, like DOS edit.com, which tends to resize the
    // console to 80 columns.

    for (size_t i = 0; i < tableSize; ++i) {
        const int width = table[i].width * columns;

        // In general, we'd like to pick a font size where cutting the number
        // of columns in half doesn't immediately violate the minimum width
        // constraint.  (e.g. To run DOS edit.com, a user might resize their
        // terminal to ~100 columns so it's big enough to show the 80 columns
        // post-resize.)  To achieve this, give priority to fonts that allow
        // this halving.  We don't want to encourage *very* large fonts,
        // though, so disable the effect as the number of columns scales from
        // 80 to 40.
        const int halfColumns = std::min(columns, std::max(40, columns / 2));
        const int halfWidth = table[i].width * halfColumns;

        std::tuple<int, int> thisScore = std::make_tuple(-1, -1);
        if (width >= 160 && halfWidth >= 160) {
            // Both sizes are good.  Prefer the smaller fonts.
            thisScore = std::make_tuple(2, -width);
        } else if (width >= 160) {
            // Prefer the smaller fonts.
            thisScore = std::make_tuple(1, -width);
        } else {
            // Otherwise, prefer the largest font in our table.
            thisScore = std::make_tuple(0, width);
        }
        if (thisScore > bestScore) {
            bestIndex = i;
            bestScore = thisScore;
        }
    }

    ASSERT(bestIndex != static_cast<size_t>(-1));
    return Font { faceName, fontFamily, table[bestIndex].size };
}

static void setSmallConsoleFont(
        HANDLE conout, int columns, bool isNewW10) {
    int codePage = GetConsoleOutputCP();
    const auto font = selectSmallFont(codePage, columns, isNewW10);
    if (setConsoleFont(conout, font)) {
        trace("setSmallConsoleFont: success");
        return;
    }
    if (codePage == 932 || codePage == 936 ||
            codePage == 949 || codePage == 950) {
        trace("setSmallConsoleFont: falling back to default codepage font instead");
        const auto fontFB = selectSmallFont(0, columns, isNewW10);
        if (setConsoleFont(conout, fontFB)) {
            trace("setSmallConsoleFont: fallback was successful");
            return;
        }
    }
    trace("setSmallConsoleFont: failure");
}

} // anonymous namespace

// A Windows console window can never be larger than the desktop window.  To
// maximize the possible size of the console in rows*cols, try to configure
// the console with a small font.  Unfortunately, we cannot make the font *too*
// small, because there is also a minimum window size in pixels.
void setSmallFont(HANDLE conout, int columns, bool isNewW10) {
    trace("setSmallFont: attempting to set a small font for %d columns "
        "(CP=%u OutputCP=%u)",
        columns,
        static_cast<unsigned>(GetConsoleCP()),
        static_cast<unsigned>(GetConsoleOutputCP()));
    dumpCurrentFont(conout, "previous font: ");
    setSmallConsoleFont(conout, columns, isNewW10);
    dumpCurrentFont(conout, "new font: ");
}
