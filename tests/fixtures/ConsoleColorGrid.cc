#include <windows.h>

#include <stdio.h>
#include <wchar.h>

namespace {

bool writeText(HANDLE output, const wchar_t *text) {
    const DWORD length = static_cast<DWORD>(wcslen(text));
    DWORD written = 0;
    return WriteConsoleW(output, text, length, &written, nullptr) &&
           written == length;
}

} // anonymous namespace

int main() {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO original = {};
    if (!GetConsoleScreenBufferInfo(output, &original)) {
        fprintf(stderr, "GetConsoleScreenBufferInfo failed (is stdout a console?)\n");
        return 1;
    }

    for (WORD background = 0; background < 16; ++background) {
        for (WORD foreground = 0; foreground < 16; ++foreground) {
            const WORD attributes = static_cast<WORD>((background << 4) | foreground);
            if (!SetConsoleTextAttribute(output, attributes) ||
                    !writeText(output, L"  ")) {
                SetConsoleTextAttribute(output, original.wAttributes);
                fprintf(stderr, "Console color write failed\n");
                return 1;
            }
        }
        SetConsoleTextAttribute(output, original.wAttributes);
        if (!writeText(output, L"\r\n")) {
            fprintf(stderr, "Console newline write failed\n");
            return 1;
        }
    }

    SetConsoleTextAttribute(output, original.wAttributes);
    return 0;
}
