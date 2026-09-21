/*
 * A Win32 program that reads raw console input with _getch and echoes
 * it to stdout.
 */

#include <stdio.h>
#include <conio.h>

int main()
{
    int count = 0;
    while (true) {
        int ch = _getch();
        printf("%02x ", ch);
        if (++count == 50)
            break;
    }
    return 0;
}
