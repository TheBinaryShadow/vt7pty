// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "HostResearch.h"
#include "HostDispatchProtocol.h"
using namespace fidelity;
using namespace hostresearch;
using namespace hostdispatch;
namespace {
struct Event { DWORD scope=0,callbackThread=0,thread=0,owner=0; uint64_t window=0; LONG object=0,child=0; };
struct Watch {
    HWINEVENTHOOK host=nullptr,agent=nullptr;
    HWND window=nullptr;
    std::array<Event,128> events{};
    size_t count=0;
    bool overflow=false;
    ~Watch() { if(host) UnhookWinEvent(host); if(agent) UnhookWinEvent(agent); }
};
Watch *watch=nullptr;
void CALLBACK eventCallback(HWINEVENTHOOK hook,DWORD,HWND window,LONG object,LONG child,DWORD thread,DWORD) {
    if(!watch || window!=watch->window || (hook!=watch->host && hook!=watch->agent)) return;
    if(watch->count==watch->events.size()) { watch->overflow=true; return; }
    Handle handle(OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,thread));
    watch->events[watch->count++]={hook==watch->host ? 1u : 2u,GetCurrentThreadId(),thread,
        handle.value ? GetProcessIdOfThread(handle.value) : 0,reinterpret_cast<uintptr_t>(window),object,child};
}
void pumpEvents() {
    MSG msg;
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}
DWORD discover(Session &s,const std::wstring &output,const std::wstring &label) {
    check(observeIdentity(s,output,(label+L"-identity").c_str()),"H1 prerequisite");
    Watch observation; observation.window=reinterpret_cast<HWND>(s.observe().window); watch=&observation;
    struct Reset { ~Reset() { watch=nullptr; } } reset;
    observation.host=SetWinEventHook(EVENT_OBJECT_NAMECHANGE,EVENT_OBJECT_NAMECHANGE,nullptr,eventCallback,GetProcessId(s.host.value),0,WINEVENT_OUTOFCONTEXT);
    observation.agent=SetWinEventHook(EVENT_OBJECT_NAMECHANGE,EVENT_OBJECT_NAMECHANGE,nullptr,eventCallback,GetProcessId(s.agent.value),0,WINEVENT_OUTOFCONTEXT);
    check(observation.host && observation.agent,"scoped title event hooks");
    // Two independent, ordinary title changes must identify the same live host thread.
    std::array<DWORD,2> selected{};
    for(size_t pass=0;pass<2;++pass) {
        const size_t first=observation.count;
        s.observe('T');
        const auto end=GetTickCount64()+500;
        while(GetTickCount64()<end) { pumpEvents(); s.pump(); Sleep(2); }
        for(size_t i=first;i<observation.count;++i) {
            const auto &e=observation.events[i];
            if(e.owner!=GetProcessId(s.host.value) || e.object!=OBJID_WINDOW || e.child!=CHILDID_SELF) continue;
            check(!selected[pass] || selected[pass]==e.thread,"unambiguous title-event thread");
            selected[pass]=e.thread;
        }
    }
    std::string data="{\"Events\":[";
    for(size_t i=0;i<observation.count;++i) {
        if(i) data+=',';
        const auto &e=observation.events[i];
        data+="{\"Scope\":"+std::to_string(e.scope)+",\"CallbackThread\":"+std::to_string(e.callbackThread)+
            ",\"EventThread\":"+std::to_string(e.thread)+",\"ThreadOwner\":"+std::to_string(e.owner)+
            ",\"Window\":"+std::to_string(e.window)+",\"Object\":"+std::to_string(e.object)+",\"Child\":"+std::to_string(e.child)+"}";
    }
    data+="],\"FirstThread\":"+std::to_string(selected[0])+",\"SecondThread\":"+std::to_string(selected[1])+
        ",\"Overflow\":"+(observation.overflow ? "true" : "false")+"}";
    writeFile(output+L"\\"+label+L"-discovery.json",data);
    check(!observation.overflow && selected[0] && selected[0]==selected[1],"repeatable owned title-event thread");
    return selected[0];
}
std::string identityJson(const Identity &i) {
    return "{\"Controller\":"+std::to_string(i.controller)+",\"ControllerThread\":"+std::to_string(i.controllerThread)+
        ",\"Host\":"+std::to_string(i.host)+",\"Thread\":"+std::to_string(i.thread)+",\"Agent\":"+std::to_string(i.agent)+
        ",\"Fixture\":"+std::to_string(i.fixture)+",\"ControllerCreated\":"+json(std::to_string(i.controllerCreated))+
        ",\"HostCreated\":"+json(std::to_string(i.hostCreated))+",\"AgentCreated\":"+json(std::to_string(i.agentCreated))+
        ",\"FixtureCreated\":"+json(std::to_string(i.fixtureCreated))+",\"Window\":"+std::to_string(i.window)+"}";
}
std::string responseJson(const Response &r) {
    return "{\"NonceLow\":"+json(std::to_string(r.nonce[0]))+",\"NonceHigh\":"+json(std::to_string(r.nonce[1]))+",\"Result\":"+std::to_string(static_cast<uint32_t>(r.result))+",\"Sequence\":"+std::to_string(r.sequence)+
        ",\"AcceptedSequence\":"+std::to_string(r.acceptedSequence)+",\"Host\":"+std::to_string(r.host)+
        ",\"Thread\":"+std::to_string(r.thread)+",\"HostCreated\":"+json(std::to_string(r.hostCreated))+
        ",\"Window\":"+std::to_string(r.window)+",\"Operation\":"+std::to_string(r.operation)+",\"ValidationStage\":"+std::to_string(r.validationStage)+",\"WindowOwner\":"+std::to_string(r.windowOwner)+",\"WindowThread\":"+std::to_string(r.windowThread)+"}";
}
void protocolOracle() {
    Request base; base.nonce[0]=1; base.nonce[1]=2; base.sequence=2; base.deadline=2000; base.identity.host=100;
    check(validate(base,base,1,1000,false)==Result::Accepted,"oracle valid request");
    auto bad=base; bad.schema=2; check(validate(bad,base,1,1000,false)==Result::Schema,"oracle schema");
    bad=base; bad.bytes=1; check(validate(bad,base,1,1000,false)==Result::Schema,"oracle size");
    bad=base; bad.nonce[1]=3; check(validate(bad,base,1,1000,false)==Result::Nonce,"oracle nonce");
    bad=base; ++bad.identity.host; check(validate(bad,base,1,1000,false)==Result::Identity,"oracle session");
    bad=base; ++bad.identity.hostCreated; check(validate(bad,base,1,1000,false)==Result::Identity,"oracle lifetime");
    check(validate(base,base,2,1000,false)==Result::Sequence,"oracle duplicate");
    bad=base; bad.sequence=4; check(validate(bad,base,1,1000,false)==Result::Sequence,"oracle gap");
    bad=base; bad.sequence=0; check(validate(bad,base,UINT64_MAX,1000,false)==Result::Sequence,"oracle wrap");
    bad=base; bad.deadline=1000; check(validate(bad,base,1,1000,false)==Result::Expired,"oracle expiry");
    bad=base; bad.deadline=6001; check(validate(bad,base,1,1000,false)==Result::Expired,"oracle excessive deadline");
    bad=base; bad.operation=Op::Bind; check(validate(bad,base,1,1000,false)==Result::Operation,"oracle rebind");
    check(validate(base,base,1,1000,true)==Result::Closed,"oracle closed generation");
    puts("Host dispatch oracle: 13 checks passed");
}
class Channel {
    Session &session;
    std::wstring output,label;
    HMODULE module=nullptr;
    HHOOK hook=nullptr;
    ATOM atom=0;
    Handle server,thread;
    bool disabled=false,closed=false;
    uint64_t sequence=1;
    unsigned int record=0;
    void verifyLive() {
        if(WaitForSingleObject(session.host.value,0)!=WAIT_TIMEOUT || WaitForSingleObject(thread.value,0)!=WAIT_TIMEOUT ||
            WaitForSingleObject(session.agent.value,0)!=WAIT_TIMEOUT || WaitForSingleObject(session.child.value,0)!=WAIT_TIMEOUT) {
            disabled=true; throw std::runtime_error("Closed: retained session lifetime ended");
        }
    }
    void signal() {
        check(!disabled && !closed,"dispatch disabled after close/ambiguity");
        check(WaitForSingleObject(session.host.value,0)==WAIT_TIMEOUT && WaitForSingleObject(thread.value,0)==WAIT_TIMEOUT,"live retained host/thread");
        check(agrees(hostQuery(session.agent.value),hostQuery(session.child.value)) &&
            (hostQuery(session.agent.value).raw&~ULONG_PTR{3})==binding.identity.host,"pre-dispatch association");
        DWORD_PTR result=0;
        SetLastError(ERROR_SUCCESS);
        if(!SendMessageTimeoutW(reinterpret_cast<HWND>(binding.identity.window),WM_NULL,atom,binding.identity.controller,
            SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&result)) { disabled=true; throw std::runtime_error("OutcomeUnknown: WM_NULL delivery timeout/failure"); }
    }
    Response receive(DWORD timeout=2000) {
        const auto end=GetTickCount64()+timeout;
        DWORD available=0;
        while(GetTickCount64()<end) {
            check(PeekNamedPipe(server.value,nullptr,0,nullptr,&available,nullptr),"acknowledgment pipe");
            if(available>=sizeof(Response)) break;
            session.pump(); Sleep(2);
        }
        if(available<sizeof(Response)) { disabled=true; throw std::runtime_error("OutcomeUnknown: acknowledgment deadline"); }
        Response response;
        transfer(server.value,&response,sizeof(response),false);
        check(response.signature==magic && response.schema==version && response.bytes==sizeof(response) &&
            response.nonce[0]==binding.nonce[0] && response.nonce[1]==binding.nonce[1] &&
            response.host==binding.identity.host && response.thread==binding.identity.thread &&
            response.hostCreated==binding.identity.hostCreated && response.window==binding.identity.window &&
            response.windowOwner==binding.identity.host && response.windowThread==binding.identity.thread,
            "actual host callback identity and challenge");
        return response;
    }
public:
    Request binding;
    unsigned int accepted=0,rejected=0;
    bool actualDispatch=false,unhooked=false;
    Channel(Session &s,const std::wstring &dir,const wchar_t *name):session(s),output(dir),label(name) {
        try {
            const DWORD tid=discover(s,output,label);
            thread.reset(OpenThread(THREAD_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,tid));
            check(thread.value && GetProcessIdOfThread(thread.value)==GetProcessId(s.host.value),"retained discovery thread");
            wchar_t desktop[256]={},station[256]={}; DWORD size=0;
            check(GetUserObjectInformationW(GetThreadDesktop(tid),UOI_NAME,desktop,sizeof(desktop),&size) && _wcsicmp(desktop,L"Default")==0 &&
                GetUserObjectInformationW(GetProcessWindowStation(),UOI_NAME,station,sizeof(station),&size) && _wcsicmp(station,L"WinSta0")==0,
                "interactive default desktop");
            const TokenIdentity ours(GetCurrentProcess()); check(ours.integrity==SECURITY_MANDATORY_MEDIUM_RID,"ordinary unelevated test account");
            HCRYPTPROV random=0;
            check(CryptAcquireContextW(&random,nullptr,nullptr,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT),"nonce provider");
            const BOOL generated=CryptGenRandom(random,sizeof(binding.nonce),reinterpret_cast<BYTE *>(binding.nonce));
            CryptReleaseContext(random,0); check(generated,"session nonce");
            const auto p=s.observe();
            binding.identity={GetCurrentProcessId(),GetCurrentThreadId(),GetProcessId(s.host.value),tid,GetProcessId(s.agent.value),GetProcessId(s.child.value),
                creation(GetCurrentProcess()),creation(s.host.value),creation(s.agent.value),creation(s.child.value),p.window};
            wchar_t suffix[33]={}; swprintf_s(suffix,L"%016llx%016llx",binding.nonce[0],binding.nonce[1]);
            const std::wstring nameValue=L"VT7Pty-H2-"+std::to_wstring(GetCurrentProcessId())+L"-"+suffix;
            server.reset(privateServer(L"\\\\.\\pipe\\"+nameValue));
            atom=GlobalAddAtomW(nameValue.c_str()); check(atom>=0xc000,"session channel atom");
            // Absolute, colocated x64 module. The portable runner verifies its package hash before this process starts.
            const auto path=directory()+L"\\HostDispatchHook.dll";
            module=LoadLibraryExW(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH); check(module!=nullptr,"research callback DLL");
            HOOKPROC callback=nullptr; const auto proc=GetProcAddress(module,"HostDispatchCallback");
            static_assert(sizeof(callback)==sizeof(proc)); memcpy(&callback,&proc,sizeof(callback)); if(!callback) throw std::runtime_error("callback export unavailable");
            hook=SetWindowsHookExW(WH_CALLWNDPROC,callback,module,tid); check(hook!=nullptr,"thread-scoped research callback");
            // CreateFile in the callback completes the pending server connection, never waits for it.
            Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr)); check(event.value!=nullptr,"connect event");
            OVERLAPPED ov={}; ov.hEvent=event.value;
            const BOOL connected=ConnectNamedPipe(server.value,&ov);
            const DWORD error=connected ? ERROR_SUCCESS : GetLastError();
            check(connected || error==ERROR_IO_PENDING || error==ERROR_PIPE_CONNECTED,"channel listen");
            try { signal(); } catch(...) {
                if(error==ERROR_IO_PENDING) { CancelIoEx(server.value,&ov); DWORD ignored=0; complete(server.value,ov,ignored,1000); }
                throw;
            }
            if(error==ERROR_IO_PENDING) { DWORD ignored=0; check(complete(server.value,ov,ignored,2000),"channel connection"); }
            ULONG peer=0; check(GetNamedPipeClientProcessId(server.value,&peer) && peer==binding.identity.host,"independent callback pipe peer");
            const Response hello=receive();
            check(hello.result==Result::Hello && hello.sequence==0 && hello.acceptedSequence==0,"callback hello");
            actualDispatch=true;
            writeFile(output+L"\\"+label+L"-hello.json","{\"Identity\":"+identityJson(binding.identity)+",\"PipePeer\":"+std::to_string(peer)+",\"Response\":"+responseJson(hello)+"}");
            auto request=next(Op::Bind); exchange(request,Result::Accepted,"bind");
        } catch(...) { detach(); throw; }
    }
    ~Channel() { detach(); }
    Request next(Op op=Op::Ping) const {
        auto request=binding; request.operation=op; request.sequence=sequence; request.deadline=GetTickCount64()+4000; return request;
    }
    Response exchange(Request request,Result expected,const char *caseName) {
        try {
            check(!disabled && !closed,"no dispatch on retired channel");
            verifyLive();
            transfer(server.value,&request,sizeof(request),true);
            signal(); const auto response=receive();
            writeFile(output+L"\\"+label+L"-"+std::to_wstring(++record)+L"-request.json",
                "{\"Case\":"+json(caseName)+",\"RequestIdentity\":"+identityJson(request.identity)+
                ",\"RequestSchema\":"+std::to_string(request.schema)+",\"RequestBytes\":"+std::to_string(request.bytes)+
                ",\"RequestNonceLow\":"+json(std::to_string(request.nonce[0]))+",\"RequestNonceHigh\":"+json(std::to_string(request.nonce[1]))+
                ",\"RequestDeadline\":"+std::to_string(request.deadline)+",\"RequestSequence\":"+std::to_string(request.sequence)+",\"RequestOperation\":"+std::to_string(static_cast<uint32_t>(request.operation))+
                ",\"ExpectedResult\":"+std::to_string(static_cast<uint32_t>(expected))+",\"Response\":"+responseJson(response)+"}");
            check(response.result==expected && response.sequence==request.sequence && response.operation==static_cast<uint32_t>(request.operation),"request outcome");
            if(expected==Result::Accepted) { ++accepted; ++sequence; check(response.acceptedSequence==sequence-1,"accepted sequence"); }
            else if(expected==Result::Received) { ++sequence; check(response.acceptedSequence==sequence-1,"receipt sequence"); }
            else { ++rejected; check(response.acceptedSequence==sequence-1,"rejection preserves sequence"); }
            return response;
        } catch(...) { disabled=true; throw; }
    }
    void ambiguityControl() {
        exchange(next(Op::SuppressAck),Result::Received,"receipt-without-final-ack");
        bool timedOut=false;
        try { receive(150); } catch(const std::runtime_error &e) { timedOut=std::string(e.what())=="OutcomeUnknown: acknowledgment deadline"; }
        check(timedOut && blocked(),"unknown outcome disables dispatch");
        bool refused=false; try { exchange(next(),Result::Accepted,"must-not-send"); } catch(...) { refused=true; }
        check(refused,"no retry after ambiguous outcome");
        writeFile(output+L"\\"+label+L"-ambiguity.json","{\"ReceiptObserved\":true,\"FinalAckTimedOut\":true,\"DispatchDisabled\":true,\"RetryRefused\":true,\"FallbackControlSent\":false}");
    }
    void endedHostControl() {
        bool refused=false; try { exchange(next(),Result::Accepted,"must-not-send-after-exit"); } catch(...) { refused=true; }
        check(refused && blocked(),"closed host rejected before write");
        writeFile(output+L"\\"+label+L"-host-exit.json","{\"HostSignaled\":true,\"DispatchDisabled\":true,\"RequestRefusedBeforeWrite\":true}");
    }
    void close() { exchange(next(Op::Close),Result::Accepted,"close"); closed=true; detach(); }
    void detach() noexcept {
        disabled=true;
        if(hook) {
            const BOOL result=UnhookWindowsHookEx(hook);
            unhooked=result!=FALSE || WaitForSingleObject(session.host.value,0)==WAIT_OBJECT_0;
            hook=nullptr;
        }
        server.reset();
        if(atom) { GlobalDeleteAtom(atom); atom=0; }
        // User32 owns the remote module's hook lifetime. This only frees our local reference.
        if(module) { FreeLibrary(module); module=nullptr; }
    }
    bool blocked() const { return disabled || closed; }
};
int abandon(const std::wstring &output,const std::wstring &name,DWORD expectedParent) {
    check(parentProcessId(GetCurrentProcessId())==expectedParent,"owned abandonment controller parent");
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    check(pipe.value!=INVALID_HANDLE_VALUE,"abandonment peer pipe");
    ULONG peer=0; check(GetNamedPipeServerProcessId(pipe.value,&peer) && peer==expectedParent,"abandonment server PID");
    Session session(77); startRead(session,"FOXTROT");
    Channel channel(session,output,L"F");
    auto identity=channel.binding.identity; transfer(pipe.value,&identity,sizeof(identity),true);
    char command=0; transfer(pipe.value,&command,1,false);
    check(command=='Q',"abandonment trigger");
    // Deliberately skip all C++ cleanup to measure controller-death behavior.
    ExitProcess(73);
}
bool controllerDeath(const std::wstring &output) {
    const auto name=L"\\\\.\\pipe\\VT7Pty-H2-death-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    Handle server(privateServer(name));
    const auto exe=imagePath(GetCurrentProcess());
    const auto command=quote(exe)+L" --abandon "+quote(output)+L" "+quote(name)+L" "+std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> mutableCommand(command.begin(),command.end()); mutableCommand.push_back(0);
    STARTUPINFOW startup={}; startup.cb=sizeof(startup); PROCESS_INFORMATION process={};
    check(CreateProcessW(exe.c_str(),mutableCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,directory().c_str(),&startup,&process),"owned abandonment controller");
    Handle controller(process.hProcess),controllerThread(process.hThread),agent,fixtureProcess,host;
    bool verifiedOwnedClients=false;
    try {
        connect(server.value);
        ULONG peer=0; check(GetNamedPipeClientProcessId(server.value,&peer) && peer==process.dwProcessId,"abandonment client PID");
        Identity identity; transfer(server.value,&identity,sizeof(identity),false);
        check(identity.controller==process.dwProcessId && identity.controllerCreated==creation(controller.value),"abandonment controller lifetime");
        auto retain=[&](Handle &handle,DWORD pid,uint64_t created,DWORD extraAccess=0) {
            handle.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|extraAccess,FALSE,pid));
            check(handle.value && creation(handle.value)==created && WaitForSingleObject(handle.value,0)==WAIT_TIMEOUT,"retain abandonment process");
        };
        retain(agent,identity.agent,identity.agentCreated,PROCESS_TERMINATE); retain(fixtureProcess,identity.fixture,identity.fixtureCreated,PROCESS_TERMINATE); retain(host,identity.host,identity.hostCreated);
        check(parentProcessId(identity.agent)==identity.controller && parentProcessId(identity.fixture)==identity.agent,"abandonment descendants");
        check(agrees(hostQuery(agent.value),hostQuery(fixtureProcess.value)) &&
            (hostQuery(agent.value).raw&~ULONG_PTR{3})==identity.host,"abandonment host association");
        verifiedOwnedClients=true;
        char quit='Q'; transfer(server.value,&quit,1,true);
        check(WaitForSingleObject(controller.value,5000)==WAIT_OBJECT_0,"abandonment controller exit");
        DWORD exit=0; check(GetExitCodeProcess(controller.value,&exit) && exit==73,"abandonment exit status");
        const bool clients=WaitForSingleObject(agent.value,5000)==WAIT_OBJECT_0 && WaitForSingleObject(fixtureProcess.value,5000)==WAIT_OBJECT_0;
        const bool hostExited=WaitForSingleObject(host.value,5000)==WAIT_OBJECT_0;
        writeFile(output+L"\\F-controller-death.json","{\"Identity\":"+identityJson(identity)+
            ",\"ControllerExit\":73,\"OwnedClientsExited\":"+(clients ? "true" : "false")+
            ",\"HostExited\":"+(hostExited ? "true" : "false")+",\"ExplicitUnhookBeforeExit\":false}");
        check(clients && hostExited,"controller-death host/client cleanup");
        return true;
    } catch(...) {
        if(WaitForSingleObject(controller.value,0)==WAIT_TIMEOUT) { TerminateProcess(controller.value,125); WaitForSingleObject(controller.value,5000); }
        // The exact retained owned-client handles may be terminated on failure; never the host.
        for(HANDLE child:{agent.value,fixtureProcess.value}) if(verifiedOwnedClients && child && child!=INVALID_HANDLE_VALUE && WaitForSingleObject(child,5000)==WAIT_TIMEOUT) {
            TerminateProcess(child,125); WaitForSingleObject(child,5000);
        }
        throw;
    }
}

