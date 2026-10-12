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
    Handle child, agent, descendant;
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
    void finish(DWORD expectedStatus=0) {
        if (!until([&] { return eof_ && WaitForSingleObject(child.value,0) == WAIT_OBJECT_0; },10000))
            throw Mismatch("final output/child completion deadline");
        DWORD status=0; check(GetExitCodeProcess(child.value,&status),"child status");
        if (status != expectedStatus) throw std::runtime_error("fixture/controlled shell exit "+std::to_string(status));
        if (!cleanup()) throw Mismatch("owned process survived shutdown");
    }
    bool cleanup() noexcept {
        if (closed_) return cleanupOkay_;
        closed_=true;
        input_.reset();
        if (pty_) { vt7pty_free(pty_); pty_=nullptr; }
        cleanupOkay_=true;
        for (HANDLE process : {descendant.value,child.value,agent.value}) {
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
std::u32string startShell(const std::wstring &name,const std::wstring &directoryPath,const std::wstring &program,
    Session &session,Observations &observations) {
    const bool cmd=name.starts_with(L"CMD");
    const std::wstring executable=cmd ? systemProgram(L"cmd.exe") :
        (name.starts_with(L"PS7") ? program : systemProgram(L"WindowsPowerShell\\v1.0\\powershell.exe"));
    check(!executable.empty(),"shell executable path");
    const std::u32string expectedPrompt=cmd ? U"FIDELITY_CMD_PROMPT>" : U"FIDELITY_PS_PROMPT>";
    std::wstring boot=name.starts_with(L"PS7") ? L"function global:prompt { 'FIDELITY_PS_PROMPT>' }" :
        L"Remove-Module PSReadLine -ErrorAction SilentlyContinue; function global:prompt { 'FIDELITY_PS_PROMPT>' }";
    if (name.starts_with(L"PS7")) {
        const std::wstring module=(std::filesystem::path(executable).parent_path()/L"Modules"/L"PSReadLine"/L"PSReadLine.psd1").wstring();
        check(GetFileAttributesW(module.c_str()) != INVALID_FILE_ATTRIBUTES,"PowerShell 7 case requires bundled PSReadLine");
        boot=L"Import-Module "+psLiteral(module)+L" -ErrorAction Stop; Set-PSReadLineOption -HistorySavePath "+
            psLiteral(directoryPath+L"\\psreadline-history.txt")+
            L" -HistorySaveStyle SaveNothing -PredictionSource None -EditMode Windows; "+boot;
    }
    const std::wstring command=cmd ? quote(executable)+L" /D /Q /K \"prompt FIDELITY_CMD_PROMPT$G\"" :
        quote(executable)+L" -NoLogo -NoProfile -NoExit -Command "+quote(boot);
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
        if (name.starts_with(L"PS_") && metadata.find("|none|ConsoleHost|native-ConsoleHost") == std::string::npos)
            throw std::runtime_error("Windows PowerShell case requires native ConsoleHost editing");
    }
    return expectedPrompt;
}

std::string childJson(const ChildPacket &p) {
    std::string keys="[";
    for (uint32_t i=0;i<p.keyCount;++i) {
        if (i) keys+=",";
        keys+="{\"Character\":"+std::to_string(p.keys[i].character)+",\"VirtualKey\":"+std::to_string(p.keys[i].virtualKey)+
            ",\"Modifiers\":"+std::to_string(p.keys[i].modifiers)+",\"Repeat\":"+std::to_string(p.keys[i].repeat)+"}";
    }
    return "{\"Stage\":"+std::to_string(p.stage)+",\"ProcessId\":"+std::to_string(p.processId)+
        ",\"ParentId\":"+std::to_string(p.parentId)+",\"InputMode\":"+std::to_string(p.inputMode)+
        ",\"ControlCount\":"+std::to_string(p.controlCount)+",\"ControlType\":"+std::to_string(p.controlType)+
        ",\"ReadReturned\":"+std::to_string(p.readReturned)+",\"ReadSucceeded\":"+std::to_string(p.readSucceeded)+
        ",\"ReadError\":"+std::to_string(p.readError)+",\"Text\":"+json(utf8(std::wstring_view(p.text,p.textCount)))+
        ",\"Keys\":"+keys+"]}";
}
bool receiveChild(HANDLE pipe,ChildPacket &packet) {
    DWORD available=0;
    if (!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)) {
        const DWORD error=GetLastError();
        check(error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED,"child observation peek");
        return false;
    }
    if (available<sizeof(packet)) return false;
    transfer(pipe,&packet,sizeof(packet),false); validateChild(packet); return true;
}
void saveChild(const std::wstring &directoryPath,const wchar_t *name,const ChildPacket &packet) {
    writeFile(directoryPath+L"\\"+name,std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
}
std::string fileText(const std::wstring &path) {
    std::ifstream file{std::filesystem::path(path)};
    std::string text; std::getline(file,text);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) text.pop_back();
    return text;
}
void childCase(const std::wstring &name,const std::wstring &resultDirectory,const std::wstring &program,Observations &observations) {
    const bool negative=name.starts_with(L"NEGATIVE_CHILD");
    const bool cmd=negative || name.starts_with(L"CMD");
    const std::wstring shellName=negative ? L"CMD_CHILD_HANDLED" : name;
    const bool defaultHandler=name.ends_with(L"DEFAULT"), unprocessed=name.ends_with(L"UNPROCESSED");
    const std::wstring mode=defaultHandler ? L"CHILD_DEFAULT" : (unprocessed ? L"CHILD_UNPROCESSED" : L"CHILD_HANDLED");
    Session session;
    Handle pipe;
    bool connected=false;
    try {
        const auto expectedPrompt=startShell(shellName,resultDirectory,program,session,observations);
        auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Child."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        pipe.reset(privateServer(pipeName));
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        const std::wstring serverId=std::to_wstring(GetCurrentProcessId());
        const std::wstring command=cmd ? quote(fixture)+L" "+mode+L" "+quote(pipeName)+L" "+serverId :
            L"& "+psLiteral(fixture)+L" "+psLiteral(mode)+L" "+psLiteral(pipeName)+L" "+serverId;
        session.input(utf8(command+L"\r"));
        connect(pipe.value); connected=true;
        ULONG peer=0; check(GetNamedPipeClientProcessId(pipe.value,&peer),"child peer PID");
        Handle candidate(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,FALSE,peer));
        const HANDLE nativeChild=candidate.value;
        if (nativeChild == nullptr) throw std::runtime_error("cannot open child process (Win32 "+std::to_string(GetLastError())+")");
        check(parentProcessId(peer) == GetProcessId(session.child.value),"fixture parent is tested shell");
        std::vector<wchar_t> image(32768); DWORD length=static_cast<DWORD>(image.size());
        check(QueryFullProcessImageNameW(nativeChild,0,image.data(),&length) && _wcsicmp(image.data(),fixture.c_str()) == 0,"child fixture image");
        // Adopt teardown ownership only after both ancestry and image validation.
        session.descendant.reset(candidate.value); candidate.value=INVALID_HANDLE_VALUE;
        ChildPacket packet;
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(packet.stage == 1 && packet.processId == peer && packet.parentId == GetProcessId(session.child.value) &&
            packet.controlCount == 0 && packet.keyCount == 0 &&
            ((packet.inputMode&ENABLE_PROCESSED_INPUT) != 0) == !unprocessed,"child readiness identity and mode");
        saveChild(resultDirectory,L"child-ready.bin",packet);
        observations.fields+=",\"ChildReady\":"+childJson(packet);
        session.input(unprocessed ? "\x03z" : "\x03");
        bool finalPacket=false;
        const bool exited=session.until([&] {
            while (receiveChild(pipe.value,packet)) { finalPacket=packet.stage == 2; }
            return WaitForSingleObject(session.descendant.value,0) == WAIT_OBJECT_0;
        },5000);
        observations.add("ChildExitedAfterCtrlC",exited);
        // A missed interruption remains a workflow failure. Ask only this owned
        // fixture to stop before tearing down its shell and console.
        if (!exited) { char quit='Q'; transfer(pipe.value,&quit,1,true); }
        if (!session.until([&] { return WaitForSingleObject(session.descendant.value,0) == WAIT_OBJECT_0; },5000))
            throw Mismatch("owned native child shutdown deadline");
        while (receiveChild(pipe.value,packet)) { finalPacket=packet.stage == 2; }
        DWORD status=0; check(GetExitCodeProcess(session.descendant.value,&status),"native child exit status");
        saveChild(resultDirectory,L"child-observation.bin",packet);
        observations.fields+=",\"ChildObservation\":"+childJson(packet)+",\"ChildExitCode\":"+std::to_string(status);
        observations.add("ChildFinalPacketReceived",finalPacket);
        ChildPacket oraclePacket=packet;
        DWORD oracleStatus=status;
        if (name == L"NEGATIVE_CHILD_CONTROL") {
            ++oraclePacket.controlCount;
            observations.fields+=",\"InjectedFault\":\"duplicate child control count\"";
        }
        if (name == L"NEGATIVE_CHILD_EXIT") {
            ++oracleStatus;
            observations.fields+=",\"InjectedFault\":\"wrong native child exit status\"";
        }
        const bool control=(defaultHandler || finalPacket) && childInputMatches(oraclePacket,defaultHandler,unprocessed);
        const DWORD expectedStatus=expectedChildExit(defaultHandler);
        const bool statusMatches=oracleStatus == expectedStatus;
        observations.add("ChildControlMatches",control);
        observations.add("ChildExitMatches",statusMatches);
        const bool returned=session.until(atPrompt,5000);
        observations.add("ReturnedToShellPrompt",returned);
        const bool drained=defaultHandler || session.state.contains(U"CHILD_FINAL_DRAIN");
        observations.add("ChildFinalOutputMatches",drained);
        const std::wstring statusPath=resultDirectory+L"\\child-exit-code.txt";
        const std::wstring continuedPath=resultDirectory+L"\\continued-command.txt";
        const std::wstring next=cmd ? L"echo %errorlevel%>"+quote(statusPath)+L" & echo CONTINUE>"+quote(continuedPath) :
            L"[IO.File]::WriteAllText("+psLiteral(statusPath)+L",$LASTEXITCODE.ToString()); [IO.File]::WriteAllText("+psLiteral(continuedPath)+L",'CONTINUE')";
        bool usable=false, reported=false;
        if (returned) {
            session.input(utf8(next+L"\r"));
            usable=session.until([&] { return GetFileAttributesW(continuedPath.c_str()) != INVALID_FILE_ATTRIBUTES && atPrompt(); });
            const auto actual=fileText(statusPath);
            reported=actual == std::to_string(static_cast<int32_t>(expectedStatus)) || actual == std::to_string(expectedStatus);
            observations.fields+=",\"ShellReportedChildExit\":"+json(actual);
        }
        observations.add("ShellReportedExitMatches",reported);
        observations.add("NextCommandExecuted",usable);
        const bool alive=WaitForSingleObject(session.child.value,0) == WAIT_TIMEOUT;
        observations.add("ShellAlive",alive);
        if (alive && returned) { session.input("exit 0\r"); session.finish(); }
        observations.raw=session.raw; observations.cleanup=session.cleanup();
        if (!exited || !control || !statusMatches || !returned || !drained || !reported || !usable || !alive)
            throw Mismatch("active child interruption workflow failed");
    } catch (...) {
        if (connected && session.descendant.value != nullptr && session.descendant.value != INVALID_HANDLE_VALUE &&
            WaitForSingleObject(session.descendant.value,0) == WAIT_TIMEOUT) {
            try { char quit='Q'; transfer(pipe.value,&quit,1,true,1000); WaitForSingleObject(session.descendant.value,5000); } catch (...) {}
        }
        pipe.reset();
        observations.raw=session.raw; observations.cleanup=session.cleanup(); throw;
    }
}
void cookedCase(const std::wstring &name,const std::wstring &resultDirectory,Observations &observations) {
    Session session;
    try {
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Cooked."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        Handle pipe(privateServer(pipeName));
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        session.spawn(fixture,quote(fixture)+L" CHILD_COOKED "+quote(pipeName)+L" "+std::to_wstring(GetCurrentProcessId()));
        connect(pipe.value);
        ULONG peer=0; check(GetNamedPipeClientProcessId(pipe.value,&peer) && peer == GetProcessId(session.child.value),"cooked peer PID");
        ChildPacket packet;
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(packet.stage == 1 && packet.processId == peer && !packet.readReturned &&
            (packet.inputMode&(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT)) ==
            (ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT),"cooked readiness mode");
        session.input("PROBE_TYPED");
        if (!session.until([&] { return session.state.contains(U"COOKED_INPUT>PROBE_TYPED"); }))
            throw std::runtime_error("pending cooked read echo barrier");
        char sample='S'; transfer(pipe.value,&sample,1,true);
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(!packet.readReturned && !packet.controlCount,"cooked pending read barrier");
        saveChild(resultDirectory,L"child-ready.bin",packet);
        observations.fields+=",\"ChildReady\":"+childJson(packet);
        const bool recordProbe=name == L"READCONSOLE_RECORD_EVENT", ownedCancel=name == L"READCONSOLE_OWNED_CANCEL";
        const bool mechanism=recordProbe || ownedCancel, direct=name == L"READCONSOLE_EVENT" || mechanism;
        observations.fields+=",\"ControlDelivery\":"+json(recordProbe ? "WriteConsoleInputThenGenerateEvent" :
            ownedCancel ? "CancelOwnedReaderThenGenerateEvent" : direct ? "FixtureGenerateConsoleCtrlEvent" : "VTByte03");
        DWORD mechanismReply[3]={};
        if (direct) {
            char command=recordProbe ? 'R' : ownedCancel ? 'K' : 'G'; transfer(pipe.value,&command,1,true);
            if (mechanism) {
                transfer(pipe.value,mechanismReply,sizeof(mechanismReply),false);
                check(mechanismReply[0] <= 1 && (!mechanismReply[0] || mechanismReply[1] == ERROR_SUCCESS) &&
                    (ownedCancel ? mechanismReply[2] == 0 : mechanismReply[2] <= 2),"mechanism reply bounds");
                observations.add("MechanismSucceeded",mechanismReply[0] == 1);
                observations.fields+=",\"MechanismError\":"+std::to_string(mechanismReply[1])+
                    ",\"MechanismRecordsWritten\":"+std::to_string(mechanismReply[2]);
            }
        } else session.input("\x03");
        const bool completed=session.until([&] {
            while (receiveChild(pipe.value,packet)) {}
            return packet.stage == 2;
        },2000);
        if (!completed) {
            transfer(pipe.value,&sample,1,true); transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        }
        saveChild(resultDirectory,L"child-observation.bin",packet);
        observations.fields+=",\"ChildObservation\":"+childJson(packet);
        const bool signal=packet.controlCount == 1 && packet.controlType == CTRL_C_EVENT;
        const bool canceled=completed && packet.readReturned && packet.textCount == 0 &&
            (!ownedCancel || packet.readError == ERROR_OPERATION_ABORTED);
        observations.add("ControlSignalMatches",signal); observations.add("CookedReadCanceled",canceled);
        if (!completed) {
            session.input("\r");
            if (!session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000))
                throw Mismatch("cooked boundary Enter recovery deadline");
            observations.fields+=",\"BoundaryEnterRead\":"+childJson(packet);
            saveChild(resultDirectory,L"boundary-enter-read.bin",packet);
        }
        bool reusable=true;
        if (ownedCancel) {
            char next='N'; transfer(pipe.value,&next,1,true);
            transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
            check(packet.stage == 1 && !packet.readReturned,"next cooked read readiness");
            saveChild(resultDirectory,L"child-next-ready.bin",packet);
            observations.fields+=",\"ChildNextReady\":"+childJson(packet);
            session.input("NEXT_PROBE\r");
            const bool nextCompleted=session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000);
            saveChild(resultDirectory,L"child-next-read.bin",packet);
            observations.fields+=",\"ChildNextRead\":"+childJson(packet);
            reusable=nextCompleted && packet.readReturned && packet.readSucceeded &&
                packet.readError == ERROR_SUCCESS && std::wstring_view(packet.text,packet.textCount) == L"NEXT_PROBE\r\n" &&
                packet.controlCount == 1 && packet.controlType == CTRL_C_EVENT;
            observations.add("NextCookedReadMatches",reusable);
            if (!nextCompleted) {
                // Preserve the failed next read before recovery. A canceled
                // client call alone must not qualify console-side cancellation.
                char sampleNext='S'; transfer(pipe.value,&sampleNext,1,true);
                transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
                saveChild(resultDirectory,L"child-next-pending.bin",packet);
                observations.fields+=",\"ChildNextPending\":"+childJson(packet);
                session.input("\r");
                const bool recovered=session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000);
                saveChild(resultDirectory,L"next-boundary-enter-read.bin",packet);
                observations.fields+=",\"NextBoundaryEnterRead\":"+childJson(packet);
                observations.add("NextBoundaryEnterRecovered",recovered);
                if (!recovered) throw Mismatch("next cooked read recovery deadline");
            }
        }
        if (!session.until([&] { return WaitForSingleObject(session.child.value,0) == WAIT_OBJECT_0 && session.state.contains(U"CHILD_FINAL_DRAIN"); }))
            throw Mismatch("cooked fixture drain/exit deadline");
        DWORD status=0; check(GetExitCodeProcess(session.child.value,&status),"cooked child exit status");
        observations.fields+=",\"ChildExitCode\":"+std::to_string(status);
        // Drain through EOF while accepting the fixture's deliberate exit 37.
        if (status == 37) session.finish(37);
        observations.raw=session.raw; observations.cleanup=session.cleanup();
        if (!signal || !canceled || !reusable || status != 37 ||
            (mechanism && (!mechanismReply[0] || (recordProbe && mechanismReply[2] != 2)))) throw Mismatch("cooked console read cancellation workflow failed");
    } catch (...) { observations.raw=session.raw; observations.cleanup=session.cleanup(); throw; }
}

