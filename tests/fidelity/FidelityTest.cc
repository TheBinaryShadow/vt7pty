// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "WinTest.h"
#include "VtState.h"
#include "../../src/include/vt7pty.h"
#include <functional>
#include <fstream>
#include <filesystem>
#include <sstream>

using namespace fidelity;
namespace {
struct Mismatch : std::runtime_error { using std::runtime_error::runtime_error; };
std::string utf8(std::wstring_view text) {
    check(text.size() < 32768,"UTF-8 input size");
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    check(size > 0 || text.empty(),"UTF-8 conversion");
    std::string result(static_cast<size_t>(size),'\0');
    if (size) check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr) == size,"UTF-8 conversion");
    return result;
}
std::wstring psLiteral(std::wstring_view text) {
    std::wstring result=L"'";
    for (wchar_t c : text) { if (c == L'\'') result += L'\''; result += c; }
    return result+L'\'';
}
std::wstring systemProgram(std::wstring_view relative) {
    wchar_t path[MAX_PATH]={}; const UINT count=GetSystemDirectoryW(path,MAX_PATH);
    check(count > 0 && count < MAX_PATH,"system directory");
    return std::wstring(path,count)+L"\\"+std::wstring(relative);
}
class Session {
public:
    VtState state{80,25};
    std::string raw;
    uint64_t lineFeeds=0;
    bool dropOutput=false;
    Handle child, agent;
    Session() {
        vt7pty_error_ptr_t error=nullptr;
        auto *config=vt7pty_config_new(0,&error);
        check(config != nullptr,"session config");
        vt7pty_config_set_initial_size(config,80,25);
        vt7pty_config_set_agent_timeout(config,5000);
        pty_=vt7pty_open(config,&error); vt7pty_config_free(config);
        if (error) { const std::string message=utf8(vt7pty_error_msg(error)); vt7pty_error_free(error);
            if (pty_) { vt7pty_free(pty_); pty_=nullptr; }
            throw std::runtime_error("agent startup: "+message);
        }
        check(pty_ != nullptr,"agent startup");
        try {
            check(DuplicateHandle(GetCurrentProcess(),vt7pty_agent_process(pty_),GetCurrentProcess(),&agent.value,0,FALSE,DUPLICATE_SAME_ACCESS),"agent ownership");
            input_.reset(CreateFileW(vt7pty_conin_name(pty_),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            output_.reset(CreateFileW(vt7pty_conout_name(pty_),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            check(input_.value != INVALID_HANDLE_VALUE && output_.value != INVALID_HANDLE_VALUE,"session pipes");
        } catch (...) { vt7pty_free(pty_); pty_=nullptr; throw; }
    }
    ~Session() { cleanup(); }
    void spawn(const std::wstring &application,const std::wstring &command) {
        vt7pty_error_ptr_t error=nullptr;
        auto *config=vt7pty_spawn_config_new(VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN,
            application.c_str(),command.c_str(),nullptr,nullptr,&error);
        check(config != nullptr,"spawn config");
        DWORD win32=0;
        const BOOL result=vt7pty_spawn(pty_,config,&child.value,nullptr,&win32,&error);
        vt7pty_spawn_config_free(config);
        if (error) vt7pty_error_free(error);
        check(result != FALSE,"spawn");
    }
    void input(std::string_view text) {
        check(text.size() <= 4096,"input bound");
        transfer(input_.value,const_cast<char *>(text.data()),static_cast<DWORD>(text.size()),true);
    }
    void pump() {
        if (eof_) return;
        for (int iteration=0;iteration<64;++iteration) {
            DWORD available=0;
            if (!PeekNamedPipe(output_.value,nullptr,0,nullptr,&available,nullptr)) {
                check(GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_PIPE_NOT_CONNECTED,"output peek");
                eof_=true; if (!dropOutput) state.finish(); return;
            }
            if (available == 0) return;
            char bytes[4096]={};
            const DWORD length=std::min<DWORD>(available,sizeof(bytes));
            transfer(output_.value,bytes,length,false);
            check(raw.size()+length <= 1024*1024,"captured output bound");
            raw.append(bytes,length);
            lineFeeds+=static_cast<uint64_t>(std::count(bytes,bytes+length,'\n'));
            if (!dropOutput) state.feed(std::string_view(bytes,length));
            for (const auto &reply : state.takeReplies()) input(reply);
        }
    }
    bool until(const std::function<bool()> &condition,DWORD milliseconds=5000) {
        const ULONGLONG deadline=GetTickCount64()+milliseconds;
        do { pump(); if (condition()) return true; Sleep(2); } while (GetTickCount64()<deadline);
        pump(); return condition();
    }
    void resize(int columns,int rows) {
        check(vt7pty_set_size(pty_,columns,rows,nullptr) != FALSE,"resize");
        state.resize(columns,rows);
    }
    void finish() {
        if (!until([&] { return eof_ && WaitForSingleObject(child.value,0) == WAIT_OBJECT_0; },10000))
            throw Mismatch("final output/child completion deadline");
        DWORD status=0; check(GetExitCodeProcess(child.value,&status),"child status");
        if (status != 0) throw std::runtime_error("fixture/controlled shell exit "+std::to_string(status));
        if (!cleanup()) throw Mismatch("owned process survived shutdown");
    }
    bool cleanup() noexcept {
        if (closed_) return cleanupOkay_;
        closed_=true;
        input_.reset();
        if (pty_) { vt7pty_free(pty_); pty_=nullptr; }
        cleanupOkay_=true;
        for (HANDLE process : {child.value,agent.value}) {
            if (process == INVALID_HANDLE_VALUE || process == nullptr) continue;
            if (WaitForSingleObject(process,5000) != WAIT_OBJECT_0) {
                cleanupOkay_=false;
                TerminateProcess(process,125);
                WaitForSingleObject(process,5000);
            }
        }
        output_.reset();
        return cleanupOkay_;
    }
private:
    vt7pty_t *pty_=nullptr;
    Handle input_,output_;
    bool eof_=false,closed_=false,cleanupOkay_=true;
};
uint16_t visualAttribute(WORD value) {
    uint16_t result=static_cast<uint16_t>(value & 0x80ff);
    if (value & COMMON_LVB_REVERSE_VIDEO)
        result=static_cast<uint16_t>((result & 0x8000) | ((result & 15)<<4) | ((result & 240)>>4));
    return result;
}
bool sameScreen(const VtState &state,const Packet &packet) {
    if (packet.columns != static_cast<uint32_t>(state.columns()) || packet.rows != static_cast<uint32_t>(state.rows()) ||
        packet.cursorX != static_cast<uint32_t>(state.x()) || packet.cursorY != static_cast<uint32_t>(state.y()) ||
        (packet.cursorVisible != 0) != state.visible()) return false;
    for (uint32_t y=0;y<packet.rows;++y) for (uint32_t x=0;x<packet.columns;++x) {
        const auto &observed=state.at(static_cast<int>(x),static_cast<int>(y));
        const auto &expected=packet.cells[y*packet.columns+x];
        if (observed.text != std::u32string(1,static_cast<char32_t>(expected.Char.UnicodeChar)) ||
            observed.attributes != visualAttribute(expected.Attributes) || observed.continuation) return false;
    }
    return true;
}
std::u32string snapshotText(const Packet &packet,uint32_t y) {
    std::u32string result;
    for (uint32_t x=0;x<packet.columns;++x) {
        const auto &cell=packet.cells[y*packet.columns+x];
        if (cell.Attributes & COMMON_LVB_TRAILING_BYTE) continue;
        const uint32_t c=cell.Char.UnicodeChar;
        if (c >= 0xd800 && c <= 0xdbff && x+1<packet.columns) {
            uint32_t next=x+1;
            while (next<packet.columns && (packet.cells[y*packet.columns+next].Attributes & COMMON_LVB_TRAILING_BYTE)) ++next;
            if (next<packet.columns) {
                const uint32_t low=packet.cells[y*packet.columns+next].Char.UnicodeChar;
                if (low>=0xdc00 && low<=0xdfff) {
                    result.push_back(static_cast<char32_t>(0x10000+((c-0xd800)<<10)+(low-0xdc00))); x=next; continue;
                }
            }
        }
        result.push_back(static_cast<char32_t>(c));
    }
    while (!result.empty() && result.back() == U' ') result.pop_back();
    return result;
}
std::u32string trimmed(std::u32string text) {
    while (!text.empty() && text.back() == U' ') text.pop_back(); return text;
}
void recipe(const Packet &p) {
    validate(p);
    if (p.columns<40 || p.rows<12) throw std::runtime_error("fixture geometry too small");
    const std::wstring label=p.bufferTag == 2 ? L"STATE_ALT" : L"STATE_READY";
    for (size_t i=0;i<label.size();++i)
        check(p.cells[p.columns+2+i].Char.UnicodeChar == label[i],"fixture label oracle");
    const wchar_t latin[]=L"\u010d\u0107\u017e\u0161\u0111";
    for (size_t i=0;i<5;++i) check(p.cells[4*p.columns+3+i].Char.UnicodeChar == latin[i],"fixture Latin oracle");
    for (size_t i=0;i<5;++i) check(p.cells[6*p.columns+5+i].Attributes == 0x1e,"fixture attribute oracle");
}
std::string packetJson(const Packet &p) {
    return "{\"Columns\":"+std::to_string(p.columns)+",\"Rows\":"+std::to_string(p.rows)+
        ",\"BufferColumns\":"+std::to_string(p.bufferColumns)+",\"BufferRows\":"+std::to_string(p.bufferRows)+
        ",\"WindowLeft\":"+std::to_string(p.windowLeft)+",\"WindowTop\":"+std::to_string(p.windowTop)+
        ",\"CursorX\":"+std::to_string(p.cursorX)+",\"CursorY\":"+std::to_string(p.cursorY)+
        ",\"InputMode\":"+std::to_string(p.inputMode)+",\"OutputMode\":"+std::to_string(p.outputMode)+
        ",\"InputCP\":"+std::to_string(p.inputCP)+",\"OutputCP\":"+std::to_string(p.outputCP)+
        ",\"FontName\":"+json(utf8(std::wstring_view(p.fontName,wcsnlen_s(p.fontName,LF_FACESIZE))))+
        ",\"FontWidth\":"+std::to_string(p.fontWidth)+",\"FontHeight\":"+std::to_string(p.fontHeight)+",\"FontError\":"+std::to_string(p.fontError)+
        ",\"ControlCount\":"+std::to_string(p.controlCount)+",\"KeyCount\":"+std::to_string(p.keyCount)+
        ",\"BufferTag\":"+std::to_string(p.bufferTag)+"}";
}
std::string differenceJson(const VtState &state,const Packet &packet) {
    uint32_t text=0,attribute=0;
    std::string first="null";
    if (packet.columns <= 120 && packet.rows <= 40) {
        for (uint32_t y=0;y<std::min(packet.rows,static_cast<uint32_t>(state.rows()));++y)
            for (uint32_t x=0;x<std::min(packet.columns,static_cast<uint32_t>(state.columns()));++x) {
                const auto &cell=state.at(static_cast<int>(x),static_cast<int>(y));
                const auto &expected=packet.cells[y*packet.columns+x];
                const bool wrongText=cell.text != std::u32string(1,static_cast<char32_t>(expected.Char.UnicodeChar)) || cell.continuation;
                const bool wrongAttribute=cell.attributes != visualAttribute(expected.Attributes);
                text+=wrongText ? 1u : 0u; attribute+=wrongAttribute ? 1u : 0u;
                if ((wrongText || wrongAttribute) && first == "null")
                    first="{\"X\":"+std::to_string(x)+",\"Y\":"+std::to_string(y)+
                        ",\"ExpectedUtf16\":"+std::to_string(expected.Char.UnicodeChar)+
                        ",\"ObservedScalar\":"+std::to_string(cell.text.empty() ? 0u : static_cast<uint32_t>(cell.text[0]))+
                        ",\"ExpectedAttribute\":"+std::to_string(visualAttribute(expected.Attributes))+
                        ",\"ObservedAttribute\":"+std::to_string(cell.attributes)+"}";
            }
    }
    return "{\"CursorX\":"+std::to_string(state.x())+",\"CursorY\":"+std::to_string(state.y())+
        ",\"Columns\":"+std::to_string(state.columns())+",\"Rows\":"+std::to_string(state.rows())+
        ",\"RawUtf16CellDifferences\":"+std::to_string(text)+",\"AttributeDifferences\":"+std::to_string(attribute)+
        ",\"FirstCellDifference\":"+first+"}";
}
struct Observations {
    std::string fields=""; std::string raw;
    std::string console="null", comparison="null";
    bool cleanup=true;
    void add(const std::string &key,bool value) { fields+=","+json(key)+":"+(value ? "true" : "false"); }
};
void fixtureCase(const std::wstring &name,const std::wstring &resultDirectory,Observations &observations) {
    Session session;
    auto packetStorage=std::make_unique<Packet>();
    Packet &packet=*packetStorage;
    try {
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Fidelity."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        Handle pipe(privateServer(pipeName));
        std::wstring mode=L"STATE";
        if (name == L"UNICODE") mode=L"UNICODE";
        if (name == L"INPUT" || name == L"UNPROCESSED_CTRL_C" || name == L"NEGATIVE_DUPLICATE") mode=L"INPUT";
        if (name == L"PROCESSED_CTRL_C") mode=L"PROCESSED";
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        session.spawn(fixture,quote(fixture)+L" "+mode+L" "+quote(pipeName)+L" "+std::to_wstring(GetCurrentProcessId()));
        connect(pipe.value);
        ULONG peer=0; check(GetNamedPipeClientProcessId(pipe.value,&peer) && peer == GetProcessId(session.child.value),"fixture client PID");
        transfer(pipe.value,&packet,sizeof(packet),false); recipe(packet);
        observations.console=packetJson(packet);
        writeFile(resultDirectory+L"\\console-snapshot.bin",std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
        auto snapshot=[&](char command='S') {
            transfer(pipe.value,&command,1,true);
            transfer(pipe.value,&packet,sizeof(packet),false); validate(packet);
            observations.console=packetJson(packet);
            writeFile(resultDirectory+L"\\console-snapshot.bin",std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
        };
        const ULONGLONG begin=GetTickCount64();
        if (mode == L"INPUT" || mode == L"PROCESSED") {
            if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("input fixture initial screen mismatch");
            if (name == L"PROCESSED_CTRL_C" || name == L"UNPROCESSED_CTRL_C") session.input("\x03z");
            else {
                const std::string input="A\xc4\x87\x1b[D\x7f";
                for (char c : input) session.input(std::string_view(&c,1));
            }
            const uint32_t expected=name == L"PROCESSED_CTRL_C" ? 1u :
                (name == L"UNPROCESSED_CTRL_C" ? 2u : 4u);
            if (!session.until([&] { snapshot(); return packet.keyCount >= expected &&
                (name != L"PROCESSED_CTRL_C" || packet.controlCount >= 1); })) throw Mismatch("input delivery deadline");
            if (name == L"NEGATIVE_DUPLICATE") { check(packet.keyCount < 128,"duplicate fault count bound"); packet.keys[packet.keyCount++]=packet.keys[0]; }
            bool correct=packet.keyCount == expected;
            for (uint32_t index=0;index<packet.keyCount;++index) correct=correct && packet.keys[index].repeat == 1;
            if (name == L"PROCESSED_CTRL_C")
                correct=correct && packet.controlCount == 1 && packet.keys[0].character == 'z';
            else if (name == L"UNPROCESSED_CTRL_C")
                correct=correct && packet.controlCount == 0 && packet.keys[0].character == 3 &&
                    (packet.keys[0].modifiers & LEFT_CTRL_PRESSED) && packet.keys[1].character == 'z';
            else correct=correct && packet.keys[0].character == 'A' && packet.keys[1].character == 0x107 &&
                packet.keys[2].virtualKey == VK_LEFT && packet.keys[3].virtualKey == VK_BACK;
            observations.add("InputMatches",correct);
            if (!correct) throw Mismatch("input/control count or record mismatch");
        } else if (name == L"UNICODE") {
            const auto expected=snapshotText(packet,9);
            session.until([&] { return trimmed(session.state.row(9)) == expected &&
                packet.cursorX == static_cast<uint32_t>(session.state.x()) &&
                packet.cursorY == static_cast<uint32_t>(session.state.y()); });
            const bool text=trimmed(session.state.row(9)) == expected;
            const bool cursor=packet.cursorX == static_cast<uint32_t>(session.state.x()) &&
                packet.cursorY == static_cast<uint32_t>(session.state.y());
            observations.add("UnicodeTextMatches",text); observations.add("UnicodeCursorMatches",cursor);
            if (!text || !cursor) throw Mismatch("Unicode text/cursor width differs from backing console");
        } else {
            if (name == L"NEGATIVE_PACKET") { packet.magic=0; validate(packet); }
            if (name == L"NEGATIVE_CELL") packet.cells[packet.columns+2].Char.UnicodeChar=L'!';
            if (name == L"NEGATIVE_ATTRIBUTE") packet.cells[6*packet.columns+5].Attributes=7;
            if (name == L"NEGATIVE_CURSOR") ++packet.cursorX;
            if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("screen/cursor/attribute mismatch");
            if (name == L"ALTERNATE") {
                snapshot('A'); recipe(packet);
                if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("alternate screen mismatch");
                snapshot('M'); recipe(packet);
                if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("restored primary mismatch");
            }
            if (name == L"RESIZE") {
                for (const auto size : {COORD{100,30},COORD{80,25},COORD{100,30},COORD{80,25}}) {
                    session.resize(size.X,size.Y); snapshot('R'); recipe(packet);
                    check(packet.columns == static_cast<uint32_t>(size.X) && packet.rows == static_cast<uint32_t>(size.Y),"child resize dimensions");
                    if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("resized screen mismatch");
                }
            }
            if (name == L"NEGATIVE_TIMEOUT" && !session.until([] { return false; },1000))
                throw Mismatch("deliberate readiness deadline");
            if (name == L"NEGATIVE_ORPHAN" && WaitForSingleObject(session.child.value,0) != WAIT_OBJECT_0)
                throw Mismatch("deliberately live child detected before cleanup");
        }
        observations.comparison=differenceJson(session.state,packet);
        observations.fields+=",\"ObservationMilliseconds\":"+std::to_string(GetTickCount64()-begin);
        if (name == L"NEGATIVE_DRAIN") session.dropOutput=true;
        char quit='Q'; transfer(pipe.value,&quit,1,true); session.finish();
        const bool drain=session.state.contains(U"FINAL_DRAIN");
        observations.add("FinalDrainMatches",drain);
        if (!drain) throw Mismatch("final marker missing from decoded output");
    } catch (...) {
        observations.comparison=differenceJson(session.state,packet);
        observations.raw=session.raw; observations.cleanup=session.cleanup(); throw;
    }
    observations.raw=session.raw; observations.cleanup=session.cleanup();
}
void shellCase(const std::wstring &name,const std::wstring &directoryPath,const std::wstring &program,Observations &observations) {
    const bool cmd=name.starts_with(L"CMD"), partial=name.ends_with(L"PARTIAL");
    const std::wstring executable=cmd ? systemProgram(L"cmd.exe") :
        (name.starts_with(L"PS7") ? program : systemProgram(L"WindowsPowerShell\\v1.0\\powershell.exe"));
    check(!executable.empty(),"shell executable path");
    const std::u32string expectedPrompt=cmd ? U"FIDELITY_CMD_PROMPT>" : U"FIDELITY_PS_PROMPT>";
    std::wstring boot=L"function global:prompt { 'FIDELITY_PS_PROMPT>' }";
    if (name.starts_with(L"PS7")) {
        const std::wstring module=(std::filesystem::path(executable).parent_path()/L"Modules"/L"PSReadLine"/L"PSReadLine.psd1").wstring();
        check(GetFileAttributesW(module.c_str()) != INVALID_FILE_ATTRIBUTES,"PowerShell 7 case requires bundled PSReadLine");
        boot=L"Import-Module "+psLiteral(module)+L" -ErrorAction Stop; Set-PSReadLineOption -HistorySavePath "+
            psLiteral(directoryPath+L"\\psreadline-history.txt")+
            L" -HistorySaveStyle SaveNothing -PredictionSource None -EditMode Windows; "+boot;
    }
    const std::wstring command=cmd ? quote(executable)+L" /D /Q /K \"prompt FIDELITY_CMD_PROMPT$G\"" :
        quote(executable)+L" -NoLogo -NoProfile -NoExit -Command "+quote(boot);
    Session session;
    try {
        session.spawn(executable,command);
        auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
        if (!session.until(atPrompt,20000)) throw std::runtime_error("controlled shell prompt unavailable");
        if (!cmd) {
            const std::wstring identityCommand=L"$m=Get-Module PSReadLine; $mv='none'; $cfg='native-ConsoleHost'; if($m){$mv=$m.Version.ToString(); $o=Get-PSReadLineOption; $cfg=$m.Path+'|'+$o.EditMode.ToString()+'|'+$o.HistorySaveStyle.ToString()}; [IO.File]::WriteAllText("+
                psLiteral(directoryPath+L"\\shell-identity.txt")+L",$PSVersionTable.PSVersion.ToString()+'|'+$mv+'|'+$Host.Name+'|'+$cfg)";
            const uint64_t lines=session.lineFeeds;
            session.input(utf8(identityCommand+L"\r"));
            if (!session.until([&] { return session.lineFeeds>lines && atPrompt() &&
                GetFileAttributesW((directoryPath+L"\\shell-identity.txt").c_str()) != INVALID_FILE_ATTRIBUTES; },10000))
                throw std::runtime_error("shell identity barrier");
            std::ifstream identity(std::filesystem::path(directoryPath+L"\\shell-identity.txt"));
            std::string metadata; std::getline(identity,metadata);
            check(!metadata.empty(),"shell metadata");
            observations.fields+=",\"ShellIdentity\":"+json(metadata);
            if (name.starts_with(L"PS7") && (!metadata.starts_with("7.2.24|") || metadata.find("|none|") != std::string::npos))
                throw std::runtime_error("PowerShell 7 case requires engine 7.2.24 and loaded PSReadLine");
        }
        const std::wstring canceledPath=directoryPath+L"\\cancelled-command.txt";
        check(GetFileAttributesW(canceledPath.c_str()) == INVALID_FILE_ATTRIBUTES,"cancellation marker preflight");
        if (partial) {
            const std::wstring input=cmd ? L"echo BAD>"+quote(canceledPath) :
                L"[IO.File]::WriteAllText("+psLiteral(canceledPath)+L",'BAD')";
            session.input(utf8(input));
            if (!session.until([&] { return !atPrompt(); })) throw std::runtime_error("partial input not displayed");
        }
        const uint64_t before=session.lineFeeds;
        session.input("\x03");
        const bool fresh=session.until([&] { return session.lineFeeds>before && atPrompt(); },2000);
        observations.add("FreshPromptAfterCtrlC",fresh);
        session.input("\r");
        if (!session.until(atPrompt)) throw std::runtime_error("shell prompt after boundary Enter unavailable");
        // Execute the next command through a separate file barrier so its echoed
        // text cannot falsely count as application output.
        const std::wstring continuedPath=directoryPath+L"\\continued-command.txt";
        const std::wstring next=cmd ? L"echo CONTINUE>"+quote(continuedPath) :
            L"[IO.File]::WriteAllText("+psLiteral(continuedPath)+L",'CONTINUE')";
        session.input(utf8(next+L"\r"));
        const bool usable=session.until([&] { return GetFileAttributesW(continuedPath.c_str()) != INVALID_FILE_ATTRIBUTES && atPrompt(); });
        const bool canceled=GetFileAttributesW(canceledPath.c_str()) == INVALID_FILE_ATTRIBUTES;
        observations.add("CanceledCommandNotExecuted",canceled);
        observations.add("NextCommandExecuted",usable);
        observations.add("ShellAlive",WaitForSingleObject(session.child.value,0) == WAIT_TIMEOUT);
        session.input("exit 0\r"); session.finish();
        if (!fresh || !canceled || !usable) throw Mismatch("shell prompt cancellation workflow failed");
    } catch (...) { observations.raw=session.raw; observations.cleanup=session.cleanup(); throw; }
    observations.raw=session.raw; observations.cleanup=session.cleanup();
}
}
int wmain(int argc,wchar_t **argv) {
    if (argc<3 || argc>4) return 2;
    const std::wstring name=argv[1], outputDirectory=argv[2], program=argc == 4 ? argv[3] : L"";
    const ULONGLONG begin=GetTickCount64();
    Observations observations;
    std::string status="Pass", category="Observed", error;
    int result=0;
    try {
        const std::vector<std::wstring> cases={L"STATE",L"UNICODE",L"ALTERNATE",L"RESIZE",L"INPUT",L"PROCESSED_CTRL_C",L"UNPROCESSED_CTRL_C",L"DRAIN",L"NEGATIVE_CELL",L"NEGATIVE_ATTRIBUTE",L"NEGATIVE_CURSOR",L"NEGATIVE_PACKET",L"NEGATIVE_DUPLICATE",L"NEGATIVE_DRAIN",L"NEGATIVE_TIMEOUT",L"NEGATIVE_ORPHAN",L"CMD_EMPTY",L"CMD_PARTIAL",L"PS_EMPTY",L"PS_PARTIAL",L"PS7_EMPTY",L"PS7_PARTIAL"};
        check(std::find(cases.begin(),cases.end(),name) != cases.end(),"unknown case");
        if (name.starts_with(L"CMD") || name.starts_with(L"PS")) shellCase(name,outputDirectory,program,observations);
        else fixtureCase(name,outputDirectory,observations);
    } catch (const Mismatch &e) { status="Fail"; category="BehaviorMismatch"; error=e.what(); result=1; }
    catch (const std::exception &e) { status="Fail"; category="HarnessOrPrerequisiteError"; error=e.what(); result=2; }
    try {
        writeFile(outputDirectory+L"\\stream.bin",observations.raw);
        const std::string record="{\"SchemaVersion\":1,\"Case\":"+json(utf8(name))+
            ",\"Status\":"+json(status)+",\"Category\":"+json(category)+",\"Error\":"+json(error)+
            ",\"DurationMilliseconds\":"+std::to_string(GetTickCount64()-begin)+
            ",\"OwnedProcessesCleaned\":"+(observations.cleanup ? "true" : "false")+
            ",\"Console\":"+observations.console+",\"DecodedState\":"+observations.comparison+observations.fields+"}";
        writeFile(outputDirectory+L"\\case.json",record);
    } catch (const std::exception &e) { fprintf(stderr,"Cannot preserve result: %s\n",e.what()); return 3; }
    printf("Fidelity %s: %s\n",utf8(name).c_str(),status.c_str());
    return result;
}