int run(const std::wstring &output) {
    protocolOracle();
    std::array<std::unique_ptr<Session>,5> sessions;
    std::array<std::unique_ptr<Channel>,5> channels;
    bool dispatch=false,isolation=false,ambiguity=false,endedHost=false,deathCleanup=false,cleaned=true,hostsExited=true; unsigned int accepted=0,rejected=0;
    std::string error;
    try {
        sessions[0]=std::make_unique<Session>(0); sessions[1]=std::make_unique<Session>(1);
        auto &a=*sessions[0]; auto &b=*sessions[1];
        startRead(a,"ALPHA"); startRead(b,"BRAVO_KEEP");
        channels[0]=std::make_unique<Channel>(a,output,L"A");
        channels[1]=std::make_unique<Channel>(b,output,L"B");
        auto &ac=*channels[0]; auto &bc=*channels[1];
        ac.exchange(ac.next(),Result::Accepted,"ping");
        auto bad=ac.next(); --bad.sequence; ac.exchange(bad,Result::Sequence,"duplicate");
        bad=ac.next(); ++bad.nonce[0]; ac.exchange(bad,Result::Nonce,"wrong-nonce");
        bad=ac.next(); bad.identity=bc.binding.identity; ac.exchange(bad,Result::Identity,"cross-session");
        bad=ac.next(); ++bad.identity.hostCreated; ac.exchange(bad,Result::Identity,"stale-host-generation");
        bad=ac.next(); bad.deadline=GetTickCount64(); ac.exchange(bad,Result::Expired,"expired");
        bad=ac.next(); ++bad.schema; ac.exchange(bad,Result::Schema,"wrong-schema");
        bad=ac.next(); bad.bytes=1; ac.exchange(bad,Result::Schema,"wrong-size");
        bad=ac.next(); ++bad.sequence; ac.exchange(bad,Result::Sequence,"sequence-gap");
        bad=ac.next(Op::Bind); ac.exchange(bad,Result::Operation,"rebind");
        ac.exchange(ac.next(),Result::Accepted,"after-rejections");
        bc.exchange(bc.next(),Result::Accepted,"unaffected-neighbor");
        check(!a.observe().readReturned && !b.observe().readReturned,"pending reads unchanged by dispatch");
        ac.close(); check(ac.blocked() && ac.unhooked,"A retired and unhooked");
        finishRead(a,L"ALPHA",output,L"A-read"); a.quit();
        check(WaitForSingleObject(a.host.value,5000)==WAIT_OBJECT_0,"A host exit");
        check(!b.observe().readReturned,"B survives A close");
        sessions[2]=std::make_unique<Session>(2); auto &c=*sessions[2];
        startRead(c,"CHARLIE"); channels[2]=std::make_unique<Channel>(c,output,L"C");
        auto &cc=*channels[2];
        bad=cc.next(); bad.nonce[0]=ac.binding.nonce[0]; bad.nonce[1]=ac.binding.nonce[1];
        cc.exchange(bad,Result::Nonce,"retired-session-nonce");
        cc.exchange(cc.next(),Result::Accepted,"replacement-ping");
        cc.close(); finishRead(c,L"CHARLIE",output,L"C-read"); c.quit();
        sessions[3]=std::make_unique<Session>(3); auto &d=*sessions[3];
        startRead(d,"DELTA"); channels[3]=std::make_unique<Channel>(d,output,L"D");
        channels[3]->ambiguityControl(); ambiguity=true; channels[3]->detach();
        finishRead(d,L"DELTA",output,L"D-read"); d.quit();
        sessions[4]=std::make_unique<Session>(4); auto &e=*sessions[4];
        startRead(e,"ECHO"); channels[4]=std::make_unique<Channel>(e,output,L"E");
        finishRead(e,L"ECHO",output,L"E-read"); e.quit();
        check(WaitForSingleObject(e.host.value,5000)==WAIT_OBJECT_0,"E host exit with callback installed");
        channels[4]->endedHostControl(); endedHost=true; channels[4]->detach();
        check(!b.observe().readReturned,"B pending across ambiguous request and neighbor exit");
        deathCleanup=controllerDeath(output);
        check(!b.observe().readReturned,"B pending across neighbor controller death");
        bc.exchange(bc.next(),Result::Accepted,"survivor-after-replacement");
        bc.close(); finishRead(b,L"BRAVO_KEEP",output,L"B-read"); b.quit();
        dispatch=ac.actualDispatch && bc.actualDispatch && cc.actualDispatch && channels[3]->actualDispatch && channels[4]->actualDispatch;
        isolation=true;
    } catch(const std::exception &e) { error=e.what(); }
    if(deathCleanup) ++accepted; // F binds in the deliberately abandoned controller.
    bool unhooked=true;
    for(size_t i=0;i<sessions.size();++i) {
        if(channels[i]) { accepted+=channels[i]->accepted; rejected+=channels[i]->rejected; channels[i]->detach(); unhooked=channels[i]->unhooked && unhooked; }
        if(sessions[i]) {
            cleaned=sessions[i]->cleanup() && cleaned;
            if(sessions[i]->host.value && sessions[i]->host.value!=INVALID_HANDLE_VALUE)
                hostsExited=(WaitForSingleObject(sessions[i]->host.value,5000)==WAIT_OBJECT_0) && hostsExited;
            writeFile(output+L"\\"+static_cast<wchar_t>(L'A'+i)+L"-stream.bin",sessions[i]->raw);
        }
    }
    const bool passed=error.empty() && dispatch && isolation && ambiguity && endedHost && deathCleanup && cleaned && hostsExited && unhooked && accepted==14 && rejected==10;
    writeFile(output+L"\\probe.json","{\"SchemaVersion\":1,\"Probe\":\"HostDispatch\",\"Version\":1,\"Status\":"+json(passed ? "Pass" : "Fail")+
        ",\"DispatchThreadVerified\":"+(dispatch ? "true" : "false")+",\"AcceptedRequests\":"+std::to_string(accepted)+
        ",\"RejectedRequests\":"+std::to_string(rejected)+",\"SessionIsolationPassed\":"+(isolation ? "true" : "false")+
        ",\"AmbiguityControlPassed\":"+(ambiguity ? "true" : "false")+",\"HostExitControlPassed\":"+(endedHost ? "true" : "false")+
        ",\"ControllerDeathCleanupPassed\":"+(deathCleanup ? "true" : "false")+
        ",\"HooksRemoved\":"+(unhooked ? "true" : "false")+",\"OwnedProcessesCleaned\":"+(cleaned ? "true" : "false")+
        ",\"ObservedHostsExited\":"+(hostsExited ? "true" : "false")+",\"CancellationImplemented\":false,\"Error\":"+json(error)+"}");
    printf("Host dispatch probe: %s\n",passed ? "Pass" : "Fail"); return passed ? 0 : 1;
}
}
int wmain(int argc,wchar_t **argv) {
    try {
        if(argc==5 && std::wstring_view(argv[1])==L"--abandon") return abandon(argv[2],argv[3],wcstoul(argv[4],nullptr,10));
        if(argc==4 && std::wstring_view(argv[1])==L"--fixture") return fixture(argv[2],wcstoul(argv[3],nullptr,10));
        if(argc==2 && std::wstring_view(argv[1])==L"--self-test") { protocolOracle(); return 0; }
        if(argc==2) return run(argv[1]);
        return 2;
    } catch(const std::exception &e) { fprintf(stderr,"Host dispatch harness error: %s\n",e.what()); return 2; }
}
