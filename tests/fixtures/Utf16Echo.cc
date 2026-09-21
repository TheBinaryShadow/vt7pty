#include <windows.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>
#include <string>

int main(int argc, char *argv[]) {
    system("cls");

    if (argc == 1) {
        printf("Usage: %s hhhh\n", argv[0]);
        return 0;
    }

    std::wstring dataToWrite;
    for (int i = 1; i < argc; ++i) {
        errno = 0;
        char *end = nullptr;
        const unsigned long value = strtoul(argv[i], &end, 16);
        if (errno != 0 || end == argv[i] || *end != '\0' || value > 0xffff) {
            fprintf(stderr, "Invalid UTF-16 code unit: %s\n", argv[i]);
            return 1;
        }
        dataToWrite.push_back(static_cast<wchar_t>(value));
    }

    assert(dataToWrite.size() <= MAXDWORD);
    const DWORD dataSize = static_cast<DWORD>(dataToWrite.size());

    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        dataToWrite.data(), dataSize, &actual, NULL);
    assert(ret && actual == dataSize);

    // Read it back.
    std::vector<CHAR_INFO> readBuffer(dataToWrite.size() * 2);
    assert(!readBuffer.empty() && readBuffer.size() <= SHRT_MAX);
    const SHORT readWidth = static_cast<SHORT>(readBuffer.size());
    COORD bufSize = {readWidth, 1};
    COORD bufCoord = {0, 0};
    SMALL_RECT topLeft = {0, 0, static_cast<SHORT>(readWidth - 1), 0};
    ret = ReadConsoleOutputW(
            GetStdHandle(STD_OUTPUT_HANDLE), readBuffer.data(),
            bufSize, bufCoord, &topLeft);
    assert(ret);

    printf("\n");
    for (size_t i = 0; i < readBuffer.size(); ++i) {
        printf("CHAR: %04x %04x\n",
            readBuffer[i].Char.UnicodeChar,
            readBuffer[i].Attributes);
    }
    return 0;
}
