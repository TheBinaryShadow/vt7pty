// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
struct ManualConsole
{
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
    DWORD original=0;
    ManualConsole()
    {
        check(GetConsoleMode(input,&original),"manual checks require a classic console");
        check(SetConsoleMode(input,ENABLE_EXTENDED_FLAGS),"manual raw input mode");
    }
    ~ManualConsole(){SetConsoleMode(input,original);}
};
void say(const std::wstring &text)
{
    DWORD written=0;
    check(WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),static_cast<DWORD>(text.size()),
        &written,nullptr) && written==text.size(),"manual instruction output");
}
std::string keyBytes(const KEY_EVENT_RECORD &key)
{
    if(!key.bKeyDown)return {};
    if(key.uChar.UnicodeChar)return utf8(std::wstring(1,key.uChar.UnicodeChar));
    switch(key.wVirtualKeyCode) {
    case VK_LEFT:return "\x1b[D";case VK_RIGHT:return "\x1b[C";
    case VK_UP:return "\x1b[A";case VK_DOWN:return "\x1b[B";
    case VK_HOME:return "\x1b[H";case VK_END:return "\x1b[F";
    case VK_DELETE:return "\x1b[3~";default:return {};
    }
}
void manualEncodingOracle()
{
    KEY_EVENT_RECORD key={};key.bKeyDown=TRUE;key.uChar.UnicodeChar=3;
    check(keyBytes(key)=="\x03","manual Ctrl+C encoding");
    key.uChar.UnicodeChar=L'\u010d';check(keyBytes(key)=="\xc4\x8d","manual Unicode encoding");
    key.uChar.UnicodeChar=0;key.wVirtualKeyCode=VK_LEFT;check(keyBytes(key)=="\x1b[D","manual navigation encoding");
    key.bKeyDown=FALSE;check(keyBytes(key).empty(),"manual key-up records are retained without text duplication");
}
struct PhysicalInput
{
    std::string records="[",forwarded;
    bool shift=false,altGr=false,deadPrefix=false;
    unsigned int count=0;
    DWORD layout=0;
};
PhysicalInput bridge(Session &session,HANDLE input,int stop)
{
    PhysicalInput result;
    const auto deadline=GetTickCount64()+180000;
    // A console HWND can be a client-side proxy. Query the console layout itself.
    using ConsoleLayout=BOOL (WINAPI *)(LPWSTR);
    const auto layoutName=reinterpret_cast<ConsoleLayout>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetConsoleKeyboardLayoutNameW"));
    check(layoutName!=nullptr,"console keyboard layout query available");

    bool done=false;
    while(!done && GetTickCount64()<deadline) {
        session.pump();
        if(WaitForSingleObject(input,100)!=WAIT_OBJECT_0)continue;
        INPUT_RECORD record={};DWORD read=0;
        check(ReadConsoleInputW(input,&record,1,&read) && read==1,"physical keyboard record");
        if(record.EventType!=KEY_EVENT)continue;
        const auto &key=record.Event.KeyEvent;
        check(++result.count<=512,"bounded physical keyboard evidence");
        if(result.count>1)result.records+=',';
        result.records+="{\"Down\":"+boolean(key.bKeyDown!=FALSE)+",\"VirtualKey\":"+std::to_string(key.wVirtualKeyCode)+
            ",\"ScanCode\":"+std::to_string(key.wVirtualScanCode)+",\"Character\":"+std::to_string(key.uChar.UnicodeChar)+
            ",\"Modifiers\":"+std::to_string(key.dwControlKeyState)+",\"Repeat\":"+std::to_string(key.wRepeatCount)+"}";
        if(!key.bKeyDown)continue;
        if(!result.layout && key.uChar.UnicodeChar) {
            wchar_t name[KL_NAMELENGTH]={};
            check(layoutName(name)!=FALSE,"actual console keyboard layout");
            wchar_t *end=nullptr;
            const auto id=wcstoul(name,&end,16);
            check(end==name+8,"eight-digit console layout identifier");
            result.layout=static_cast<DWORD>(id&0xffff);
        }
        result.shift=result.shift || (key.uChar.UnicodeChar && IsCharUpperW(key.uChar.UnicodeChar) &&
            (key.dwControlKeyState&SHIFT_PRESSED)!=0);
        result.altGr=result.altGr || (key.uChar.UnicodeChar && (key.dwControlKeyState&RIGHT_ALT_PRESSED)!=0 &&
            (key.dwControlKeyState&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))!=0);
        result.deadPrefix=result.deadPrefix || (!key.uChar.UnicodeChar &&
            key.wVirtualKeyCode!=VK_SHIFT && key.wVirtualKeyCode!=VK_CONTROL && key.wVirtualKeyCode!=VK_MENU &&
            key.wVirtualKeyCode!=VK_LSHIFT && key.wVirtualKeyCode!=VK_RSHIFT &&
            key.wVirtualKeyCode!=VK_LCONTROL && key.wVirtualKeyCode!=VK_RCONTROL &&
            key.wVirtualKeyCode!=VK_LMENU && key.wVirtualKeyCode!=VK_RMENU);
        const auto bytes=keyBytes(key);
        for(WORD repeat=0;repeat<key.wRepeatCount;++repeat) {
            if(!bytes.empty()){session.input(bytes);result.forwarded+=bytes;}
        }
        done=(stop==1 && key.wVirtualKeyCode==VK_RETURN) ||
             (stop==2 && key.uChar.UnicodeChar==3) ||
             (stop==3 && key.uChar.UnicodeChar==L'z');
    }
    result.records+="]";
    check(done,"physical keyboard step deadline");
    return result;
}
void physicalRecord(const std::wstring &path,const PhysicalInput &input)
{
    writeFile(path,"{\"Source\":\"PhysicalConsoleInput\",\"SyntheticInputUsed\":false,\"LayoutLanguage\":"+
        std::to_string(input.layout)+",\"ShiftObserved\":"+boolean(input.shift)+
        ",\"AltGrObserved\":"+boolean(input.altGr)+",\"DeadPrefixObserved\":"+boolean(input.deadPrefix)+
        ",\"ForwardedUtf8\":"+json(input.forwarded)+",\"Keys\":"+input.records+"}");
}
void exactManualRead(Session &s,const std::wstring &expected,const std::wstring &path,uint32_t events)
{
    IdentityPacket p;
    check(s.until([&]{p=s.observe();return p.readReturned!=0;},5000),"manual read returned");
    writeFile(path,packetJson(p));
    check(p.readSucceeded && !p.readError && p.controlCount==events &&
        std::wstring(p.text,p.textCount)==expected+L"\r\n","exact manual line");
}
int runKeyboard(const std::wstring &out,const std::wstring &layout)
{
    check(layout==L"US" || layout==L"HR","manual layout US or HR");
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new keyboard evidence");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"manual keyboard trace");
    ManualConsole console;Collector collector(out);
    Context context(out+L"\\session");auto &s=context.start();
    const DWORD expectedLayout=layout==L"US" ? 0x409u:0x41au;
    say(L"\r\nH4 physical keyboard check. Select the "+layout+L" layout on this console.\r\n"
        L"I forward the text and navigation bytes from the keys you press to the real VT7Pty input pipe.\r\n");
    const std::wstring expected=layout==L"US" ? L"AZaz09[]{}\\|" : L"\u010d\u0107\u0111\u0161\u017e \u010c\u0106\u0110\u0160\u017d @[]{}\\|";
    startRead(s,"");
    say(L"Type this line exactly, using physical Shift/AltGr keys as needed, then press Enter:\r\n"+expected+L"\r\n");
    const auto text=bridge(s,console.input,1);physicalRecord(out+L"\\text-keys.json",text);
    exactManualRead(s,expected,out+L"\\text-read.json",0);
    check(text.layout==expectedLayout && text.shift,"physical layout and Shift");
    if(layout==L"HR") {
        check(text.altGr,"physical Croatian AltGr");
        startRead(s,"");
        say(L"\r\nUse a dead-key composition to type \u00e1, then press Enter.\r\n");
        const auto dead=bridge(s,console.input,1);physicalRecord(out+L"\\dead-keys.json",dead);
        exactManualRead(s,L"\u00e1",out+L"\\dead-read.json",0);
        check(dead.layout==expectedLayout && dead.deadPrefix,"physical dead-key prefix");
    }
    startRead(s,"");
    say(L"\r\nType OLD, then press Ctrl+C without pressing Enter.\r\n");
    const auto cancel=bridge(s,console.input,2);physicalRecord(out+L"\\cancel-keys.json",cancel);
    check(cancel.forwarded=="OLD\x03","manual cancellation input sequence");
    auto p=waitRead(s,1);writeFile(out+L"\\canceled-read.json",packetJson(p));
    check(canceled(p,1),"physical cooked Ctrl+C");
    s.observe('R');say(L"\r\nType NEXT, then press Enter.\r\n");
    const auto next=bridge(s,console.input,1);physicalRecord(out+L"\\next-keys.json",next);
    exactManualRead(s,L"NEXT",out+L"\\next-read.json",1);
    s.observe('U');say(L"\r\nRaw-input check: press Ctrl+C, release it, then type lowercase z. Do not press Enter.\r\n");
    const auto raw=bridge(s,console.input,3);physicalRecord(out+L"\\raw-keys.json",raw);
    check(s.until([&]{p=s.observe();return p.readReturned!=0;},5000),"physical raw read");
    writeFile(out+L"\\raw-read.json",packetJson(p));
    check(p.controlCount==1 && p.readSucceeded && !p.readError && p.keyCount==3 &&
        p.keys[0].virtualKey==VK_CONTROL && p.keys[1].character==3 && p.keys[2].character=='z',
        "physical raw Ctrl+C has no control event");
    nextRead(s,context.out,1);
    check(context.finish(),"manual keyboard host cleanup");
    collector.stop();
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4ManualKeyboard\",\"SchemaVersion\":1,\"Status\":\"Pass\",\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"DeclaredLayout\":"+json(utf8(layout))+",\"LayoutLanguage\":"+
        std::to_string(expectedLayout)+",\"PhysicalInputRequired\":true,\"H4Accepted\":false}");
    say(L"\r\nKeyboard check passed. Keep the complete result directory.\r\n");
    return 0;
}
int runSessionCheck(const std::wstring &out)
{
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new session-transition evidence");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"manual session trace");
    Collector collector(out);
    Context context(out+L"\\existing");auto &s=context.start();startRead(s,"SESSION_KEEP");
    say(L"\r\nH4 lock/unlock check. Save your work, then press Win+L and sign back in.\r\n"
        L"I do not lock, switch desktops, sign out, or change your Windows settings.\r\n"
        L"The pending test line stays in its private backing console. You have five minutes.\r\n");
    const auto deadline=GetTickCount64()+300000;
    bool inactive=false,recovered=false,denied=false;
    std::string states="[",previous;
    unsigned int changes=0;
    while(GetTickCount64()<deadline) {
        const auto snapshot=h4host::desktopState();
        const auto description=desktopJson();
        const auto key=snapshot.input+L":"+std::to_wstring(snapshot.error);
        if(utf8(key)!=previous) {
            if(changes++)states+=',';
            states+=description;previous=utf8(key);
            check(changes<=128,"bounded desktop transition evidence");
        }
        if(!snapshot.eligible() && !inactive) {
            inactive=true;
            auto *config=vt7pty_config_new(VT7PTY_FLAG_HOST_CANCELLATION,nullptr);
            vt7pty_error_ptr_t error=nullptr;auto *opened=vt7pty_open(config,&error);
            const auto message=error ? utf8(vt7pty_error_msg(error)):"";
            const auto code=vt7pty_error_code(error);
            vt7pty_error_free(error);vt7pty_config_free(config);
            if(opened)vt7pty_free(opened);
            denied=!opened && code && (message.find("ordinary caller desktop")!=std::string::npos ||
                message.find("input desktop")!=std::string::npos);
            writeFile(out+L"\\inactive-open.json","{\"Denied\":"+boolean(denied)+",\"Message\":"+json(message)+
                ",\"ErrorCode\":"+std::to_string(code)+"}");
        }
        if(inactive && snapshot.eligible()){recovered=true;break;}
        Sleep(100);
    }
    states+="]";writeFile(out+L"\\transitions.json","{\"States\":"+states+"}");
    check(inactive && recovered && denied,"observed inactive desktop, startup refusal and return");
    const auto pending=s.observe();writeFile(out+L"\\pending-after-unlock.json",packetJson(pending));
    check(!pending.readReturned && pending.controlCount==0,"pending read preserved across lock");
    s.input("\x03");const auto p=waitRead(s,1);writeFile(out+L"\\cancel-after-unlock.json",packetJson(p));
    check(canceled(p,1),"cancellation recovers after unlock");nextRead(s,context.out,1);
    Context fresh(out+L"\\reopened");cooked(fresh,"after-unlock");
    check(fresh.finish() && context.finish(),"session transition cleanup");
    collector.stop();
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4ManualSession\",\"SchemaVersion\":1,\"Status\":\"Pass\",\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"InactiveObserved\":true,\"Recovered\":true,\"StartupDeniedWhileInactive\":true,"
        "\"PendingReadPreserved\":true,\"DesktopSwitchedByTest\":false,\"H4Accepted\":false}");
    say(L"\r\nSession-transition check passed. Keep the complete result directory.\r\n");
    return 0;
}

