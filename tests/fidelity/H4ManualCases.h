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
bool manualSynthetic=false;
DWORD manualSyntheticLayout=0;
std::string manualRuntimeSource()
{
    wchar_t value[41]={};
    const auto n=GetEnvironmentVariableW(L"VT7PTY_H4_RUNTIME_SOURCE",value,41);
    if(!n)return GenVersion_Commit;
    check(n==40 && std::wstring(value).find_first_not_of(L"0123456789abcdef")==std::wstring::npos,
        "manual runtime source identity");
    return utf8(value);
}
std::string manualMetadata()
{
    return ",\"ManualRevision\":2,\"RuntimeSourceCommit\":"+json(manualRuntimeSource())+
        ",\"SyntheticInputUsed\":"+boolean(manualSynthetic);
}
void say(const std::wstring &text)
{
    DWORD written=0;
    check(WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),static_cast<DWORD>(text.size()),
        &written,nullptr) && written==text.size(),"manual instruction output");
    if(rootEvidenceCreated) {
        const auto path=std::filesystem::path(outRoot+L"\\ui-transcript.txt");
        check(!std::filesystem::exists(path) || std::filesystem::file_size(path)<1024*1024,
              "bounded manual UI transcript");
        std::ofstream log(path,std::ios::binary|std::ios::app);
        const auto bytes=utf8(text);log.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        check(log.good(),"manual UI transcript");
    }
}
std::wstring visible(const std::wstring &text)
{
    std::wstring result;
    for(auto c:text) {
        if(c==L'\r')result+=L"<CR>";
        else if(c==L'\n')result+=L"<LF>";
        else if(c==3)result+=L"^C";
        else if(c>=32)result+=c;
    }
    return result;
}
struct ConsoleLayout
{
    std::wstring id;
    DWORD language=0;
};
bool queryConsoleLayout(ConsoleLayout &layout)
{
    // Hidden-console smoke records carry an explicit simulated layout. Normal
    // qualification always queries the actual console and never uses this path.
    if(manualSynthetic){layout={manualSyntheticLayout==0x409 ? L"00000409":L"0000041A",manualSyntheticLayout};return true;}
    using Query=BOOL (WINAPI *)(LPWSTR);
    const auto kernel=GetModuleHandleW(L"kernel32.dll");
    if(!kernel)throw std::runtime_error("kernel32.dll unavailable");
    const auto query=reinterpret_cast<Query>(GetProcAddress(kernel,"GetConsoleKeyboardLayoutNameW"));
    check(query!=nullptr,"console keyboard layout query available");
    wchar_t name[KL_NAMELENGTH]={};if(!query(name))return false;
    wchar_t *end=nullptr;const auto n=wcstoul(name,&end,16);
    check(end==name+8,"eight-digit console layout identifier");
    layout={name,static_cast<DWORD>(n&0xffff)};return true;
}
ConsoleLayout consoleLayout()
{
    ConsoleLayout layout;check(queryConsoleLayout(layout),"actual console keyboard layout");return layout;
}
std::wstring layoutLabel(const ConsoleLayout &layout)
{
    return (layout.language==0x409 ? L"US" : layout.language==0x41a ? L"Croatian" : L"Other")+
        std::wstring(L" (")+layout.id+L")";
}
void waitForLayout(HANDLE input,DWORD expected)
{
    say(L"\r\nSelect "+std::wstring(expected==0x409 ? L"US" : L"Croatian")+
        L" on this console, then press Enter to begin. F12 stops the check.\r\n");
    const auto deadline=GetTickCount64()+180000;
    std::wstring previous;
    while(GetTickCount64()<deadline) {
        ConsoleLayout current;
        if(queryConsoleLayout(current) && current.id!=previous){
            say(L"Current layout: "+layoutLabel(current)+L"\r\n");previous=current.id;
        }
        if(WaitForSingleObject(input,100)!=WAIT_OBJECT_0)continue;
        INPUT_RECORD record={};DWORD read=0;
        check(ReadConsoleInputW(input,&record,1,&read) && read==1,"layout confirmation key");
        if(record.EventType!=KEY_EVENT || !record.Event.KeyEvent.bKeyDown)continue;
        check(record.Event.KeyEvent.wVirtualKeyCode!=VK_F12,"manual check stopped with F12");
        if(manualSynthetic && record.Event.KeyEvent.wVirtualKeyCode==VK_F2){manualSyntheticLayout=expected;continue;}
        if(record.Event.KeyEvent.wVirtualKeyCode!=VK_RETURN)continue;
        if(!queryConsoleLayout(current)){
            say(L"The console has not reported its layout. Select the required layout and press Enter again.\r\n");continue;
        }
        if(current.language!=expected){say(L"That is not the required layout. Switch layouts and press Enter again.\r\n");continue;}
        writeFile(outRoot+L"\\layout-ready.json","{\"LayoutId\":"+json(utf8(current.id))+
            ",\"LayoutLanguage\":"+std::to_string(current.language)+",\"Confirmed\":true}");
        return;
    }
    check(false,"keyboard layout confirmation deadline");
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
struct InputPreview
{
    std::wstring line;
    std::wstring update(const KEY_EVENT_RECORD &key)
    {
        if(!key.bKeyDown)return {};
        const auto c=key.uChar.UnicodeChar;
        if(c==L'\b'){if(line.empty())return {};line.pop_back();return L"\b \b";}
        if(c==L'\r')return L"\r\n";
        if(c==3)return L"^C\r\n";
        if(c>=32){line+=c;return std::wstring(1,c);}
        return {};
    }
};
void manualDisplayOracle()
{
    manualEncodingOracle();
    InputPreview p;KEY_EVENT_RECORD k={};k.bKeyDown=TRUE;k.uChar.UnicodeChar=L'A';
    check(p.update(k)==L"A" && p.line==L"A","visible input before Enter");
    k.uChar.UnicodeChar=L'\b';check(p.update(k)==L"\b \b" && p.line.empty(),"visible correction");
    check(p.update(k).empty(),"backspace cannot erase the prompt");
    k.uChar.UnicodeChar=L'\u010d';check(p.update(k)==L"\u010d","visible Croatian input");
    k.bKeyDown=FALSE;check(p.update(k).empty(),"no duplicate key-up preview");
    k.bKeyDown=TRUE;k.uChar.UnicodeChar=3;check(p.update(k)==L"^C\r\n","visible Ctrl+C");
    check(visible(L"NEXT\r\n")==L"NEXT<CR><LF>","unambiguous backend readback");
    printf("Manual runner oracle: 11 checks passed.\n");
}
struct PhysicalInput
{
    std::string records="[",forwarded;
    bool shift=false,altGr=false,deadPrefix=false,layoutMatched=true;
    unsigned int count=0;
    DWORD layout=0;
};
void physicalRecord(const std::wstring &path,const PhysicalInput &input,bool complete)
{
    writeFile(path,"{\"Source\":"+json(manualSynthetic ? "OwnedConsoleSyntheticSmoke":"PhysicalConsoleInput")+
        ",\"SyntheticInputUsed\":"+boolean(manualSynthetic)+",\"Complete\":"+boolean(complete)+
        ",\"LayoutLanguage\":"+std::to_string(input.layout)+",\"AllTextUsedRequiredLayout\":"+
        boolean(input.layoutMatched)+",\"ShiftObserved\":"+boolean(input.shift)+
        ",\"AltGrObserved\":"+boolean(input.altGr)+",\"DeadPrefixObserved\":"+boolean(input.deadPrefix)+
        ",\"ForwardedUtf8\":"+json(input.forwarded)+",\"Keys\":"+input.records+"]}");
}
PhysicalInput bridge(Session &session,HANDLE input,int stop,DWORD required,const std::wstring &path)
{
    PhysicalInput result;InputPreview preview;
    const auto deadline=GetTickCount64()+180000;
    say(L"Typed input (local preview): ");
    bool done=false;
    while(!done && GetTickCount64()<deadline) {
        session.pump();
        if(WaitForSingleObject(input,100)!=WAIT_OBJECT_0)continue;
        INPUT_RECORD record={};DWORD read=0;
        check(ReadConsoleInputW(input,&record,1,&read) && read==1,"physical keyboard record");
        if(record.EventType!=KEY_EVENT)continue;
        const auto &key=record.Event.KeyEvent;
        const auto layout=consoleLayout();
        check(++result.count<=512,"bounded physical keyboard evidence");
        if(result.count>1)result.records+=',';
        result.records+="{\"Down\":"+boolean(key.bKeyDown!=FALSE)+",\"VirtualKey\":"+std::to_string(key.wVirtualKeyCode)+
            ",\"ScanCode\":"+std::to_string(key.wVirtualScanCode)+",\"Character\":"+std::to_string(key.uChar.UnicodeChar)+
            ",\"Modifiers\":"+std::to_string(key.dwControlKeyState)+",\"Repeat\":"+std::to_string(key.wRepeatCount)+
            ",\"LayoutId\":"+json(utf8(layout.id))+"}";
        if(key.bKeyDown && key.uChar.UnicodeChar) {
            if(!result.layout)result.layout=layout.language;
            result.layoutMatched=result.layoutMatched && layout.language==required;
        }
        if(key.bKeyDown) {
            const auto bytes=keyBytes(key);
            for(WORD repeat=0;repeat<key.wRepeatCount;++repeat) {
                if(!bytes.empty()){session.input(bytes);result.forwarded+=bytes;}
                const auto echo=preview.update(key);if(!echo.empty())say(echo);
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
            done=(stop==1 && key.wVirtualKeyCode==VK_RETURN) ||
                 (stop==2 && key.uChar.UnicodeChar==3) ||
                 (stop==3 && key.uChar.UnicodeChar==L'z');
        }
        physicalRecord(path,result,done);
        check(!key.bKeyDown || key.wVirtualKeyCode!=VK_F12,"manual check stopped with F12");
    }
    check(done,"physical keyboard step deadline");
    check(result.layoutMatched,"required keyboard layout throughout typed input");
    return result;
}
void exactManualRead(Session &s,const std::wstring &expected,const std::wstring &path,uint32_t events)
{
    IdentityPacket p;
    check(s.until([&]{p=s.observe();return p.readReturned!=0;},5000),"manual read returned");
    writeFile(path,packetJson(p));
    say(L"Backend returned: "+visible(std::wstring(p.text,p.textCount))+L"\r\n");
    if(std::wstring(p.text,p.textCount)!=expected+L"\r\n")
        say(L"Expected: "+visible(expected+L"\r\n")+L"\r\n");
    check(p.readSucceeded && !p.readError && p.controlCount==events &&
        std::wstring(p.text,p.textCount)==expected+L"\r\n","exact manual line");
}
int runKeyboard(const std::wstring &out,const std::wstring &layout,bool synthetic=false)
{
    manualSynthetic=synthetic;
    manualSyntheticLayout=layout==L"US" ? 0x41au:0x409u;
    check(layout==L"US" || layout==L"HR","manual layout US or HR");
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new keyboard evidence");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"manual keyboard trace");
    ManualConsole console;
    const DWORD expectedLayout=layout==L"US" ? 0x409u:0x41au;
    say(L"\r\nH4 keyboard check: "+layout+L". Physical input is required for qualification.\r\n"
        L"The local preview shows your keys. Backend readback is checked separately.\r\n");
    if(synthetic)say(L"SYNTHETIC SMOKE TEST. This is not physical keyboard acceptance.\r\n");
    waitForLayout(console.input,expectedLayout);
    Collector collector(out);Context context(out+L"\\session");auto &s=context.start();
    const std::wstring expected=layout==L"US" ? L"AZaz09[]{}\\|" : L"\u010d\u0107\u0111\u0161\u017e \u010c\u0106\u0110\u0160\u017d @[]{}\\|";
    startRead(s,"");
    say(L"Type this line exactly, using physical Shift/AltGr keys as needed, then press Enter:\r\n"+expected+L"\r\n");
    const auto text=bridge(s,console.input,1,expectedLayout,out+L"\\text-keys.json");
    exactManualRead(s,expected,out+L"\\text-read.json",0);
    check(text.layout==expectedLayout && text.shift,"physical layout and Shift");
    if(layout==L"HR") {
        check(text.altGr,"physical Croatian AltGr");startRead(s,"");
        say(L"\r\nUse a dead-key composition to type \u00e1, then press Enter.\r\n");
        const auto dead=bridge(s,console.input,1,expectedLayout,out+L"\\dead-keys.json");
        exactManualRead(s,L"\u00e1",out+L"\\dead-read.json",0);
        check(dead.layout==expectedLayout && dead.deadPrefix,"physical dead-key prefix");
    }
    startRead(s,"");say(L"\r\nType OLD, then press Ctrl+C without pressing Enter.\r\n");
    const auto cancel=bridge(s,console.input,2,expectedLayout,out+L"\\cancel-keys.json");
    check(cancel.forwarded=="OLD\x03","manual cancellation input sequence");
    auto p=waitRead(s,1);writeFile(out+L"\\canceled-read.json",packetJson(p));
    check(canceled(p,1),"physical cooked Ctrl+C");
    say(L"Backend canceled the line: error 995, empty text, one Ctrl+C event.\r\n");
    s.observe('R');say(L"\r\nType NEXT, then press Enter.\r\n");
    bridge(s,console.input,1,expectedLayout,out+L"\\next-keys.json");
    exactManualRead(s,L"NEXT",out+L"\\next-read.json",1);
    s.observe('U');say(L"\r\nRaw-input check: press Ctrl+C, release it, then type lowercase z. Do not press Enter.\r\n");
    bridge(s,console.input,3,expectedLayout,out+L"\\raw-keys.json");
    check(s.until([&]{p=s.observe();return p.readReturned!=0;},5000),"physical raw read");
    writeFile(out+L"\\raw-read.json",packetJson(p));
    check(p.controlCount==1 && p.readSucceeded && !p.readError && p.keyCount==3 &&
        p.keys[0].virtualKey==VK_CONTROL && p.keys[1].character==3 && p.keys[2].character=='z',
        "physical raw Ctrl+C has no control event");
    say(L"\r\nBackend raw records verified: Ctrl, C and z; no extra control event.\r\n");
    nextRead(s,context.out,1);
    check(context.finish(),"manual keyboard host cleanup");collector.stop();
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4ManualKeyboard\",\"SchemaVersion\":1,\"Status\":\"Pass\",\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"DeclaredLayout\":"+json(utf8(layout))+",\"LayoutLanguage\":"+
        std::to_string(expectedLayout)+",\"PhysicalInputRequired\":"+boolean(!synthetic)+
        manualMetadata()+",\"H4Accepted\":false}");
    say(L"\r\nKeyboard check passed. Keep the complete result directory.\r\n");
    return 0;
}
struct ManualSmokeDesktop
{
    HDESK original=nullptr,owned=nullptr;
    explicit ManualSmokeDesktop(bool enabled)
    {
        if(!enabled)return;
        original=GetThreadDesktop(GetCurrentThreadId());
        const auto name=L"VT7Pty-Manual-Smoke-"+GenRandom().randomHexString(16);
        const auto sd=h4host::privateDescriptor();SECURITY_ATTRIBUTES sa={sizeof(sa),sd.get(),FALSE};
        std::thread creator([&]{owned=CreateDesktopW(name.c_str(),nullptr,nullptr,0,
            DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS,&sa);});creator.join();
        check(owned!=nullptr,"owned smoke desktop");
    }
    void enter(){check(SetThreadDesktop(owned)!=FALSE,"smoke caller desktop association");}
    void restore(){if(original)check(SetThreadDesktop(original)!=FALSE,"restore smoke caller desktop");}
    ~ManualSmokeDesktop(){if(original)SetThreadDesktop(original);if(owned)CloseDesktop(owned);}
};
int runSessionCheck(const std::wstring &out,bool synthetic=false)
{
    manualSynthetic=synthetic;
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new session-transition evidence");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"manual session trace");
    Collector collector(out);
    Context context(out+L"\\existing");auto &s=context.start();startRead(s,"SESSION_KEEP");
    if(synthetic)say(L"\r\nSynthetic caller-desktop recovery smoke. No workstation lock or input-desktop switch.\r\n");
    else say(L"\r\nH4 lock/unlock check. Save your work, then press Win+L and sign back in.\r\n"
        L"I do not lock, switch desktops, sign out, or change your Windows settings.\r\n"
        L"The pending test line stays in its private backing console. You have five minutes.\r\n");
    ManualSmokeDesktop smoke(synthetic);
    const auto deadline=GetTickCount64()+(synthetic ? 10000:300000);
    bool inactive=false,recovered=false,denied=false;
    std::string states="[",previous;
    unsigned int changes=0;
    while(GetTickCount64()<deadline) {
        const auto snapshot=h4host::desktopState();
        const auto description=desktopJson();
        const auto key=snapshot.caller+L":"+snapshot.input+L":"+std::to_wstring(snapshot.error);
        if(utf8(key)!=previous) {
            if(changes++)states+=',';
            states+=description;previous=utf8(key);
            check(changes<=128,"bounded desktop transition evidence");
        }
        if(synthetic && !inactive && changes==1){smoke.enter();continue;}
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
            if(synthetic)smoke.restore();
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
        "\"PendingReadPreserved\":true,\"DesktopSwitchedByTest\":false"+manualMetadata()+
        ",\"CallerThreadDesktopChangedBySmoke\":"+boolean(synthetic)+",\"H4Accepted\":false}");
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
        ",\"FlaglessOpenPassed\":true,\"ObservedHostExited\":true"+manualMetadata()+",\"H4Accepted\":false}");
    printf("Elevated-caller denial check passed.\n");
    return 0;
}
