// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
// Research only: no hooks, host messages, remote writes or input cancellation.
#include "WinTest.h"
#include "VtState.h"
#include "../../src/include/vt7pty.h"
#include <algorithm>
#include <array>
#include <functional>
#include <limits>

using namespace fidelity;
namespace {
struct Query {
    LONG status=static_cast<LONG>(0xc0000002u);
    ULONG length=0;
    ULONG_PTR raw=0;
};
// phnt identifies ProcessConsoleHostProcess as class 49, ULONG_PTR.
// Internal interface: retain raw results and reject unfamiliar representations.
Query hostQuery(HANDLE process) {
    using Function=LONG (NTAPI *)(HANDLE,ULONG,void *,ULONG,ULONG *);
    const HMODULE module=GetModuleHandleW(L"ntdll.dll"); if(!module) return {};
    const auto address=GetProcAddress(module,"NtQueryInformationProcess");
    Function function=nullptr;
    static_assert(sizeof(function)==sizeof(address));
    memcpy(&function,&address,sizeof(function));
    Query result;
    if(function) result.status=function(process,49,&result.raw,sizeof(result.raw),&result.length);
    return result;
}
bool valid(const Query &q) {
    return q.status>=0 && q.length==sizeof(ULONG_PTR) && q.raw!=0 &&
        q.raw<=std::numeric_limits<DWORD>::max() && (q.raw&~ULONG_PTR{3})!=0;
}
bool agrees(const Query &a,const Query &b) { return valid(a) && valid(b) && a.raw==b.raw; }
std::string queryJson(const Query &q) {
    return "{\"NtStatus\":"+std::to_string(static_cast<ULONG>(q.status))+
        ",\"ReturnLength\":"+std::to_string(q.length)+",\"RawHostValue\":"+std::to_string(q.raw)+",\"LowBits\":"+std::to_string(q.raw&3u)+
        ",\"NormalizedPid\":"+std::to_string(q.raw&~ULONG_PTR{3})+"}";
}
std::string utf8(std::wstring_view text) {
    if(text.empty()) return {};
    check(text.size()<32768,"UTF-8 size");
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    check(size>0,"UTF-8 conversion");
    std::string result(static_cast<size_t>(size),'\0');
    check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr)==size,"UTF-8 conversion");
    return result;
}
std::wstring imagePath(HANDLE process) {
    std::vector<wchar_t> buffer(32768); DWORD length=static_cast<DWORD>(buffer.size());
    check(QueryFullProcessImageNameW(process,0,buffer.data(),&length),"process image");
    return {buffer.data(),length};
}
struct IdentityPacket {
    uint32_t magic=0x31494856,schema=1,processId=0,parentId=0;
    Query query;
    uint64_t window=0;
    uint32_t windowProcess=0,windowThread=0,consoleCount=0;
    DWORD consoleProcesses[16]={};
    uint32_t readReturned=0,readSucceeded=0,readError=0,textCount=0;
    wchar_t text[128]={};
};
static_assert(sizeof(IdentityPacket)==392);
struct Read {
    volatile LONG returned=0;
    BOOL succeeded=FALSE;
    DWORD error=0,count=0;
    wchar_t text[128]={};
};
DWORD WINAPI readLine(void *data) {
    auto &r=*static_cast<Read *>(data);
    SetLastError(ERROR_SUCCESS);
    r.succeeded=ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE),r.text,127,&r.count,nullptr);
    r.error=GetLastError(); InterlockedExchange(&r.returned,1); return 0;
}
int fixture(const std::wstring &name,DWORD expectedServer) {
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    check(pipe.value!=INVALID_HANDLE_VALUE,"fixture pipe");
    ULONG server=0; check(GetNamedPipeServerProcessId(pipe.value,&server) && server==expectedServer,"fixture server");
    check(SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),ENABLE_EXTENDED_FLAGS|ENABLE_PROCESSED_INPUT|ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT),"fixture mode");
    check(FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE)),"fixture input barrier");
    Read read; Handle thread;
    // Preserve reader storage until completion, or exit this owned fixture.
    try {
        const ULONGLONG deadline=GetTickCount64()+60000;
        while(GetTickCount64()<deadline) {
            DWORD available=0; check(PeekNamedPipe(pipe.value,nullptr,0,nullptr,&available,nullptr),"fixture command peek");
            if(!available) { Sleep(2); continue; }
            char command=0; transfer(pipe.value,&command,1,false);
            if(command=='Q') {
                check(thread.value==INVALID_HANDLE_VALUE || WaitForSingleObject(thread.value,1000)==WAIT_OBJECT_0,"read completed before quit");
                return 37;
            }
            if(command=='R') {
                check(thread.value==INVALID_HANDLE_VALUE,"one read per fixture");
                constexpr wchar_t marker[]=L"HOST_INPUT>"; DWORD count=0;
                check(WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),marker,11,&count,nullptr) && count==11,"input marker");
                thread.reset(CreateThread(nullptr,0,readLine,&read,0,nullptr)); check(thread.value!=nullptr,"reader thread");
            } else check(command=='I',"identity fixture command");
            IdentityPacket p;
            p.processId=GetCurrentProcessId(); p.parentId=parentProcessId(p.processId);
            p.query=hostQuery(GetCurrentProcess());
            const HWND window=GetConsoleWindow(); p.window=reinterpret_cast<uintptr_t>(window);
            DWORD windowProcess=0; p.windowThread=GetWindowThreadProcessId(window,&windowProcess); p.windowProcess=windowProcess;
            p.consoleCount=GetConsoleProcessList(p.consoleProcesses,16);
            check(p.consoleCount>0 && p.consoleCount<=16,"console process list bound");
            if(InterlockedCompareExchange(&read.returned,0,0)) {
                p.readReturned=1; p.readSucceeded=read.succeeded ? 1u : 0u;
                p.readError=read.error; p.textCount=read.count;
                check(read.count<=127,"read text bound"); std::copy_n(read.text,read.count,p.text);
            }
            transfer(pipe.value,&p,sizeof(p),true);
        }
        throw std::runtime_error("fixture deadline");
    } catch(...) { ExitProcess(124); }
}
class Session {
    vt7pty_t *pty_=nullptr;
    Handle input_,output_,pipe_;
    bool closed_=false;
public:
    Handle agent,child,host;
    VtState state{80,25};
    std::string raw;
    bool cleanupOkay=true;
    explicit Session(unsigned int index) {
        try {
            auto *config=vt7pty_config_new(0,nullptr); check(config!=nullptr,"config");
            vt7pty_config_set_initial_size(config,80,25); vt7pty_config_set_agent_timeout(config,5000);
            pty_=vt7pty_open(config,nullptr); vt7pty_config_free(config); check(pty_!=nullptr,"open session");
            check(DuplicateHandle(GetCurrentProcess(),vt7pty_agent_process(pty_),GetCurrentProcess(),&agent.value,0,FALSE,DUPLICATE_SAME_ACCESS),"agent handle");
            input_.reset(CreateFileW(vt7pty_conin_name(pty_),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            output_.reset(CreateFileW(vt7pty_conout_name(pty_),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            check(input_.value!=INVALID_HANDLE_VALUE && output_.value!=INVALID_HANDLE_VALUE,"session pipes");
            const std::wstring name=L"\\\\.\\pipe\\VT7Pty-host-identity-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(index);
            pipe_.reset(privateServer(name));
            const std::wstring exe=directory()+L"\\HostIdentityProbe.exe";
            const std::wstring command=quote(exe)+L" --fixture "+quote(name)+L" "+std::to_wstring(GetCurrentProcessId());
            auto *spawn=vt7pty_spawn_config_new(VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN,exe.c_str(),command.c_str(),nullptr,nullptr,nullptr);
            check(spawn!=nullptr,"spawn config");
            const BOOL okay=vt7pty_spawn(pty_,spawn,&child.value,nullptr,nullptr,nullptr);
            vt7pty_spawn_config_free(spawn); check(okay!=FALSE,"spawn fixture");
            connect(pipe_.value);
            ULONG peer=0; check(GetNamedPipeClientProcessId(pipe_.value,&peer) && peer==GetProcessId(child.value),"owned pipe client");
            check(_wcsicmp(imagePath(child.value).c_str(),exe.c_str())==0,"fixture image");
        } catch(...) { cleanup(); throw; }
    }
    ~Session() { cleanup(); }
    IdentityPacket observe(char command='I') {
        transfer(pipe_.value,&command,1,true);
        IdentityPacket p; transfer(pipe_.value,&p,sizeof(p),false);
        check(p.magic==0x31494856 && p.schema==1 && p.processId==GetProcessId(child.value) &&
            p.parentId==GetProcessId(agent.value) && p.window!=0 && p.windowThread!=0 &&
            p.consoleCount>0 && p.consoleCount<=16 && p.readReturned<=1 && p.readSucceeded<=1 && p.textCount<=127,"identity packet");
        const auto contains=[&](DWORD id) { return std::find(p.consoleProcesses,p.consoleProcesses+p.consoleCount,id)!=p.consoleProcesses+p.consoleCount; };
        check(contains(p.processId) && contains(p.parentId) && p.consoleCount==2,"owned console members");
        return p;
    }
    void input(std::string_view text) { transfer(input_.value,const_cast<char *>(text.data()),static_cast<DWORD>(text.size()),true); }
    void pump() {
        for(unsigned int n=0;n<64;++n) {
            DWORD available=0;
            if(!PeekNamedPipe(output_.value,nullptr,0,nullptr,&available,nullptr)) {
                check(GetLastError()==ERROR_BROKEN_PIPE || GetLastError()==ERROR_PIPE_NOT_CONNECTED,"output peek"); return;
            }
            if(!available) return;
            char buffer[4096]; DWORD size=std::min<DWORD>(available,sizeof(buffer));
            transfer(output_.value,buffer,size,false);
            check(raw.size()+size<=1024*1024,"output size"); raw.append(buffer,size); state.feed({buffer,size});
            for(const auto &reply:state.takeReplies()) input(reply);
        }
    }
    bool until(const std::function<bool()> &condition,DWORD timeout=5000) {
        const ULONGLONG end=GetTickCount64()+timeout;
        do { pump(); if(condition()) return true; Sleep(2); } while(GetTickCount64()<end);
        return false;
    }
    void quit() {
        char command='Q'; transfer(pipe_.value,&command,1,true);
        check(until([&] { return WaitForSingleObject(child.value,0)==WAIT_OBJECT_0; }),"fixture quit");
        DWORD code=0; check(GetExitCodeProcess(child.value,&code) && code==37,"fixture exit status");
        check(cleanup(),"session cleanup");
    }
    bool cleanup() noexcept {
        if(closed_) return cleanupOkay;
        closed_=true; pipe_.reset(); input_.reset();
        if(pty_) { vt7pty_free(pty_); pty_=nullptr; }
        for(HANDLE process:{child.value,agent.value}) {
            if(process==nullptr || process==INVALID_HANDLE_VALUE) continue;
            if(WaitForSingleObject(process,5000)!=WAIT_OBJECT_0) {
                cleanupOkay=false; TerminateProcess(process,125); WaitForSingleObject(process,5000);
            }
        }
        output_.reset(); return cleanupOkay;
    }
};
struct TokenIdentity {
    std::vector<unsigned char> user;
    TOKEN_STATISTICS statistics={};
    DWORD integrity=0,session=0;
    explicit TokenIdentity(HANDLE process) {
        Handle token; check(OpenProcessToken(process,TOKEN_QUERY,&token.value),"process token");
        DWORD size=0; GetTokenInformation(token.value,TokenUser,nullptr,0,&size);
        check(size>0 && size<65536,"token size"); user.resize(size);
        check(GetTokenInformation(token.value,TokenUser,user.data(),size,&size),"token user");
        check(GetTokenInformation(token.value,TokenStatistics,&statistics,sizeof(statistics),&size),"token statistics");
        check(GetTokenInformation(token.value,TokenSessionId,&session,sizeof(session),&size),"token session");
        size=0; GetTokenInformation(token.value,TokenIntegrityLevel,nullptr,0,&size);
        check(size>0 && size<65536,"integrity size"); std::vector<unsigned char> storage(size);
        check(GetTokenInformation(token.value,TokenIntegrityLevel,storage.data(),size,&size),"integrity level");
        auto *sid=reinterpret_cast<TOKEN_MANDATORY_LABEL *>(storage.data())->Label.Sid;
        check(IsValidSid(sid) && *GetSidSubAuthorityCount(sid)>0,"integrity SID");
        integrity=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1);
    }
    bool same(const TokenIdentity &other) const {
        return EqualSid(reinterpret_cast<const TOKEN_USER *>(user.data())->User.Sid,
            reinterpret_cast<const TOKEN_USER *>(other.user.data())->User.Sid) &&
            statistics.AuthenticationId.LowPart==other.statistics.AuthenticationId.LowPart &&
            statistics.AuthenticationId.HighPart==other.statistics.AuthenticationId.HighPart &&
            integrity==other.integrity && session==other.session;
    }
};
uint64_t creation(HANDLE process) {
    FILETIME c={},e={},k={},u={}; check(GetProcessTimes(process,&c,&e,&k,&u),"process creation time");
    return (static_cast<uint64_t>(c.dwHighDateTime)<<32)|c.dwLowDateTime;
}
std::string guiThreads(DWORD pid) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0)); check(snapshot.value!=INVALID_HANDLE_VALUE,"thread snapshot");
    THREADENTRY32 entry={}; entry.dwSize=sizeof(entry); check(Thread32First(snapshot.value,&entry),"first thread");
    std::string result="["; unsigned int count=0;
    do {
        if(entry.th32OwnerProcessID!=pid) continue;
        check(++count<=128,"host thread bound");
        if(count>1) result+=',';
        Handle thread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ThreadID));
        const DWORD openError=thread.value ? 0 : GetLastError();
        const bool belongs=thread.value && GetProcessIdOfThread(thread.value)==pid;
        GUITHREADINFO gui={}; gui.cbSize=sizeof(gui); SetLastError(ERROR_SUCCESS);
        const BOOL okay=belongs ? GetGUIThreadInfo(entry.th32ThreadID,&gui) : FALSE;
        const DWORD error=belongs ? (okay ? 0 : GetLastError()) : openError;
        result+="{\"ThreadId\":"+std::to_string(entry.th32ThreadID)+",\"OwnedThreadVerified\":"+(belongs ? "true" : "false")+
            ",\"GuiInfoSucceeded\":"+(okay ? "true" : "false")+",\"Error\":"+std::to_string(error)+"}";
    } while(Thread32Next(snapshot.value,&entry));
    check(GetLastError()==ERROR_NO_MORE_FILES,"thread enumeration");
    return result+"]";
}
std::string packetJson(const IdentityPacket &p) {
    std::string members="[";
    for(uint32_t i=0;i<p.consoleCount;++i) { if(i) members+=','; members+=std::to_string(p.consoleProcesses[i]); }
    return "{\"ProcessId\":"+std::to_string(p.processId)+",\"ParentId\":"+std::to_string(p.parentId)+
        ",\"SelfQuery\":"+queryJson(p.query)+",\"Window\":"+std::to_string(p.window)+
        ",\"WindowProcessId\":"+std::to_string(p.windowProcess)+",\"WindowThreadId\":"+std::to_string(p.windowThread)+
        ",\"ConsoleMembers\":"+members+"],\"ReadReturned\":"+std::to_string(p.readReturned)+
        ",\"ReadSucceeded\":"+std::to_string(p.readSucceeded)+",\"ReadError\":"+std::to_string(p.readError)+
        ",\"Text\":"+json(utf8({p.text,p.textCount}))+"}";
}
bool observeIdentity(Session &s,const std::wstring &output,const wchar_t *label) {
    const IdentityPacket packet=s.observe();
    const Query child=hostQuery(s.child.value),agent=hostQuery(s.agent.value);
    std::string record="{\"Fixture\":"+packetJson(packet)+",\"ChildQuery\":"+queryJson(child)+",\"AgentQuery\":"+queryJson(agent);
    bool eligible=false; std::string error;
    try {
        check(WaitForSingleObject(s.child.value,0)==WAIT_TIMEOUT && WaitForSingleObject(s.agent.value,0)==WAIT_TIMEOUT,"live owned clients");
        check(agrees(packet.query,child) && agrees(child,agent),"consistent host identity");
        // libuv v1.44.2 tty.c also removes the low two flag bits before using the PID.
        const DWORD pid=static_cast<DWORD>(child.raw&~ULONG_PTR{3});
        check(pid!=GetCurrentProcessId() && pid!=packet.processId && pid!=packet.parentId,"distinct host process");
        if(s.host.value==INVALID_HANDLE_VALUE) s.host.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        if(!s.host.value) throw std::runtime_error("host query access denied or unavailable: "+std::to_string(GetLastError()));
        check(GetProcessId(s.host.value)==pid,"retained host query handle");
        check(WaitForSingleObject(s.host.value,0)==WAIT_TIMEOUT,"live host");
        const std::wstring path=imagePath(s.host.value);
        wchar_t system[MAX_PATH]={}; const UINT length=GetSystemDirectoryW(system,MAX_PATH);
        check(length>0 && length<MAX_PATH,"system path");
        check(_wcsicmp(path.c_str(),(std::wstring(system)+L"\\conhost.exe").c_str())==0,"System32 host image");
        const TokenIdentity ours(GetCurrentProcess()),host(s.host.value),client(s.child.value),owner(s.agent.value);
        check(ours.same(host) && ours.same(client) && ours.same(owner),"same user logon integrity and session");
        BOOL wow=TRUE; check(IsWow64Process(s.host.value,&wow) && !wow,"native x64 host");
        check(creation(s.host.value)<=creation(s.child.value),"host predates fixture");
        record+=",\"HostProcessId\":"+std::to_string(pid)+",\"HostImage\":"+json(utf8(path))+
            ",\"HostCreationTime\":"+json(std::to_string(creation(s.host.value)))+
            ",\"SessionId\":"+std::to_string(host.session)+",\"IntegrityRid\":"+std::to_string(host.integrity)+
            ",\"SameUserLogonIntegritySession\":true,\"NativeX64\":true,\"Threads\":"+guiThreads(pid);
        check(agrees(hostQuery(s.child.value),child) && agrees(hostQuery(s.agent.value),agent),"stable host association");
        eligible=true;
    } catch(const std::exception &e) { error=e.what(); }
    record+=",\"IdentityEligible\":"+std::string(eligible ? "true" : "false")+
        ",\"DispatchThreadVerified\":false,\"Error\":"+json(error)+"}";
    writeFile(output+L"\\"+label+L".json",record);
    return eligible;
}
void oracle() {
    const Query good{0,sizeof(ULONG_PTR),400};
    for(ULONG_PTR flags=0;flags<4;++flags) {
        const Query q{0,sizeof(ULONG_PTR),400|flags};
        check(valid(q) && agrees(q,q) && (q.raw&~ULONG_PTR{3})==400,"oracle flagged PID");
    }
    std::array<Query,5> bad={Query{static_cast<LONG>(0xc0000003u),sizeof(ULONG_PTR),400},
        Query{0,4,400},Query{0,sizeof(ULONG_PTR),0},Query{0,sizeof(ULONG_PTR),0x100000000ull},Query{0,sizeof(ULONG_PTR),3}};
    for(const auto &q:bad) check(!valid(q) && !agrees(good,q),"oracle invalid query");
    check(!agrees(good,Query{0,sizeof(ULONG_PTR),404}),"oracle different host");
    puts("Host identity oracle: 10 checks passed");
}
void startRead(Session &s,const std::string &text) {
    check(!s.observe('R').readReturned,"read starts pending"); s.input(text);
    std::u32string expected=U"HOST_INPUT>"; for(unsigned char c:text) expected+=c;
    check(s.until([&] { return s.state.contains(expected); }),"pending input echo");
    check(!s.observe().readReturned,"echoed read still pending");
}
void finishRead(Session &s,const std::wstring &text,const std::wstring &output,const wchar_t *label) {
    s.input("\r"); IdentityPacket p;
    check(s.until([&] { p=s.observe(); return p.readReturned!=0; }),"ordinary read completion");
    writeFile(output+L"\\"+label+L".json",packetJson(p));
    check(p.readSucceeded && p.readError==0 && std::wstring(p.text,p.textCount)==text+L"\r\n","isolated read text");
}
int run(const std::wstring &output) {
    std::array<std::unique_ptr<Session>,3> sessions;
    std::string error; bool identity=true,isolation=false,cleaned=true,hostExited=true;
    unsigned int observations=0;
    auto observe=[&](Session &s,const wchar_t *label) {
        identity=observeIdentity(s,output,label) && identity; ++observations;
    };
    try {
        oracle();
        sessions[0]=std::make_unique<Session>(0); sessions[1]=std::make_unique<Session>(1);
        auto &a=*sessions[0]; auto &b=*sessions[1];
        observe(a,L"A-initial"); observe(b,L"B-initial");
        if(identity) check(GetProcessId(a.host.value)!=GetProcessId(b.host.value),"separate live hosts");
        startRead(a,"ALPHA"); startRead(b,"BRAVO_KEEP");
        observe(a,L"A-pending"); observe(b,L"B-pending");
        finishRead(a,L"ALPHA",output,L"A-read"); a.quit();
        if(a.host.value && a.host.value!=INVALID_HANDLE_VALUE) hostExited=WaitForSingleObject(a.host.value,5000)==WAIT_OBJECT_0 && hostExited;
        check(WaitForSingleObject(a.child.value,0)==WAIT_OBJECT_0 && WaitForSingleObject(a.agent.value,0)==WAIT_OBJECT_0,"stale owned clients rejected");
        writeFile(output+L"\\A-after-close.json","{\"ChildSignaled\":true,\"AgentSignaled\":true,\"EligibleForDispatch\":false}");
        check(!b.observe().readReturned,"B read remains pending after A closes");
        sessions[2]=std::make_unique<Session>(2); auto &c=*sessions[2];
        observe(c,L"C-replacement"); observe(b,L"B-after-replacement");
        if(identity) check(GetProcessId(c.host.value)!=GetProcessId(b.host.value),"replacement host separate from survivor");
        startRead(c,"CHARLIE"); finishRead(c,L"CHARLIE",output,L"C-read"); c.quit();
        check(!b.observe().readReturned,"B read pending through C lifetime");
        finishRead(b,L"BRAVO_KEEP",output,L"B-read"); b.quit(); isolation=true;
    } catch(const std::exception &e) { error=e.what(); }
    for(size_t i=0;i<sessions.size();++i) if(sessions[i]) {
        cleaned=sessions[i]->cleanup() && cleaned;
        if(sessions[i]->host.value && sessions[i]->host.value!=INVALID_HANDLE_VALUE)
            hostExited=(WaitForSingleObject(sessions[i]->host.value,5000)==WAIT_OBJECT_0) && hostExited;
        writeFile(output+L"\\"+static_cast<wchar_t>(L'A'+i)+L"-stream.bin",sessions[i]->raw);
    }
    identity=identity && observations==6;
    const bool passed=error.empty() && identity && isolation && cleaned && hostExited;
    writeFile(output+L"\\probe.json","{\"SchemaVersion\":1,\"Probe\":\"HostIdentity\",\"Version\":1,\"Status\":"+json(passed ? "Pass" : "Fail")+
        ",\"IdentityObservations\":"+std::to_string(observations)+",\"IdentityEligible\":"+(identity ? "true" : "false")+
        ",\"SessionIsolationPassed\":"+(isolation ? "true" : "false")+
        ",\"OwnedProcessesCleaned\":"+(cleaned ? "true" : "false")+",\"ObservedHostsExited\":"+(hostExited ? "true" : "false")+
        ",\"CancellationImplemented\":false,\"DispatchThreadVerified\":false,\"Error\":"+json(error)+"}");
    printf("Host identity probe: %s\n",passed ? "Pass" : "Fail"); return passed ? 0 : 1;
}
}
int wmain(int argc,wchar_t **argv) {
    try {
        if(argc==4 && std::wstring_view(argv[1])==L"--fixture") return fixture(argv[2],wcstoul(argv[3],nullptr,10));
        if(argc==2 && std::wstring_view(argv[1])==L"--self-test") { oracle(); return 0; }
        if(argc==2) return run(argv[1]);
        return 2;
    } catch(const std::exception &e) { fprintf(stderr,"Host identity harness error: %s\n",e.what()); return 2; }
}
