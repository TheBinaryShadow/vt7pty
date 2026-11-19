// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
namespace profilecases {
const std::array<const wchar_t *,8> manualNames={L"Startup",L"Editing",L"History",L"Completion",L"Multiline",L"Cancellation",L"Native child",L"Resize"};
std::wstring instruction(unsigned stage) {
    const std::array<const wchar_t *,9> text={
        L"Complete normal startup prompts. At a fresh prompt, F8 records profile/editor metadata.",
        L"F2 inserts a deliberate EDXT typo. Change X to I using arrows/Backspace, End, Enter, then F8.",
        L"F2, Enter; then Up, Enter to recall the command. At the fresh prompt press F8.",
        L"F2 inserts a command prefix. Use Tab to complete it, Enter, then F8.",
        L"F2, Enter; type first\u010d, Enter, second\u017e, Enter, '@, Enter, blank Enter, then F8.",
        L"F2 inserts a line. Do NOT press Enter. Ctrl+C, then F8 at the fresh prompt.",
        L"F2, Enter; wait for PROFILE_CHILD_READY, Ctrl+C, then F8 at the parent prompt.",
        L"Resize this console window. F2, Enter, then F8 at the fresh prompt.",
        L"All checks recorded. Type exit 37 and press Enter to finish naturally."};
    return text[std::min<size_t>(stage,8)];
}
struct View {
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE),previous=GetStdHandle(STD_OUTPUT_HANDLE);Handle output;
    DWORD mode=0;
    View(){
        check(GetConsoleMode(input,&mode),"profile manual classic-console input");
        output.reset(CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CONSOLE_TEXTMODE_BUFFER,nullptr));
        check(output.value!=INVALID_HANDLE_VALUE,"private profile display buffer");
        check(SetConsoleScreenBufferSize(output.value,{160,60}),"profile display capacity");
        SMALL_RECT window={0,0,79,32};const auto maximum=GetLargestConsoleWindowSize(output.value);window.Bottom=static_cast<SHORT>(std::min<int>(32,maximum.Y-1));
        check(window.Bottom>=19 && SetConsoleWindowInfo(output.value,TRUE,&window),"profile display geometry");
        check(SetConsoleMode(input,ENABLE_EXTENDED_FLAGS|ENABLE_WINDOW_INPUT),"profile manual input mode");
        if(!SetConsoleActiveScreenBuffer(output.value)){const auto failure=GetLastError();SetConsoleMode(input,mode);SetLastError(failure);check(false,"profile live display activation");}
    }
    ~View(){SetConsoleMode(input,mode);SetConsoleActiveScreenBuffer(previous);}
    std::pair<int,int> size(){CONSOLE_SCREEN_BUFFER_INFO info={};check(GetConsoleScreenBufferInfo(output.value,&info),"profile display geometry read");return {info.srWindow.Right-info.srWindow.Left+1,info.srWindow.Bottom-info.srWindow.Top+1};}
    void draw(const Session &s,unsigned stage,bool synthetic){
        const auto [width,height]=size();std::vector<CHAR_INFO> cells(static_cast<size_t>(width*height));
        for(auto &c:cells){c.Char.UnicodeChar=L' ';c.Attributes=7;}
        auto line=[&](int y,const std::wstring &value,WORD attr=15){if(y<0||y>=height)return;for(size_t x=0;x<value.size()&&x<static_cast<size_t>(width);++x){auto &c=cells[static_cast<size_t>(y*width)+x];c.Char.UnicodeChar=value[x];c.Attributes=attr;}};
        line(0,synthetic?L"PROFILE MANUAL RUNNER: SYNTHETIC SMOKE, NEVER QUALIFICATION":L"VT7Pty ordinary PowerShell profiles: private live display");
        line(1,L"F2 inserts test input | F8 verifies and advances | F12 stops without accepting",11);
        const int pane=height-6,top=std::max(0,s.state.y()-pane+1);
        for(int y=0;y<pane && y+top<s.state.rows();++y)for(int x=0;x<width&&x<s.state.columns();++x){
            const auto &v=s.state.at(x,y+top);auto &c=cells[static_cast<size_t>((y+3)*width+x)];c.Attributes=v.attributes;
            c.Char.UnicodeChar=v.text.empty()?L' ':v.text[0]<=0xffff?static_cast<wchar_t>(v.text[0]):L'\ufffd';
        }
        line(height-2,L"Step "+std::to_wstring(std::min(stage+1,8u))+L"/8: "+(stage<8?manualNames[stage]:L"Natural exit"),14);
        auto guide=instruction(stage);line(height-1,guide.substr(0,static_cast<size_t>(width)),14);
        if(guide.size()>static_cast<size_t>(width))line(2,guide.substr(static_cast<size_t>(width)),14);
        SMALL_RECT rect={0,0,static_cast<SHORT>(width-1),static_cast<SHORT>(height-1)};
        check(WriteConsoleOutputW(output.value,cells.data(),{static_cast<SHORT>(width),static_cast<SHORT>(height)},{0,0},&rect),"profile live display write");
        if(synthetic){
            std::vector<CHAR_INFO> observed(cells.size());rect={0,0,static_cast<SHORT>(width-1),static_cast<SHORT>(height-1)};
            check(ReadConsoleOutputW(output.value,observed.data(),{static_cast<SHORT>(width),static_cast<SHORT>(height)},{0,0},&rect),"owned display readback");
            for(size_t i=0;i<cells.size();++i)check(cells[i].Char.UnicodeChar==observed[i].Char.UnicodeChar&&cells[i].Attributes==observed[i].Attributes,"owned display cells match live rendering");
        }
        const int y=s.state.y()-top+3;if(y<height-2)SetConsoleCursorPosition(output.value,{static_cast<SHORT>(s.state.x()),static_cast<SHORT>(y)});
    }
};
std::wstring starter(const Shell &shell,unsigned stage){
    const auto fn=shell.checkName();
    switch(stage){
    case 1:return fn+L" Editing 'EDXT \u010d\u0107\u0111\u0161\u017e'";
    case 2:return fn+L" History 'HISTORY_OK'";
    case 3:return L"VT7PtyProfileComple";
    case 4:return fn+L" Multiline @'";
    case 5:return fn+L" Cancelled 'SHOULD_NOT_RUN'";
    case 6:return L"VT7PtyProfileChild_"+wide(shell.nonce);
    case 7:return fn+L" Resize 'RESIZE_OK'";
    default:return {};
    }
}
std::string physicalBytes(const KEY_EVENT_RECORD &key){
    if(!key.bKeyDown)return {};
    if(key.uChar.UnicodeChar)return utf8(std::wstring(1,key.uChar.UnicodeChar));
    switch(key.wVirtualKeyCode){case VK_INSERT:return "\x1b[2~";case VK_PRIOR:return "\x1b[5~";case VK_NEXT:return "\x1b[6~";default:return keyBytes(key);}
}
void sendOwnConsoleKey(HANDLE input,WORD vk,wchar_t ch=0){
    INPUT_RECORD records[2]={};records[0].EventType=KEY_EVENT;auto &key=records[0].Event.KeyEvent;
    key.bKeyDown=TRUE;key.wRepeatCount=1;key.wVirtualKeyCode=vk;key.uChar.UnicodeChar=ch;
    key.wVirtualScanCode=static_cast<WORD>(MapVirtualKeyW(vk,MAPVK_VK_TO_VSC));
    if(vk==VK_LEFT||vk==VK_RIGHT||vk==VK_UP||vk==VK_DOWN||vk==VK_END)key.dwControlKeyState=ENHANCED_KEY;
    records[1]=records[0];records[1].Event.KeyEvent.bKeyDown=FALSE;DWORD count=0;
    check(WriteConsoleInputW(input,records,2,&count)&&count==2,"owned synthetic console input");
}
int manual(const std::wstring &out,bool pwsh,bool synthetic=false){
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new ordinary profile result");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L""),"no ordinary profile trace capture");
    Context context(out+L"\\session");bool okay=false;std::string failure;
    unsigned stage=0,keysSeen=0,resizes=0;std::array<unsigned,256> physical={};std::vector<std::array<unsigned,256>> checkpoints;
    std::string resizeRows="[",stageRows="[";
    try{
        auto &s=context.start();Shell shell(context,{pwsh?"ordinary-pwsh":"ordinary-wps",pwsh,false},true,synthetic);
        View view;std::pair<int,int> previous={80,25};bool helpers=false,smokeCheckpointQueued=false;
        auto verify=[&]{
            check(stage<8,"all profile checkpoints already recorded");
            if(!helpers){const auto code=flattened(probeSource(context.out,shell.nonce,false));check(code.size()<7900,"bounded interactive helper definition");s.input(utf8(code+L"\r"));helpers=true;}
            if(stage==1){shell.proof(L"Editing");check(physical[VK_BACK]&&physical[VK_LEFT]&&physical[VK_END],"physical editing keys observed");}
            if(stage==2){const auto data=shell.proof(L"History");check(number(data,"CheckInvocation")>=2&&physical[VK_UP],"physical history recall");}
            if(stage==3){shell.proof(L"Completion");check(physical[VK_TAB],"physical completion key");}
            if(stage==4)shell.proof(L"Multiline");
            if(stage==5){check(physical['C']||synthetic,"physical prompt Ctrl+C");
                check(GetFileAttributesW((context.out+L"\\Cancelled.json").c_str())==INVALID_FILE_ATTRIBUTES,"ordinary canceled text not executed");
                s.input(utf8(shell.checkName()+L" Continue 'CONTINUE_OK'\r"));
                check(s.until([&]{return GetFileAttributesW((context.out+L"\\Continue.json").c_str())!=INVALID_FILE_ATTRIBUTES;}),"ordinary next command after cancellation");shell.proof(L"Continue");
            }
            if(stage==6){shell.childProof();check(physical['C']||synthetic,"physical child Ctrl+C");}
            if(stage==7){shell.proof(L"Resize");check(resizes>0,"physical console resize observed");}
            const auto snapshot=shell.snapshot(static_cast<int>(stage),true);
            if(stage==0)check(number(snapshot,"StartupErrorCount")==0,"ordinary startup errors need review");
            if(stageRows.size()>1)stageRows+=',';
            stageRows+="{\"Stage\":"+std::to_string(stage)+",\"Confirmed\":true,\"Source\":"+json(synthetic?"OwnedConsoleSyntheticSmoke":"PhysicalConsoleInput")+"}";
            checkpoints.push_back(physical);physical={};smokeCheckpointQueued=false;++stage;
        };
        const auto deadline=GetTickCount64()+1500000;unsigned smokePhase=0;ULONGLONG smokeNext=0;bool smokeChildInterrupted=false;
        while(GetTickCount64()<deadline && WaitForSingleObject(s.descendant.value,0)==WAIT_TIMEOUT){
            s.pump();const auto dimensions=view.size();const auto backend=std::pair<int,int>{std::clamp(dimensions.first,20,160),std::clamp(dimensions.second-6,10,54)};
            if(backend!=previous){
                check(vt7pty_set_size(s.runtimeHandle(),backend.first,backend.second,nullptr),"ordinary profile resize");s.state.resize(backend.first,backend.second);previous=backend;
                if(stage==7){++resizes;if(resizeRows.size()>1)resizeRows+=',';resizeRows+="{\"Columns\":"+std::to_string(backend.first)+",\"Rows\":"+std::to_string(backend.second)+"}";}
            }
            view.draw(s,stage,synthetic);
            if(shell.nativeChild.value==INVALID_HANDLE_VALUE && GetFileAttributesW((context.out+L"\\child-ready.json").c_str())!=INVALID_FILE_ATTRIBUTES){shell.retainChild();}
            if(synthetic && GetTickCount64()>=smokeNext){
                // Smoke uses the same renderer, probe, key loop and checks. It
                // never loads user profiles and cannot qualify a manual run.
                if(stage==0){if(!s.state.contains(U"SMOKE[")){Sleep(10);continue;}if(!smokeCheckpointQueued){sendOwnConsoleKey(view.input,VK_F8);smokeCheckpointQueued=true;}smokeNext=GetTickCount64()+150;}
                else if(stage==1 && smokePhase<1){
                    sendOwnConsoleKey(view.input,VK_F2);for(int n=0;n<8;++n)sendOwnConsoleKey(view.input,VK_LEFT);
                    sendOwnConsoleKey(view.input,VK_BACK,L'\b');sendOwnConsoleKey(view.input,'I',L'I');sendOwnConsoleKey(view.input,VK_END);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');
                    smokePhase=1;smokeNext=GetTickCount64()+500;
                }
                else if(stage==2 && smokePhase<2){
                    sendOwnConsoleKey(view.input,VK_F2);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');sendOwnConsoleKey(view.input,VK_UP);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');
                    smokePhase=2;smokeNext=GetTickCount64()+500;
                }
                else if(stage==3 && smokePhase<3){
                    sendOwnConsoleKey(view.input,VK_F2);sendOwnConsoleKey(view.input,VK_TAB,L'\t');sendOwnConsoleKey(view.input,VK_RETURN,L'\r');smokePhase=3;smokeNext=GetTickCount64()+500;
                }
                else if(stage==4 && smokePhase<4){
                    sendOwnConsoleKey(view.input,VK_F2);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');
                    for(const auto ch:std::wstring(L"first\u010d\rsecond\u017e\r'@\r\r"))sendOwnConsoleKey(view.input,ch==L'\r'?VK_RETURN:0,ch);
                    smokePhase=4;smokeNext=GetTickCount64()+700;
                }
                else if(stage==5 && smokePhase<5){sendOwnConsoleKey(view.input,VK_F2);smokePhase=5;smokeNext=GetTickCount64()+150;}
                else if(stage==5 && !physical['C']){sendOwnConsoleKey(view.input,'C',3);smokeNext=GetTickCount64()+500;}
                else if(stage==6 && smokePhase<6){sendOwnConsoleKey(view.input,VK_F2);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');smokePhase=6;smokeNext=GetTickCount64()+300;}
                else if(stage==6 && shell.nativeChild.value==INVALID_HANDLE_VALUE){smokeNext=GetTickCount64()+50;}
                else if(stage==6 && !smokeChildInterrupted){sendOwnConsoleKey(view.input,'C',3);smokeChildInterrupted=true;smokeNext=GetTickCount64()+300;}
                else if(stage==6 && WaitForSingleObject(shell.nativeChild.value,0)==WAIT_TIMEOUT){smokeNext=GetTickCount64()+50;}
                else if(stage==7 && smokePhase<7){
                    SMALL_RECT window={0,0,95,32};check(SetConsoleWindowInfo(view.output.value,TRUE,&window),"owned smoke resize");
                    sendOwnConsoleKey(view.input,VK_F2);sendOwnConsoleKey(view.input,VK_RETURN,L'\r');smokePhase=7;smokeNext=GetTickCount64()+500;
                }
                else if(stage==8){s.input("exit 37\r");smokeNext=GetTickCount64()+1000;}
                else{if(!smokeCheckpointQueued){sendOwnConsoleKey(view.input,VK_F8);smokeCheckpointQueued=true;}smokeNext=GetTickCount64()+150;}
            }
            if(WaitForSingleObject(view.input,25)!=WAIT_OBJECT_0)continue;
            INPUT_RECORD record={};DWORD read=0;check(ReadConsoleInputW(view.input,&record,1,&read)&&read==1,"ordinary profile physical input");
            if(record.EventType!=KEY_EVENT||!record.Event.KeyEvent.bKeyDown)continue;const auto &key=record.Event.KeyEvent;
            check(++keysSeen<20000,"bounded ordinary profile key count");
            if(key.wVirtualKeyCode==VK_F12)throw std::runtime_error("ordinary profile check stopped");
            if(key.wVirtualKeyCode==VK_F8){verify();continue;}
            if(key.wVirtualKeyCode==VK_F2){s.input(utf8(starter(shell,stage)));continue;}
            if(key.uChar.UnicodeChar==3)physical['C']+=key.wRepeatCount;
            else if(key.wVirtualKeyCode==VK_BACK||key.wVirtualKeyCode==VK_LEFT||key.wVirtualKeyCode==VK_RIGHT||key.wVirtualKeyCode==VK_HOME||
                key.wVirtualKeyCode==VK_END||key.wVirtualKeyCode==VK_UP||key.wVirtualKeyCode==VK_TAB||key.wVirtualKeyCode==VK_RETURN)physical[key.wVirtualKeyCode]+=key.wRepeatCount;
            const auto bytes=physicalBytes(key);for(WORD n=0;n<key.wRepeatCount;++n)if(!bytes.empty())s.input(bytes);
        }
        DWORD exit=0;check(stage==8&&GetExitCodeProcess(s.descendant.value,&exit)&&exit==37,"ordinary profile complete workflow and natural exit");
        writeFile(context.out+L"\\profile-result.json","{\"ShellId\":"+std::to_string(shell.pid)+",\"NaturalExit\":37,\"WorkflowsPassed\":true}");
        okay=true;
    }catch(const std::exception &e){failure=e.what();}
    okay=context.finish()&&okay;if(failure.empty()&&!context.error.empty())failure=context.error;
    std::string counts="[";for(size_t i=0;i<checkpoints.size();++i){if(i)counts+=',';counts+="{";bool first=true;for(size_t vk=0;vk<256;++vk)if(checkpoints[i][vk]){if(!first)counts+=',';first=false;counts+=json(std::to_string(vk))+":"+std::to_string(checkpoints[i][vk]);}counts+='}';}
    writeFile(out+L"\\physical-checks.json","{\"Stages\":"+stageRows+"],\"KeyCountsByStage\":"+counts+"],\"PrintableTextCollected\":false,\"Resizes\":"+resizeRows+"]}");
    writeFile(out+L"\\probe.json","{\"Kind\":\"OrdinaryProfileProbe\",\"SchemaVersion\":1,\"Status\":"+json(okay?"Pass":"Fail")+
        ",\"SourceCommit\":"+json(GenVersion_Commit)+",\"RuntimeSourceCommit\":"+json(manualRuntimeSource())+",\"SyntheticInputUsed\":"+boolean(synthetic)+
        ",\"OrdinaryProfilesLoaded\":"+boolean(!synthetic)+",\"ProfileContentCollected\":false,\"ShellOutputCollected\":false,\"HistoryContentCollected\":false,\"Stages\":"+std::to_string(stage)+
        ",\"OwnedProcessesCleaned\":"+boolean(context.cleaned)+",\"HostExited\":"+boolean(context.hostExited)+",\"Error\":"+json(failure)+",\"Step12Accepted\":false}");
    printf("Ordinary profile %s: %s\n",synthetic?"synthetic smoke":"physical check",okay?"PASS":"FAIL");return okay?0:1;
}
int smoke(const std::wstring &out,bool pwsh){
    const auto exe=currentExe();auto command=quote(exe)+L" --profiles-manual-smoke-worker "+(pwsh?L"pwsh ":L"wps ")+quote(out);
    std::vector<wchar_t> line(command.begin(),command.end());line.push_back(0);STARTUPINFOW startup={};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process={};check(CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,nullptr,nullptr,&startup,&process),"owned hidden profile smoke console");
    Handle childProcess(process.hProcess),thread(process.hThread);
    if(WaitForSingleObject(childProcess.value,180000)!=WAIT_OBJECT_0){TerminateProcess(childProcess.value,124);WaitForSingleObject(childProcess.value,10000);throw std::runtime_error("owned manual smoke deadline");}
    DWORD code=1;check(GetExitCodeProcess(childProcess.value,&code),"manual smoke status");return static_cast<int>(code);
}
}