int runAccessCheck(const std::wstring &out)
{
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new elevated-caller evidence");
    rootEvidenceCreated=true;
    writeFile(out+L"\\preflight.json",desktopJson());
    h4host::Token token(GetCurrentProcess());
    check(token.integrity>SECURITY_MANDATORY_MEDIUM_RID,"run this check from an elevated classic console");
    check(h4host::desktopState().eligible(),"elevated caller active desktop");
    auto *config=vt7pty_config_new(VT7PTY_FLAG_HOST_CANCELLATION,nullptr);
    vt7pty_error_ptr_t failure=nullptr;
    auto *opened=vt7pty_open(config,&failure);
    const auto message=failure ? utf8(vt7pty_error_msg(failure)):"";
    const auto code=vt7pty_error_code(failure);
    vt7pty_config_free(config);vt7pty_error_free(failure);
    if(opened)vt7pty_free(opened);
    check(!opened && code && message.find("medium-integrity interactive caller")!=std::string::npos,
        "specific elevated caller refusal");
    config=vt7pty_config_new(0,nullptr);
    auto *plain=vt7pty_open(config,nullptr);vt7pty_config_free(config);
    check(plain!=nullptr,"flagless elevated open");
    Handle agent;
    check(DuplicateHandle(GetCurrentProcess(),vt7pty_agent_process(plain),GetCurrentProcess(),&agent.value,
                          0,FALSE,DUPLICATE_SAME_ACCESS),"retained elevated flagless agent");
    const auto query=hostQuery(agent.value);check(valid(query),"elevated flagless host association");
    Handle host(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,
                            static_cast<DWORD>(query.raw&~ULONG_PTR{3})));
    check(host.value!=nullptr,"retained elevated flagless host");
    check(vt7pty_close(plain,nullptr),"flagless elevated close");vt7pty_free(plain);
    check(WaitForSingleObject(agent.value,10000)==WAIT_OBJECT_0 &&
        WaitForSingleObject(host.value,10000)==WAIT_OBJECT_0,"flagless elevated cleanup");
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4ManualAccess\",\"SchemaVersion\":1,\"Status\":\"Pass\",\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"IntegrityRid\":"+std::to_string(token.integrity)+
        ",\"OptInDenied\":true,\"Message\":"+json(message)+",\"ErrorCode\":"+std::to_string(code)+
        ",\"FlaglessOpenPassed\":true,\"ObservedHostExited\":true,\"H4Accepted\":false}");
    printf("Elevated-caller denial check passed.\n");
    return 0;
}
