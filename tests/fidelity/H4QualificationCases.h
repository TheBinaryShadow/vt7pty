// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "H4TestGate.h"
namespace h4host = vt7pty::internal::host;
std::string desktopJson()
{
    const auto state=h4host::desktopState();
    h4host::Token token(GetCurrentProcess());
    HANDLE impersonation=nullptr;
    const BOOL impersonated=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&impersonation);
    const DWORD tokenError=impersonated ? ERROR_SUCCESS : GetLastError();
    if(impersonation) CloseHandle(impersonation);
    const bool eligible=state.eligible() && token.integrity==SECURITY_MANDATORY_MEDIUM_RID &&
        !impersonated && tokenError==ERROR_NO_TOKEN;
    return "{\"Kind\":\"H4DesktopPreflight\",\"Eligible\":"+boolean(eligible)+
        ",\"WindowStation\":"+json(utf8(state.station))+",\"CallerDesktop\":"+json(utf8(state.caller))+
        ",\"InputDesktop\":"+json(utf8(state.input))+",\"InputDesktopError\":"+std::to_string(state.error)+
        ",\"SourceCommit\":"+json(GenVersion_Commit)+
        ",\"IntegrityRid\":"+std::to_string(token.integrity)+",\"Impersonated\":"+boolean(impersonated!=FALSE)+
        ",\"TokenError\":"+std::to_string(tokenError)+",\"ProcessId\":"+std::to_string(GetCurrentProcessId())+
        ",\"Tick\":"+h3trace::u64(GetTickCount64())+"}";
}
bool desktopPreflight(const std::wstring &path)
{
    const auto text=desktopJson();
    writeFile(path,text);
    return text.find("\"Eligible\":true")!=std::string::npos;
}
struct ResourceSample
{
    DWORD handles=0,user=0,gdi=0;
    SIZE_T privateBytes=0;
    ResourceSample()
    {
        check(GetProcessHandleCount(GetCurrentProcess(),&handles),"resource handle sample");
        user=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
        gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        PROCESS_MEMORY_COUNTERS_EX memory={};memory.cb=sizeof(memory);
        check(K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                                      sizeof(memory)),"resource memory sample");
        privateBytes=memory.PrivateUsage;
    }
    std::string jsonText() const
    {
        return "{\"Handles\":"+std::to_string(handles)+",\"UserObjects\":"+std::to_string(user)+
            ",\"GdiObjects\":"+std::to_string(gdi)+",\"PrivateBytes\":"+h3trace::u64(privateBytes)+"}";
    }
};
void resourceBound(const ResourceSample &before,const ResourceSample &after)
{
    check(after.handles<=before.handles+8,"bounded caller handles");
    check(after.user<=before.user+2 && after.gdi<=before.gdi+2,"bounded caller GUI resources");
    check(after.privateBytes<=before.privateBytes+64ull*1024*1024,"bounded caller retained memory");
}
void inactiveDesktop(Context &context)
{
    const auto before=desktopJson();
    check(before.find("\"Eligible\":true")!=std::string::npos,"active desktop before denial");
    const auto name=L"VT7Pty-H4-Inactive-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(GetTickCount64());
    std::string observation,error;
    HDESK createdDesktop=nullptr;
    std::thread worker([&]{
        HDESK desk=nullptr;
        try {
            const auto descriptor=h4host::privateDescriptor();
            SECURITY_ATTRIBUTES sa={sizeof(sa),descriptor.get(),FALSE};
            desk=CreateDesktopW(name.c_str(),nullptr,nullptr,0,
                DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS,&sa);
            createdDesktop=desk;
            check(desk!=nullptr && SetThreadDesktop(desk),"owned noninput desktop");
            observation=desktopJson();
            auto *config=vt7pty_config_new(VT7PTY_FLAG_HOST_CANCELLATION,nullptr);
            vt7pty_error_ptr_t failure=nullptr;
            auto *pty=vt7pty_open(config,&failure);
            const auto message=failure ? utf8(vt7pty_error_msg(failure)) : "";
            const auto code=vt7pty_error_code(failure);
            vt7pty_config_free(config);vt7pty_error_free(failure);
            if(pty) vt7pty_free(pty);
            observation="{\"Preflight\":"+observation+",\"Denied\":"+boolean(!pty)+
                ",\"ErrorCode\":"+std::to_string(code)+",\"Message\":"+json(message)+
                ",\"DesktopSwitched\":false}";
            check(!pty && code && message.find("ordinary caller desktop")!=std::string::npos &&
                message.find("caller="+utf8(name))!=std::string::npos,"specific noninput desktop refusal");
        } catch(const Exception &e){error=utf8(e.what());}
          catch(const std::exception &e){error=e.what();}
        // The assigned desktop handle is closed by a separate thread after this worker exits.
    });
    worker.join();
    if(createdDesktop) check(CloseDesktop(createdDesktop),"release inactive desktop after worker exit");
    writeFile(context.out+L"\\denial.json",observation);
    check(error.empty(),"inactive desktop experiment");
    check(desktopPreflight(context.out+L"\\recovery-preflight.json"),"ordinary desktop recovery");
    cooked(context,"desktop-recovery");
}
void concurrentResources(Context &context)
{
    const ResourceSample before;
    {
        std::vector<std::unique_ptr<Context>> sessions;
        for(unsigned int i=0;i<8;++i) {
            auto item=std::make_unique<Context>(context.out+L"\\session-"+std::to_wstring(i));
            auto &s=item->start();startRead(s,"concurrent");
            sessions.push_back(std::move(item));
        }
        writeFile(context.out+L"\\peak.json",ResourceSample{}.jsonText());
        for(const auto &item:sessions)
            check(WaitForSingleObject(item->session->host.value,0)==WAIT_TIMEOUT &&
                  WaitForSingleObject(item->session->child.value,0)==WAIT_TIMEOUT,
                  "all eight retained hosts and clients live together");
        // The diagnostic channel is intentionally bounded and lossy. Keep all
        // sessions live, but sequence dispatches so every keyboard record survives.
        for(unsigned int i=0;i<8;++i) {
            auto &c=*sessions[i];auto &s=*c.session;
            s.input("\x03");
            const auto p=waitRead(s,1);
            writeFile(c.out+L"\\cancel-1.json",packetJson(p));
            check(canceled(p,1),"isolated multi-session cancellation");
            nextRead(s,c.out,1);
            check(vt7pty_set_size(s.runtimeHandle(),81+static_cast<int>(i),26,nullptr),
                  "multi-session resize");
        }
        for(const auto &item:sessions)check(item->finish(),"multi-session host cleanup");
    }
    const ResourceSample after;
    writeFile(context.out+L"\\resources.json","{\"ConcurrentSessions\":8,\"DispatchSequencedForCompleteTrace\":true,\"Before\":"+before.jsonText()+
        ",\"After\":"+after.jsonText()+",\"HandleBudget\":8,\"GuiBudget\":2,\"PrivateByteBudget\":67108864}");
    resourceBound(before,after);
}
struct TestGate
{
    Handle mapping,ready,entered,release,finished,stop;
    h4gate::State *state=nullptr;
    std::thread installer;
    std::string error;
    std::wstring name;
    HDESK installerDesktop=nullptr;
    TestGate(Session &s,const Desktop &desktop,unsigned int mode)
    {
        try {
        const auto id=h4host::objectName(GetThreadDesktop(GetCurrentThreadId()));
        (void)id;
        wchar_t random[33]={};
        const auto hex=GenRandom().randomHexString(16);
        check(hex.size()==32,"gate nonce size");
        wcscpy_s(random,hex.c_str());
        name=std::wstring(h4gate::prefix)+random;
        const auto full=L"Local\\"+name;
        const auto sd=h4host::privateDescriptor();
        SECURITY_ATTRIBUTES sa={sizeof(sa),sd.get(),FALSE};
        SetLastError(ERROR_SUCCESS);
        mapping.reset(CreateFileMappingW(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(h4gate::State),full.c_str()));
        check(mapping.value && GetLastError()!=ERROR_ALREADY_EXISTS,"new private test mapping");
        state=static_cast<h4gate::State *>(MapViewOfFile(mapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(h4gate::State)));
        check(state!=nullptr,"test mapping view");
        new(state) h4gate::State{};
        state->mode=mode;state->controller=GetCurrentProcessId();state->controllerCreated=creation(GetCurrentProcess());
        state->host=GetProcessId(s.host.value);state->hostCreated=creation(s.host.value);
        state->agent=GetProcessId(s.agent.value);
        const auto p=s.observe();state->window=p.window;
        state->thread=0;
        // Console HWND queries can identify an agent-side proxy. Resolve the
        // actual host thread from two owned title events on the backing desktop.
        DWORD verifiedThread=0;
        HDESK lookupDesktop=nullptr;
        std::string lookupError;
        std::thread lookup([&] {
            struct Watch {HWND window;DWORD host,thread=0,count=0;};
            static thread_local Watch *current=nullptr;
            Watch watch{reinterpret_cast<HWND>(state->window),state->host};
            HWINEVENTHOOK events=nullptr;
            try {
                lookupDesktop=OpenDesktopW(desktop.name.c_str(),0,FALSE,
                    DESKTOP_READOBJECTS|DESKTOP_CREATEWINDOW|DESKTOP_WRITEOBJECTS);
                check(lookupDesktop && SetThreadDesktop(lookupDesktop),"gate lookup private desktop");
                current=&watch;
                events=SetWinEventHook(EVENT_OBJECT_NAMECHANGE,EVENT_OBJECT_NAMECHANGE,nullptr,
                    [](HWINEVENTHOOK,DWORD,HWND window,LONG object,LONG child,DWORD tid,DWORD) {
                        if(!current || current->window!=window || object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
                        Handle thread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,tid));
                        if(thread.value && GetProcessIdOfThread(thread.value)==current->host) {
                            current->thread=tid;++current->count;
                        }
                    },state->host,0,WINEVENT_OUTOFCONTEXT);
                check(events!=nullptr,"owned backing title hook");
                for(unsigned int pass=0;pass<2;++pass) {
                    watch.thread=0;s.observe('T');
                    const auto deadline=GetTickCount64()+1000;
                    while(!watch.thread && GetTickCount64()<deadline) {
                        MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
                        Sleep(1);
                    }
                    check(watch.thread && (!verifiedThread || verifiedThread==watch.thread),"repeatable retained host title thread");
                    verifiedThread=watch.thread;Sleep(20);
                }
                check(watch.count>=2 && watch.count<=128,"bounded owned title events");
            } catch(const std::exception &e) {lookupError=e.what();}
            if(events)UnhookWinEvent(events);current=nullptr;
        });
        lookup.join();
        if(lookupDesktop) CloseDesktop(lookupDesktop);
        writeFile(outRoot+L"\\gate-discovery-"+std::to_wstring(state->host)+L".json",
            "{\"Desktop\":"+json(utf8(desktop.name))+",\"Window\":"+h3trace::u64(state->window)+
            ",\"Host\":"+std::to_string(state->host)+",\"Thread\":"+std::to_string(verifiedThread)+
            ",\"Error\":"+json(lookupError)+"}");
        if(!lookupError.empty())throw std::runtime_error(lookupError);
        Handle thread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,verifiedThread));
        check(thread.value && GetProcessIdOfThread(thread.value)==state->host,"retained test gate host thread");
        state->thread=verifiedThread;
        const auto event=[&](const wchar_t *suffix) {
            SetLastError(ERROR_SUCCESS);
            HANDLE result=CreateEventW(&sa,TRUE,FALSE,(full+suffix).c_str());
            check(result && GetLastError()!=ERROR_ALREADY_EXISTS,"new private test gate event");
            return result;
        };
        ready.reset(event(L"-ready"));entered.reset(event(L"-entered"));release.reset(event(L"-release"));
        finished.reset(event(L"-finished"));stop.reset(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        check(stop.value!=nullptr,"test gate stop event");
        const auto desktopName=desktop.name;
        installer=std::thread([&,desktopName]{
            HDESK desk=nullptr;HMODULE library=nullptr;HHOOK installed=nullptr;ATOM atom=0;
            try {
                desk=OpenDesktopW(desktopName.c_str(),0,FALSE,DESKTOP_HOOKCONTROL|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS);
                installerDesktop=desk;
                check(desk && SetThreadDesktop(desk),"test gate installer desktop");
                library=LoadLibraryW((directory()+L"\\H4TestGate.dll").c_str());
                check(library!=nullptr,"load test-only gate module");
                const auto address=GetProcAddress(library,"H4TestGateCallback");
                HOOKPROC callback=nullptr;static_assert(sizeof(callback)==sizeof(address));memcpy(&callback,&address,sizeof(callback));
                check(callback!=nullptr,"test gate export");
                installed=SetWindowsHookExW(WH_CALLWNDPROC,callback,library,state->thread);
                check(installed!=nullptr,"owned-thread test gate");
                atom=GlobalAddAtomW(name.c_str());check(atom!=0,"test gate binding atom");
                DWORD_PTR result=0;
                check(SendMessageTimeoutW(reinterpret_cast<HWND>(state->window),RegisterWindowMessageW(h4gate::initName),
                    atom,state->controller,SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,1000,&result)!=0,"test gate binding notification");
                check(WaitForSingleObject(ready.value,1000)==WAIT_OBJECT_0 && state->initialized==1,"actual gate binding");
                WaitForSingleObject(stop.value,15000);
            }catch(const std::exception &e){error=e.what();SetEvent(ready.value);}
            if(installed) UnhookWindowsHookEx(installed);
            if(atom) GlobalDeleteAtom(atom);
            if(library) FreeLibrary(library);
        });
        check(WaitForSingleObject(ready.value,3000)==WAIT_OBJECT_0 && state->initialized==1 && error.empty(),
              "gate ready on retained host");
        } catch(...) { dispose(); throw; }
    }
    ~TestGate() { dispose(); }
    void dispose() noexcept
    {
        if(release.value && release.value!=INVALID_HANDLE_VALUE) SetEvent(release.value);
        if(stop.value && stop.value!=INVALID_HANDLE_VALUE) SetEvent(stop.value);
        if(installer.joinable()) installer.join();
        if(state) {UnmapViewOfFile(state);state=nullptr;}
        if(installerDesktop) {CloseDesktop(installerDesktop);installerDesktop=nullptr;}
    }
    void finishGate(){SetEvent(release.value);check(WaitForSingleObject(finished.value,6000)==WAIT_OBJECT_0,"gate completion");}
};
void callbackFailure(Context &context,unsigned int mode)
{
    auto &s=context.start();startRead(s,"timeout-line");
    TestGate gate(s,context.desktop,mode);
    const auto tracePath=std::filesystem::path(outRoot+L"\\runtime-trace.jsonl");
    const auto traceStart=static_cast<std::streamoff>(std::filesystem::file_size(tracePath));
    s.input("\x03");
    check(WaitForSingleObject(gate.entered.value,3000)==WAIT_OBJECT_0,"actual production callback gate entered");
    const auto begin=GetTickCount64();
    if(mode==3) check(TerminateProcess(s.agent.value,123),"terminate only the retained test agent");
    bool unavailable=false;
    uint64_t unavailableTick=0;
    if(mode!=3) {
        const auto deadline=GetTickCount64()+2500;
        const auto pidToken="\"pid\":"+std::to_string(GetProcessId(s.agent.value))+",";
        do {
            std::ifstream trace(tracePath,std::ios::binary);
            trace.seekg(traceStart);
            std::string line;
            while(std::getline(trace,line)) if(line.find(pidToken)!=std::string::npos &&
                line.find("host-cancel unavailable sequence=2 outcome=unknown retry=disabled")!=std::string::npos)
                {unavailable=true;unavailableTick=GetTickCount64();}
            if(!unavailable)Sleep(10);
        }while(!unavailable && GetTickCount64()<deadline);
    }
    const bool agentExited=WaitForSingleObject(s.agent.value,mode==3 ? 2500:0)==WAIT_OBJECT_0;
    // Expire any unpublished/rejected late request before releasing the early gate.
    if(mode==1 && GetTickCount64()-begin<3200) Sleep(static_cast<DWORD>(3200-(GetTickCount64()-begin)));
    gate.finishGate();
    const auto &g=*gate.state;
    const bool restored=g.beforeValid && g.afterValid && memcmp(g.before,g.after,256)==0;
    bool temporary=true;
    if(mode>=2) {
        vt7pty::internal::cancel::Keyboard k;
        k.captured=k.applied=k.sent=k.restored=k.equal=1;
        memcpy(k.before,g.before,256);memcpy(k.during,g.during,256);memcpy(k.after,g.after,256);
        temporary=g.duringValid && vt7pty::internal::cancel::restoredKeyboard(k);
    }
    const bool childExited=WaitForSingleObject(s.child.value,10000)==WAIT_OBJECT_0;
    const bool hostExited=WaitForSingleObject(s.host.value,10000)==WAIT_OBJECT_0;
    vt7pty_error_ptr_t failure=nullptr;
    const BOOL resized=vt7pty_set_size(s.runtimeHandle(),80,25,&failure);
    const bool refused=!resized && vt7pty_error_code(failure)==VT7PTY_ERROR_LOST_CONNECTION;
    vt7pty_error_free(failure);
    writeFile(context.out+L"\\fault.json","{\"Mode\":"+std::to_string(mode)+",\"ActualHost\":"+std::to_string(g.actualHost)+
        ",\"ActualThread\":"+std::to_string(g.actualThread)+",\"ExpectedHost\":"+std::to_string(g.host)+
        ",\"ExpectedThread\":"+std::to_string(g.thread)+",\"AgentExitedBeforeRelease\":"+boolean(agentExited)+
        ",\"TraceStartOffset\":"+h3trace::u64(static_cast<uint64_t>(traceStart))+
        ",\"UnavailableBeforeRelease\":"+boolean(unavailable)+",\"UnavailableObservedTick\":"+h3trace::u64(unavailableTick)+
        ",\"EnteredTick\":"+h3trace::u64(g.enteredTick)+",\"ReleasedTick\":"+h3trace::u64(g.releasedTick)+
        ",\"GateInitialized\":"+std::to_string(g.initialized)+",\"GateEntered\":"+std::to_string(g.entered)+
        ",\"GateFinished\":"+std::to_string(g.finished)+",\"BeforeValid\":"+boolean(g.beforeValid!=FALSE)+
        ",\"DuringValid\":"+boolean(g.duringValid!=FALSE)+",\"AfterValid\":"+boolean(g.afterValid!=FALSE)+
        ",\"ChildExited\":"+boolean(childExited)+",\"HostExitedWithoutTermination\":"+boolean(hostExited)+
        ",\"PostFailureRpcRefused\":"+boolean(refused)+",\"CMessageCount\":"+std::to_string(g.cMessages)+
        ",\"GateError\":"+std::to_string(g.error)+",\"KeyboardRestored\":"+boolean(restored)+
        ",\"TemporaryCtrlVerified\":"+boolean(temporary)+",\"Before\":"+h3trace::hex(g.before)+
        ",\"During\":"+h3trace::hex(g.during)+",\"After\":"+h3trace::hex(g.after)+"}");
    check((mode==3 ? agentExited:unavailable) && childExited && hostExited && refused && !g.error && restored && temporary &&
        g.actualHost==g.host && g.actualThread==g.thread && g.cMessages==(mode==1 ? 0 : 1),
        "production timeout/rundown boundary");
}
int runQualification(const std::wstring &out)
{
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new qualification directory");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json")) return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"qualification trace");
    Collector collector(out);
    const std::vector<std::pair<std::string,std::function<void(Context &)>>> cases={
        {"inactive-desktop",inactiveDesktop},
        {"concurrent-resources",concurrentResources},
        {"job-explicit-breakaway-allowed",[](Context &c){callerDeath(c,false,true,JOB_OBJECT_LIMIT_BREAKAWAY_OK);}},
        {"job-silent-breakaway",[](Context &c){callerDeath(c,false,true,JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK);}},
        {"timeout-before-dispatch",[](Context &c){callbackFailure(c,1);}},
        {"timeout-during-callback",[](Context &c){callbackFailure(c,2);}},
        {"agent-death-during-callback",[](Context &c){callbackFailure(c,3);}}
    };
    std::string rows="[";unsigned int passed=0;
    for(size_t i=0;i<cases.size();++i) {
        Context context(out+L"\\"+std::wstring(cases[i].first.begin(),cases[i].first.end()));
        bool ok=false;std::string error;
        try {check(desktopPreflight(context.out+L"\\preflight.json"),"qualification desktop precondition");
            cases[i].second(context);ok=true;}
        catch(const std::exception &e){error=e.what();}
        ok=context.finish(ok && i<4) && ok;
        if(ok)++passed;
        if(i)rows+=',';
        rows+="{\"Case\":"+json(cases[i].first)+",\"Passed\":"+boolean(ok)+",\"OwnedProcessesCleaned\":"+
            boolean(context.cleaned)+",\"HostExited\":"+boolean(context.hostExited)+",\"Error\":"+json(error)+"}";
        printf("%s: %s %s\n",ok ? "PASS":"FAIL",cases[i].first.c_str(),error.c_str());
    }
    collector.stop();
    writeFile(out+L"\\cases.json","{\"Cases\":"+rows+"]}");
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4QualificationProbe\",\"SchemaVersion\":1,\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"Status\":"+json(passed==cases.size() ? "Pass":"Fail")+
        ",\"Passed\":"+std::to_string(passed)+",\"Total\":"+std::to_string(cases.size())+",\"H4Accepted\":false}");
    return passed==cases.size() ? 0:1;
}
int runOptInSoak(const std::wstring &out,unsigned int minutes)
{
    check(minutes>=1 && minutes<=120,"soak duration 1 through 120 minutes");
    outRoot=out;check(CreateDirectoryW(out.c_str(),nullptr),"new opt-in soak directory");rootEvidenceCreated=true;
    applicationDesktop=nameOf(GetThreadDesktop(GetCurrentThreadId()));
    if(!desktopPreflight(out+L"\\preflight.json"))return 2;
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG",L"trace"),"soak trace");
    Collector collector(out,268435456);
    const ResourceSample cold;
    writeFile(out+L"\\resources-cold.json",cold.jsonText());
    {
        Context warmup(out+L"\\warmup");cooked(warmup,"soak-warmup");
        check(warmup.finish(),"recorded soak warmup cleanup");
    }
    const ResourceSample before;
    writeFile(out+L"\\resources-baseline.json",before.jsonText());
    const auto begin=GetTickCount64(),deadline=begin+static_cast<uint64_t>(minutes)*60000;
    unsigned int cycles=0,recreated=0;
    auto lastProgress=begin;
    {
        Context primary(out+L"\\primary"),neighbor(out+L"\\neighbor");
        auto &active=primary.start();auto &quiet=neighbor.start();startRead(quiet,"SOAK_NEIGHBOR");
        while(GetTickCount64()<deadline || cycles<2) {
            const auto directoryPath=out+L"\\cycle-"+std::to_wstring(cycles+1);
            check(CreateDirectoryW(directoryPath.c_str(),nullptr),"soak cycle evidence");
            check(desktopPreflight(directoryPath+L"\\preflight.json"),"soak active desktop");
            startRead(active,"SOAK_DISCARD");
            active.input("\x03");
            const auto p=waitRead(active,cycles+1);
            writeFile(directoryPath+L"\\cancel.json",packetJson(p));
            check(canceled(p,cycles+1) && p.trace.overflow==0,"sustained cooked cancellation");
            nextRead(active,directoryPath,cycles+1);
            check(vt7pty_set_size(active.runtimeHandle(),80+static_cast<int>(cycles%3),25,nullptr),"soak resize");
            const auto pending=quiet.observe();
            check(!pending.readReturned && pending.controlCount==0,"soak neighbor preservation");
            writeFile(directoryPath+L"\\neighbor.json",packetJson(pending));
            if(cycles%10==0) {
                Context churn(directoryPath+L"\\recreated");cooked(churn,"soak-churn");
                check(churn.finish(),"soak recreated host cleanup");++recreated;
            }
            writeFile(directoryPath+L"\\resources.json",ResourceSample{}.jsonText());
            ++cycles;
            const auto now=GetTickCount64();
            if(now-lastProgress>=60000) {
                printf("Opt-in soak: %llu/%u minutes, %u cycles, %u recreated sessions\n",
                    static_cast<unsigned long long>((now-begin)/60000),minutes,cycles,recreated);
                fflush(stdout);lastProgress=now;
            }
            if(now<deadline)Sleep(static_cast<DWORD>(std::min<uint64_t>(5000,deadline-now)));
        }
        finishRead(quiet,L"SOAK_NEIGHBOR",neighbor.out,L"neighbor-read");
        check(primary.finish() && neighbor.finish(),"long-lived session cleanup");
    }
    const ResourceSample after;
    const auto elapsed=GetTickCount64()-begin;
    writeFile(out+L"\\resources-final.json",after.jsonText());
    resourceBound(before,after);
    collector.stop();
    writeFile(out+L"\\probe.json","{\"Kind\":\"H4OptInSoak\",\"SchemaVersion\":1,\"Status\":\"Pass\",\"SourceCommit\":"+
        json(GenVersion_Commit)+",\"Minutes\":"+std::to_string(minutes)+",\"ElapsedMilliseconds\":"+h3trace::u64(elapsed)+
        ",\"Cycles\":"+std::to_string(cycles)+",\"RecreatedSessions\":"+std::to_string(recreated)+
        ",\"Before\":"+before.jsonText()+",\"After\":"+after.jsonText()+
        ",\"LongLivedActiveAndNeighbor\":true,\"H4Accepted\":false}");
    return 0;
}
#include "H4ManualCases.h"
