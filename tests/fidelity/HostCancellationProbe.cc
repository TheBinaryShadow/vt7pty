// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "HostCancellationWorker.h"

std::wstring randomName() {
    HCRYPTPROV provider=0; check(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT),"desktop nonce provider");
    uint64_t nonce[2]={}; const BOOL okay=CryptGenRandom(provider,sizeof(nonce),reinterpret_cast<BYTE *>(nonce));
    CryptReleaseContext(provider,0); check(okay,"desktop nonce");
    wchar_t text[33]={}; swprintf_s(text,L"%016llx%016llx",nonce[0],nonce[1]); return text;
}
struct Desktop {
    HDESK value=nullptr;
    std::wstring name;
    bool closed=false;
    Desktop():name(L"VT7Pty-H3-"+randomName()) {
        Handle token; check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value),"desktop token");
        DWORD bytes=0; GetTokenInformation(token.value,TokenUser,nullptr,0,&bytes);
        std::vector<BYTE> storage(bytes); check(GetTokenInformation(token.value,TokenUser,storage.data(),bytes,&bytes),"desktop user");
        LocalAllocation<wchar_t> sid; check(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid,&sid.value),"desktop SID");
        const std::wstring acl=L"D:P(A;;GA;;;SY)(A;;GA;;;"+std::wstring(sid.value)+L")";
        LocalAllocation<void> descriptor; check(ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor.value,nullptr),"desktop ACL");
        SECURITY_ATTRIBUTES attributes={sizeof(attributes),descriptor.value,FALSE};
        value=CreateDesktopW(name.c_str(),nullptr,nullptr,0,
            DESKTOP_CREATEWINDOW|DESKTOP_CREATEMENU|DESKTOP_ENUMERATE|DESKTOP_HOOKCONTROL|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS,&attributes);
        check(value!=nullptr,"private desktop");
    }
    ~Desktop() { close(); }
    bool close() noexcept { if(value) { closed=CloseDesktop(value)!=FALSE; if(closed) value=nullptr; } return closed; }
};
struct Lifetime {
    Identity identity;
    Handle agent,fixture,host,shell;
    DWORD shellPid=0;
};
struct Worker {
    Desktop desktop;
    Handle process,thread,pipe;
    std::wstring output;
    std::vector<std::unique_ptr<Lifetime>> lifetimes;
    uint32_t lastRegistration=0;
    bool ready=false,cleanupOkay=true;
    DWORD exitCode=STILL_ACTIVE;
    explicit Worker(const std::wstring &dir,const wchar_t *role):output(dir) {
        check(CreateDirectoryW(output.c_str(),nullptr),"new worker directory");
        const auto pipeName=L"\\\\.\\pipe\\VT7Pty-H3-parent-"+randomName();
        pipe.reset(privateServer(pipeName));
        const auto exe=imagePath(GetCurrentProcess());
        const auto command=quote(exe)+L" --worker "+quote(pipeName)+L" "+std::to_wstring(GetCurrentProcessId())+L" "+quote(output)+L" "+role;
        std::vector<wchar_t> line(command.begin(),command.end()); line.push_back(0);
        std::wstring full=L"WinSta0\\"+desktop.name;
        STARTUPINFOW startup={}; startup.cb=sizeof(startup); startup.lpDesktop=full.data(); PROCESS_INFORMATION pi={};
        check(CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,directory().c_str(),&startup,&pi),"private desktop worker");
        process.reset(pi.hProcess); thread.reset(pi.hThread);
        try {
            connect(pipe.value); ULONG peer=0;
            check(GetNamedPipeClientProcessId(pipe.value,&peer) && peer==pi.dwProcessId,"created worker pipe peer");
            // Cross-desktop GetThreadDesktop is denied on the development host.
            // Registration independently locates the exact console HWND through our retained desktop.
        } catch(...) { stop(); throw; }
    }
    ~Worker() { stop(); }
    bool alive() { return WaitForSingleObject(process.value,0)==WAIT_TIMEOUT; }
    void poll() {
        DWORD available=0;
        if(!PeekNamedPipe(pipe.value,nullptr,0,nullptr,&available,nullptr)) {
            check(GetLastError()==ERROR_BROKEN_PIPE || GetLastError()==ERROR_PIPE_NOT_CONNECTED,"worker pipe observation");
            check(WaitForSingleObject(process.value,2000)==WAIT_OBJECT_0,"worker closed channel without exiting"); return;
        }
        if(available<sizeof(Registration)) return;
        Registration r; transfer(pipe.value,&r,sizeof(r),false);
        if(r.kind==2) { check(r.ready==1 && !ready,"neighbor ready"); ready=true; return; }
        check(r.kind==1 && r.sequence==lastRegistration+1,"registration sequence");
        const auto &i=r.identity;
        check(i.controller==GetProcessId(process.value) && i.controllerCreated==creation(process.value),"registered worker lifetime");
        auto life=std::make_unique<Lifetime>(); life->identity=i;
        life->agent.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,i.agent));
        life->fixture.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,i.fixture));
        life->host.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,i.host));
        check(life->agent.value && life->fixture.value && life->host.value,"registered process access");
        check(creation(life->agent.value)==i.agentCreated && creation(life->fixture.value)==i.fixtureCreated &&
            creation(life->host.value)==i.hostCreated && parentProcessId(i.agent)==i.controller && parentProcessId(i.fixture)==i.agent,"registered descendants");
        check(_wcsicmp(imagePath(life->fixture.value).c_str(),imagePath(process.value).c_str())==0 &&
            _wcsicmp(imagePath(life->agent.value).c_str(),(directory()+L"\\VT7Pty-Agent.exe").c_str())==0,"registered client images");
        check(agrees(hostQuery(life->agent.value),hostQuery(life->fixture.value)) && (hostQuery(life->fixture.value).raw&~ULONG_PTR{3})==i.host,"registered host association");
        Handle hostThread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,i.thread));
        check(hostThread.value && GetProcessIdOfThread(hostThread.value)==i.host,"retained registered host thread");
        struct WindowMatch { HWND expected; bool found=false; } match{reinterpret_cast<HWND>(i.window)};
        check(EnumDesktopWindows(desktop.value,[](HWND window,LPARAM state)->BOOL {
            auto &m=*reinterpret_cast<WindowMatch *>(state); if(window==m.expected) m.found=true; return TRUE;
        },reinterpret_cast<LPARAM>(&match)) && match.found,"console HWND on exact private desktop");
        const TokenIdentity ours(GetCurrentProcess()),host(life->host.value);
        check(ours.same(host) && host.integrity==SECURITY_MANDATORY_MEDIUM_RID,"root/host same ordinary token");
        if(r.shell) {
            life->shell.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,r.shell));
            check(life->shell.value && creation(life->shell.value)==r.shellCreated && parentProcessId(r.shell)==i.fixture &&
                agrees(hostQuery(life->shell.value),hostQuery(life->fixture.value)),"registered owned shell");
            life->shellPid=r.shell;
        }
        writeFile(output+L"\\lifetime-"+std::to_wstring(r.sequence)+L".json","{\"Identity\":"+identityJson(i)+
            ",\"Desktop\":"+json(utf8(desktop.name))+",\"ConsoleWindowEnumeratedOnDesktop\":true,\"Shell\":"+std::to_string(r.shell)+",\"ShellCreated\":"+json(std::to_string(r.shellCreated))+"}");
        lifetimes.push_back(std::move(life)); lastRegistration=r.sequence;
        char ack='A'; transfer(pipe.value,&ack,1,true);
    }
    void command(char value) { transfer(pipe.value,&value,1,true); }
    bool stop() noexcept {
        if(!process.value || process.value==INVALID_HANDLE_VALUE) return cleanupOkay;
        if(WaitForSingleObject(process.value,0)!=WAIT_OBJECT_0) {
            cleanupOkay=false; TerminateProcess(process.value,124); WaitForSingleObject(process.value,5000);
        }
        GetExitCodeProcess(process.value,&exitCode);
        pipe.reset();
        for(const auto &life:lifetimes) {
            // Retained and independently verified descendants only. Never request
            // PROCESS_TERMINATE for conhost or terminate it on any cleanup path.
            for(HANDLE client:{life->shell.value,life->fixture.value,life->agent.value}) if(client && client!=INVALID_HANDLE_VALUE) {
                if(WaitForSingleObject(client,5000)!=WAIT_OBJECT_0) {
                    cleanupOkay=false; TerminateProcess(client,125); WaitForSingleObject(client,5000);
                }
            }
            if(WaitForSingleObject(life->host.value,5000)!=WAIT_OBJECT_0) cleanupOkay=false;
        }
        process.reset(); thread.reset();
        cleanupOkay=desktop.close() && cleanupOkay;
        return cleanupOkay;
    }
};
struct ParentWatch {
    Handle parent,stopEvent,thread;
    static DWORD WINAPI monitor(void *value) {
        const auto &self=*static_cast<ParentWatch *>(value);
        HANDLE handles[2]={self.parent.value,self.stopEvent.value};
        if(WaitForMultipleObjects(2,handles,FALSE,INFINITE)==WAIT_OBJECT_0) ExitProcess(124);
        return 0;
    }
    explicit ParentWatch(DWORD pid) {
        parent.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        check(parent.value && parentProcessId(GetCurrentProcessId())==pid &&
            _wcsicmp(imagePath(parent.value).c_str(),imagePath(GetCurrentProcess()).c_str())==0,"owned worker parent");
        stopEvent.reset(CreateEventW(nullptr,TRUE,FALSE,nullptr)); check(stopEvent.value!=nullptr,"parent monitor stop");
        thread.reset(CreateThread(nullptr,0,monitor,this,0,nullptr)); check(thread.value!=nullptr,"parent monitor");
    }
    ~ParentWatch() { SetEvent(stopEvent.value); WaitForSingleObject(thread.value,INFINITE); }
};
int worker(const std::wstring &pipeName,DWORD parent,const std::wstring &output,const std::wstring &role) {
    ParentWatch watchParent(parent);
    check(desktopName(GetCurrentThreadId()).starts_with(L"VT7Pty-H3-"),"private worker desktop");
    Handle pipe(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    check(pipe.value!=INVALID_HANDLE_VALUE,"worker parent channel");
    ULONG peer=0; check(GetNamedPipeServerProcessId(pipe.value,&peer) && peer==parent,"worker parent pipe peer");
    parentPipe=pipe.value;
    if(role==L"suite") return suite(output);
    if(role==L"neighbor") return neighbor(output);
    return 2;
}
int run(const std::wstring &output) {
    cancellationOracle();
    const auto originalDesktop=desktopName(GetCurrentThreadId());
    check(originalDesktop==L"Default","root on normal test desktop");
    const TokenIdentity token(GetCurrentProcess()); check(token.integrity==SECURITY_MANDATORY_MEDIUM_RID,"ordinary test user");
    std::unique_ptr<Worker> a,b;
    std::string error;
    bool isolated=false,cleaned=true;
    DWORD aExit=2,bExit=2;
    unsigned int polls=0;
    try {
        b=std::make_unique<Worker>(output+L"\\neighbor",L"neighbor");
        auto deadline=GetTickCount64()+15000;
        while(!b->ready && b->alive() && GetTickCount64()<deadline) { b->poll(); Sleep(2); }
        check(b->ready && b->alive(),"neighbor pending readiness");
        a=std::make_unique<Worker>(output+L"\\suite",L"suite");
        check(a->desktop.name!=b->desktop.name && a->desktop.name!=originalDesktop && b->desktop.name!=originalDesktop,"separate desktops");
        uint32_t observed=0;
        deadline=GetTickCount64()+150000;
        while(a->alive() && GetTickCount64()<deadline) {
            a->poll(); b->poll();
            check(b->alive(),"neighbor alive during suite");
            if(a->lastRegistration!=observed) { b->command('I'); ++polls; observed=a->lastRegistration; }
            Sleep(2);
        }
        check(!a->alive(),"suite deadline");
        a->poll(); check(GetExitCodeProcess(a->process.value,&aExit),"suite exit");
        b->command('Q');
        deadline=GetTickCount64()+15000;
        while(b->alive() && GetTickCount64()<deadline) { b->poll(); Sleep(2); }
        check(!b->alive() && GetExitCodeProcess(b->process.value,&bExit),"neighbor exit");
        isolated=bExit==0 && polls>0 && desktopName(GetCurrentThreadId())==originalDesktop;
    } catch(const std::exception &e) { error=e.what(); cleaned=false; }
    if(a) cleaned=a->stop() && cleaned;
    if(b) cleaned=b->stop() && cleaned;
    const bool passed=error.empty() && aExit==0 && bExit==0 && isolated && cleaned;
    writeFile(output+L"\\probe.json","{\"SchemaVersion\":1,\"Probe\":\"HostCancellation\",\"Version\":1,\"Status\":"+json(passed ? "Pass" : "Fail")+
        ",\"SuiteExit\":"+std::to_string(aExit)+",\"NeighborExit\":"+std::to_string(bExit)+",\"NeighborPolls\":"+std::to_string(polls)+
        ",\"DesktopIsolationVerified\":"+(isolated ? "true" : "false")+",\"OwnedLifetimesCleaned\":"+(cleaned ? "true" : "false")+
        ",\"RootDesktop\":"+json(utf8(originalDesktop))+",\"SuiteDesktop\":"+json(a ? utf8(a->desktop.name) : "")+
        ",\"NeighborDesktop\":"+json(b ? utf8(b->desktop.name) : "")+
        ",\"MilestoneAccepted\":false,\"ProductionCancellationImplemented\":false,\"Error\":"+json(error)+"}");
    printf("Host cancellation probe: %s\n",passed ? "Pass" : "Fail");
    return passed ? 0 : 1;
}
}
int wmain(int argc,wchar_t **argv) {
    try {
        if(argc==4 && std::wstring_view(argv[1])==L"--fixture") return fixture(argv[2],wcstoul(argv[3],nullptr,10));
        if(argc==6 && std::wstring_view(argv[1])==L"--worker") return worker(argv[2],wcstoul(argv[3],nullptr,10),argv[4],argv[5]);
        if(argc==2 && std::wstring_view(argv[1])==L"--self-test") { cancellationOracle(); return 0; }
        if(argc==2) return run(argv[1]);
        return 2;
    } catch(const std::exception &e) { fprintf(stderr,"Host cancellation harness error: %s\n",e.what()); return 2; }
}
