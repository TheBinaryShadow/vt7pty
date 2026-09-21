#include <windows.h>

#include <stdio.h>

#include "TestUtil.cc"

static void dumpCurrentFont(HANDLE conout, BOOL maximumWindow) {
    CONSOLE_FONT_INFOEX info = {};
    info.cbSize = sizeof(info);
    if (!GetCurrentConsoleFontEx(conout, maximumWindow, &info)) {
        cprintf(L"GetCurrentConsoleFontEx failed: %u\n", GetLastError());
        return;
    }

    cprintf(L"nFont=%u dwFontSize=(%d,%d) "
        L"FontFamily=0x%x FontWeight=%u FaceName=%ls\n",
        static_cast<unsigned>(info.nFont),
        info.dwFontSize.X, info.dwFontSize.Y,
        info.FontFamily, info.FontWeight, info.FaceName);
}

int main() {
    const HANDLE conout = openConout();
    const COORD largest = GetLargestConsoleWindowSize(conout);
    cprintf(L"largestConsoleWindowSize=(%d,%d)\n", largest.X, largest.Y);
    cprintf(L"maximumWindow=0: ");
    dumpCurrentFont(conout, FALSE);
    cprintf(L"maximumWindow=1: ");
    dumpCurrentFont(conout, TRUE);
    cprintf(L"CP=%u OutputCP=%u\n", GetConsoleCP(), GetConsoleOutputCP());
    return 0;
}