void shellCase(const std::wstring &name,const std::wstring &directoryPath,const std::wstring &program,Observations &observations) {
    const bool cmd=name.starts_with(L"CMD"), partial=name.ends_with(L"PARTIAL");
    Session session;
    try {
        const auto expectedPrompt=startShell(name,directoryPath,program,session,observations);
        auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
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
        const std::vector<std::wstring> cases={L"STATE",L"UNICODE",L"ALTERNATE",L"RESIZE",L"INPUT",L"PROCESSED_CTRL_C",L"UNPROCESSED_CTRL_C",L"DRAIN",L"NEGATIVE_CELL",L"NEGATIVE_ATTRIBUTE",L"NEGATIVE_CURSOR",L"NEGATIVE_PACKET",L"NEGATIVE_DUPLICATE",L"NEGATIVE_DRAIN",L"NEGATIVE_TIMEOUT",L"NEGATIVE_ORPHAN",L"CMD_EMPTY",L"CMD_PARTIAL",L"PS_EMPTY",L"PS_PARTIAL",L"PS7_EMPTY",L"PS7_PARTIAL",L"CMD_CHILD_HANDLED",L"CMD_CHILD_DEFAULT",L"CMD_CHILD_UNPROCESSED",L"PS_CHILD_HANDLED",L"PS_CHILD_DEFAULT",L"PS_CHILD_UNPROCESSED",L"PS7_CHILD_HANDLED",L"PS7_CHILD_DEFAULT",L"PS7_CHILD_UNPROCESSED",L"NEGATIVE_CHILD_CONTROL",L"NEGATIVE_CHILD_EXIT",L"READCONSOLE_TRANSPORT",L"READCONSOLE_EVENT",L"READCONSOLE_RECORD_EVENT",L"READCONSOLE_OWNED_CANCEL"};
        check(std::find(cases.begin(),cases.end(),name) != cases.end(),"unknown case");
        if (name.find(L"_CHILD_") != std::wstring::npos) childCase(name,outputDirectory,program,observations);
        else if (name.starts_with(L"READCONSOLE_")) cookedCase(name,outputDirectory,observations);
        else if (name.starts_with(L"CMD") || name.starts_with(L"PS")) shellCase(name,outputDirectory,program,observations);
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
