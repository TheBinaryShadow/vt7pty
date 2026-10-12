// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "../fidelity/WinTest.h"
#include <algorithm>
#include <cwchar>
using namespace fidelity;
namespace {
volatile LONG controls=0;
BOOL WINAPI controlHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) { InterlockedIncrement(&controls); return TRUE; }
    return FALSE;
}
std::unique_ptr<Packet> observe(HANDLE output, const std::vector<Key> &keys) {
    auto storage=std::make_unique<Packet>();
    Packet &packet=*storage;
    CONSOLE_SCREEN_BUFFER_INFO info={}; CONSOLE_CURSOR_INFO cursor={};
    check(GetConsoleScreenBufferInfo(output,&info),"screen info");
    check(GetConsoleCursorInfo(output,&cursor),"cursor info");
    packet.columns=static_cast<uint32_t>(info.srWindow.Right-info.srWindow.Left+1);
    packet.rows=static_cast<uint32_t>(info.srWindow.Bottom-info.srWindow.Top+1);
    packet.bufferColumns=static_cast<uint32_t>(info.dwSize.X); packet.bufferRows=static_cast<uint32_t>(info.dwSize.Y);
    packet.windowLeft=static_cast<uint32_t>(info.srWindow.Left); packet.windowTop=static_cast<uint32_t>(info.srWindow.Top);
    packet.cursorX=static_cast<uint32_t>(info.dwCursorPosition.X-info.srWindow.Left);
    packet.cursorY=static_cast<uint32_t>(info.dwCursorPosition.Y-info.srWindow.Top);
    packet.cursorVisible=cursor.bVisible ? 1u : 0u;
    DWORD inputMode=0, outputMode=0;
    check(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&inputMode),"input mode");
    check(GetConsoleMode(output,&outputMode),"output mode");
    packet.inputMode=inputMode; packet.outputMode=outputMode;
    packet.inputCP=GetConsoleCP(); packet.outputCP=GetConsoleOutputCP();
    CONSOLE_FONT_INFOEX font={}; font.cbSize=sizeof(font);
    if (GetCurrentConsoleFontEx(output,FALSE,&font)) {
        packet.fontWidth=static_cast<uint32_t>(font.dwFontSize.X); packet.fontHeight=static_cast<uint32_t>(font.dwFontSize.Y);
        wcsncpy_s(packet.fontName,font.FaceName,_TRUNCATE);
    } else packet.fontError=GetLastError();
    packet.controlCount=static_cast<uint32_t>(InterlockedCompareExchange(&controls,0,0));
    packet.keyCount=static_cast<uint32_t>(keys.size());
    std::copy(keys.begin(),keys.end(),packet.keys);
    validate(packet);
    COORD size={static_cast<SHORT>(packet.columns),static_cast<SHORT>(packet.rows)};
    SMALL_RECT rectangle=info.srWindow;
    check(ReadConsoleOutputW(output,packet.cells,size,{0,0},&rectangle),"snapshot cells");
    check(rectangle.Left == info.srWindow.Left && rectangle.Top == info.srWindow.Top &&
        rectangle.Right == info.srWindow.Right && rectangle.Bottom == info.srWindow.Bottom,"snapshot rectangle");
    return storage;
}
void draw(HANDLE output, bool unicode, bool alternate=false) {
    CONSOLE_SCREEN_BUFFER_INFO info={}; check(GetConsoleScreenBufferInfo(output,&info),"draw geometry");
    const int columns=info.srWindow.Right-info.srWindow.Left+1, rows=info.srWindow.Bottom-info.srWindow.Top+1;
    check(columns >= 40 && columns <= 120 && rows >= 12 && rows <= 40,"draw bounds");
    std::vector<CHAR_INFO> cells(static_cast<size_t>(columns*rows));
    for (auto &cell : cells) { cell.Char.UnicodeChar=L' '; cell.Attributes=7; }
    auto line=[&](int x,int y,const wchar_t *text,WORD attribute) {
        while (*text) { auto &cell=cells[static_cast<size_t>(y*columns+x++)];
            cell.Char.UnicodeChar=*text++; cell.Attributes=attribute; }
    };
    line(2,1,alternate ? L"STATE_ALT" : L"STATE_READY",7);
    line(3,4,L"\u010d\u0107\u017e\u0161\u0111",7);
    line(5,6,L"COLOR",0x1e);
    SMALL_RECT rectangle=info.srWindow;
    check(WriteConsoleOutputW(output,cells.data(),{static_cast<SHORT>(columns),static_cast<SHORT>(rows)},{0,0},&rectangle),"draw cells");
    check(SetConsoleTextAttribute(output,7),"draw default attribute");
    if (unicode) {
        check(SetConsoleCursorPosition(output,{static_cast<SHORT>(info.srWindow.Left+2),static_cast<SHORT>(info.srWindow.Top+9)}),"Unicode cursor");
        const wchar_t text[]=L"U: \u0107 \u4e2d \U0001f642 e\u0301";
        DWORD written=0;
        check(WriteConsoleW(output,text,static_cast<DWORD>(wcslen(text)),&written,nullptr) &&
            written == wcslen(text),"Unicode write");
    } else check(SetConsoleCursorPosition(output,{static_cast<SHORT>(info.srWindow.Left+17),static_cast<SHORT>(info.srWindow.Top+7)}),"draw cursor");
    CONSOLE_CURSOR_INFO cursor={25,TRUE}; check(SetConsoleCursorInfo(output,&cursor),"visible cursor");
}
}
int wmain(int argc,wchar_t **argv) {
    if (argc != 4) return 2;
    try {
        const std::wstring mode=argv[1], pipeName=argv[2];
        const DWORD expectedServer=wcstoul(argv[3],nullptr,10);
        Handle pipe(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
        check(pipe.value != INVALID_HANDLE_VALUE,"fixture pipe");
        ULONG server=0; check(GetNamedPipeServerProcessId(pipe.value,&server) && server == expectedServer,"fixture server PID");
        SetConsoleTitleW(L"VT7Pty controlled fidelity fixture");
        const bool input=mode == L"INPUT" || mode == L"PROCESSED";
        const bool unicode=mode == L"UNICODE";
        check(input || unicode || mode == L"STATE","fixture mode");
        Handle output(CreateFileW(L"CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        check(output.value != INVALID_HANDLE_VALUE,"fixture output");
        Handle alternate;
        HANDLE active=output.value;
        std::vector<Key> keys;
        if (input) {
            check(SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),ENABLE_EXTENDED_FLAGS |
                (mode == L"PROCESSED" ? ENABLE_PROCESSED_INPUT : 0)),"fixture input mode");
            check(SetConsoleCtrlHandler(controlHandler,TRUE),"control handler");
            draw(active,false);
        } else draw(active,unicode);
        auto packetStorage=observe(active,keys);
        Packet &packet=*packetStorage;
        transfer(pipe.value,&packet,sizeof(packet),true);
        const ULONGLONG deadline=GetTickCount64()+30000;
        while (GetTickCount64() < deadline) {
            if (input) {
                DWORD count=0;
                check(GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE),&count),"input event count");
                if (count != 0) {
                    INPUT_RECORD records[64]={}; DWORD read=0;
                    check(ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE),records,std::min<DWORD>(count,64),&read),"input records");
                    for (DWORD index=0; index<read; ++index) {
                        const auto &r=records[index].Event.KeyEvent;
                        if (records[index].EventType != KEY_EVENT || !r.bKeyDown ||
                            r.wVirtualKeyCode == VK_CONTROL || r.wVirtualKeyCode == VK_SHIFT ||
                            r.wVirtualKeyCode == VK_MENU) continue;
                        check(keys.size() < 128,"input record bound");
                        keys.push_back({static_cast<uint32_t>(r.uChar.UnicodeChar),r.wVirtualKeyCode,r.dwControlKeyState,r.wRepeatCount});
                    }
                }
            }
            DWORD available=0; check(PeekNamedPipe(pipe.value,nullptr,0,nullptr,&available,nullptr),"fixture control peek");
            if (available != 0) {
                char command=0; transfer(pipe.value,&command,1,false);
                if (command == 'Q') {
                    if (alternate.value != INVALID_HANDLE_VALUE) { check(SetConsoleActiveScreenBuffer(output.value),"exit restore"); active=output.value; }
                    CONSOLE_SCREEN_BUFFER_INFO final={}; check(GetConsoleScreenBufferInfo(active,&final),"drain geometry");
                    check(SetConsoleCursorPosition(active,{static_cast<SHORT>(final.srWindow.Left+2),static_cast<SHORT>(final.srWindow.Top+10)}),"drain cursor");
                    const wchar_t marker[]=L"FINAL_DRAIN";
                    DWORD written=0; check(WriteConsoleW(active,marker,11,&written,nullptr) && written == 11,"final drain");
                    return 0;
                }
                if (command == 'A') {
                    check(!input && alternate.value == INVALID_HANDLE_VALUE,"alternate state");
                    auto current=observe(active,keys);
                    alternate.reset(CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CONSOLE_TEXTMODE_BUFFER,nullptr));
                    check(alternate.value != INVALID_HANDLE_VALUE,"alternate buffer");
                    const COORD size={static_cast<SHORT>(current->columns),static_cast<SHORT>(current->rows)};
                    check(SetConsoleScreenBufferSize(alternate.value,size),"alternate size");
                    const SMALL_RECT window={0,0,static_cast<SHORT>(size.X-1),static_cast<SHORT>(size.Y-1)};
                    check(SetConsoleWindowInfo(alternate.value,TRUE,&window),"alternate window");
                    check(SetConsoleActiveScreenBuffer(alternate.value),"activate alternate");
                    active=alternate.value; draw(active,false,true);
                } else if (command == 'M') {
                    check(alternate.value != INVALID_HANDLE_VALUE,"restore state");
                    check(SetConsoleActiveScreenBuffer(output.value),"restore primary"); active=output.value;
                } else if (command == 'R') { check(!input,"repaint input fixture"); draw(active,unicode); }
                else check(command == 'S',"fixture command");
                packet=*observe(active,keys); packet.bufferTag=active == output.value ? 1u : 2u;
                transfer(pipe.value,&packet,sizeof(packet),true);
            }
            Sleep(2);
        }
        throw std::runtime_error("fixture overall deadline");
    } catch (const std::exception &e) { fprintf(stderr,"%s\n",e.what()); return 101; }
}
