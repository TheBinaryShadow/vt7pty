/*
 * A Win32 program that reads raw console input with ReadFile and echos
 * it to stdout.
 */

#include <stdio.h>
#include <conio.h>
#include <windows.h>

int main()
{
    int count = 0;
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hStdIn, 0);

    while (true) {
        DWORD actual;
        char ch;
        if (!ReadFile(hStdIn, &ch, 1, &actual, NULL) || actual != 1) {
            return 1;
        }
        printf("%02x ", ch);
        if (++count == 50)
            break;
    }
    return 0;
}
