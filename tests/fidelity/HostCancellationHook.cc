// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
// Research only: private-desktop, thread-local key processing. No production use.
#include "HostCancellationResearch.h"
#include "HostCancellationProtocol.h"
using namespace fidelity;
using namespace cancelresearch;
using namespace hostcancel;
namespace {
HMODULE module=nullptr;
HANDLE pipe=INVALID_HANDLE_VALUE,controller=nullptr,agent=nullptr,fixtureProcess=nullptr;
Request binding;
uint64_t lastSequence=0;
ATOM channelAtom=0;
HWND target=nullptr;
bool bound=false,retired=false;
volatile LONG entered=0;
uint32_t validationStage=0;
void closeChannel() noexcept {
    for(HANDLE h:{pipe,controller,agent,fixtureProcess}) if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    pipe=INVALID_HANDLE_VALUE; controller=agent=fixtureProcess=nullptr;
    retired=true;
}
bool processOkay(HANDLE h,uint32_t pid,uint64_t created) {
    return h && h!=INVALID_HANDLE_VALUE && GetProcessId(h)==pid && creation(h)==created && WaitForSingleObject(h,0)==WAIT_TIMEOUT;
}
bool ordinaryDesktop() {
    wchar_t station[256]={},desktop[256]={},active[256]={}; DWORD size=0;
    const bool named=GetUserObjectInformationW(GetProcessWindowStation(),UOI_NAME,station,sizeof(station),&size) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&size) &&
        _wcsicmp(station,L"WinSta0")==0 && wcsncmp(desktop,L"VT7Pty-H3-",10)==0;
    if(!named) return false;
    const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!input) return false;
    const bool privateDesktop=GetUserObjectInformationW(input,UOI_NAME,active,sizeof(active),&size) && _wcsicmp(active,desktop)!=0;
    CloseDesktop(input); return privateDesktop;
}
void sendResponse(Result result,const Request *request=nullptr,const KeyboardEvidence *keyboard=nullptr) {
    Response response; if(keyboard) response.keyboard=*keyboard;
    DWORD desktopBytes=0;
    if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,response.desktop,sizeof(response.desktop),&desktopBytes)) {
        closeChannel(); return;
    }
    response.result=result; response.host=GetCurrentProcessId(); response.thread=GetCurrentThreadId();
    response.hostCreated=creation(GetCurrentProcess()); response.window=reinterpret_cast<uintptr_t>(target);
    response.nonce[0]=binding.nonce[0]; response.nonce[1]=binding.nonce[1];
    response.acceptedSequence=lastSequence; response.validationStage=validationStage;
    DWORD windowOwner=0; response.windowThread=GetWindowThreadProcessId(target,&windowOwner); response.windowOwner=windowOwner;
    if(request) { response.sequence=request->sequence; response.operation=static_cast<uint32_t>(request->operation); }
    DWORD written=0;
    // PIPE_NOWAIT prevents the GUI callback waiting for a reader or buffer space.
    if(!WriteFile(pipe,&response,sizeof(response),&written,nullptr) || written!=sizeof(response)) closeChannel();
}
void openChannel(const CWPSTRUCT &message) {
    if(message.wParam<0xc000 || message.wParam>0xffff || message.lParam<=0 || static_cast<uint64_t>(message.lParam)>MAXDWORD) return;
    wchar_t name[256]={};
    const ATOM atom=static_cast<ATOM>(message.wParam);
    if(!GlobalGetAtomNameW(atom,name,256)) return;
    const DWORD pid=static_cast<DWORD>(message.lParam);
    const std::wstring prefix=L"VT7Pty-H3-"+std::to_wstring(pid)+L"-";
    const std::wstring value(name);
    if(value.size()!=prefix.size()+32 || value.compare(0,prefix.size(),prefix)!=0 ||
        value.find_first_not_of(L"0123456789abcdef",prefix.size())!=std::wstring::npos) return;
    Handle candidate(CreateFileW((L"\\\\.\\pipe\\"+value).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
    if(candidate.value==INVALID_HANDLE_VALUE) return; // Never WaitNamedPipe.
    DWORD mode=PIPE_READMODE_BYTE|PIPE_NOWAIT;
    check(SetNamedPipeHandleState(candidate.value,&mode,nullptr,nullptr),"nonblocking callback pipe");
    ULONG server=0;
    check(GetNamedPipeServerProcessId(candidate.value,&server) && server==pid,"callback server PID");
    Handle owner(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
    check(owner.value && WaitForSingleObject(owner.value,0)==WAIT_TIMEOUT,"live controller");
    std::vector<wchar_t> dllPath(32768);
    const DWORD length=GetModuleFileNameW(module,dllPath.data(),32768); check(length>0 && length<32768,"callback module path");
    const std::wstring path(dllPath.data(),length);
    const auto expected=path.substr(0,path.find_last_of(L'\\'))+L"\\HostCancellationProbe.exe";
    check(_wcsicmp(imagePath(owner.value).c_str(),expected.c_str())==0,"controller beside exact hook DLL");
    const TokenIdentity ours(GetCurrentProcess()),theirs(owner.value);
    check(ours.same(theirs) && ours.integrity==SECURITY_MANDATORY_MEDIUM_RID && ordinaryDesktop(),"callback desktop and token policy");
    for(size_t i=0;i<2;++i) binding.nonce[i]=_wcstoui64(value.substr(prefix.size()+i*16,16).c_str(),nullptr,16);
    binding.identity.controller=pid; binding.identity.controllerCreated=creation(owner.value);
    controller=owner.value; owner.value=INVALID_HANDLE_VALUE;
    pipe=candidate.value; candidate.value=INVALID_HANDLE_VALUE;
    channelAtom=atom; target=message.hwnd;
    sendResponse(Result::Hello);
}
bool bindRequest(const Request &r) {
    if(r.signature!=magic || r.schema!=version || r.bytes!=sizeof(r) || r.operation!=Op::Bind || r.sequence!=1 ||
        r.nonce[0]!=binding.nonce[0] || r.nonce[1]!=binding.nonce[1] ||
        r.identity.controller!=binding.identity.controller || r.identity.controllerCreated!=binding.identity.controllerCreated ||
        r.identity.host!=GetCurrentProcessId() || r.identity.thread!=GetCurrentThreadId() ||
        r.identity.hostCreated!=creation(GetCurrentProcess()) || r.identity.window!=reinterpret_cast<uintptr_t>(target) ||
        r.deadline<=GetTickCount64() || r.deadline-GetTickCount64()>5000) return false;
    validationStage=1;
    Handle controllerThread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,r.identity.controllerThread));
    check(controllerThread.value && GetProcessIdOfThread(controllerThread.value)==r.identity.controller,"controller thread owner");
    wchar_t oursDesktop[256]={},controllerDesktop[256]={}; DWORD desktopBytes=0;
    check(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,oursDesktop,sizeof(oursDesktop),&desktopBytes) &&
        GetUserObjectInformationW(GetThreadDesktop(r.identity.controllerThread),UOI_NAME,controllerDesktop,sizeof(controllerDesktop),&desktopBytes) &&
        wcscmp(oursDesktop,controllerDesktop)==0,"same private research desktop");
    validationStage=2;
    Handle a(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,r.identity.agent));
    Handle f(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,r.identity.fixture));
    check(processOkay(a.value,r.identity.agent,r.identity.agentCreated) && processOkay(f.value,r.identity.fixture,r.identity.fixtureCreated),"binding live clients");
    validationStage=3;
    check(parentProcessId(r.identity.agent)==r.identity.controller && parentProcessId(r.identity.fixture)==r.identity.agent,"binding owned descendants");
    validationStage=4;
    const Query aq=hostQuery(a.value),fq=hostQuery(f.value);
    check(agrees(aq,fq) && (aq.raw&~ULONG_PTR{3})==GetCurrentProcessId(),"binding actual console host");
    validationStage=5;
    DWORD owner=0; const DWORD windowThread=GetWindowThreadProcessId(target,&owner);
    check(owner==GetCurrentProcessId() && windowThread==GetCurrentThreadId(),"binding actual console window dispatch");
    validationStage=6;
    const TokenIdentity ours(GetCurrentProcess()),at(a.value),ft(f.value);
    check(ours.same(at) && ours.same(ft),"binding client tokens");
    binding=r; agent=a.value; a.value=INVALID_HANDLE_VALUE; fixtureProcess=f.value; f.value=INVALID_HANDLE_VALUE;
    bound=true; lastSequence=1; validationStage=0; return true;
}
// Scope holds all 256 original bytes. A deliberately thrown failure traverses
// this same restoration path before evidence is sent to the controller.
struct KeyboardScope {
    KeyboardEvidence &e;
    explicit KeyboardScope(KeyboardEvidence &value):e(value) {
        e.captured=GetKeyboardState(e.before)!=FALSE;
        check(e.captured!=0,"capture keyboard state");
    }
    ~KeyboardScope() {
        if(!e.applied) return;
        e.restored=SetKeyboardState(e.before)!=FALSE;
        if(!e.restored) e.error=GetLastError();
        e.equal=GetKeyboardState(e.after) && memcmp(e.before,e.after,256)==0;
    }
    void apply() {
        BYTE keys[256]; memcpy(keys,e.before,256);
        for(unsigned int vk:{VK_SHIFT,VK_LSHIFT,VK_RSHIFT,VK_CONTROL,VK_LCONTROL,VK_RCONTROL,VK_MENU,VK_LMENU,VK_RMENU,VK_LWIN,VK_RWIN})
            keys[vk]&=1;
        keys[VK_CONTROL]|=0x80; keys[VK_LCONTROL]|=0x80;
        e.applied=SetKeyboardState(keys)!=FALSE;
        check(e.applied!=0,"temporary thread keyboard state");
        check(GetKeyboardState(e.during) && memcmp(keys,e.during,256)==0 && (e.during[VK_LCONTROL]&0x80) && (GetKeyState(VK_CONTROL)&0x8000),"applied Ctrl state");
    }
};
KeyboardEvidence cancelRead(bool failureControl) {
    KeyboardEvidence e;
    try {
        KeyboardScope scope(e); scope.apply();
        if(failureControl) { e.injectedFailure=1; throw std::runtime_error("injected before key delivery"); }
        // Same verified host thread, same HWND. No queue pumping, global input,
        // AttachThreadInput, generated console event, or WM_CHAR duplication.
        const UINT scan=MapVirtualKeyW('C',MAPVK_VK_TO_VSC);
        check(scan!=0 && scan<256,"C scan code");
        SendMessageW(target,WM_KEYDOWN,'C',static_cast<LPARAM>((scan<<16)|1u));
        e.sent=1;
    } catch(...) { if(!e.injectedFailure && !e.error) e.error=GetLastError() ? GetLastError() : ERROR_GEN_FAILURE; }
    return e;
}
void dispatch(const CWPSTRUCT &message) {
    if(retired || !ordinaryDesktop()) return;
    if(pipe==INVALID_HANDLE_VALUE) { openChannel(message); return; }
    if(message.wParam!=channelAtom || message.lParam!=binding.identity.controller || message.hwnd!=target) return;
    if(WaitForSingleObject(controller,0)!=WAIT_TIMEOUT) { closeChannel(); return; }
    DWORD available=0;
    if(!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)) { closeChannel(); return; }
    if(available==0) return;
    if(available!=sizeof(Request)) { closeChannel(); return; } // Fixed framing, no partial reads or waits.
    Request request; DWORD read=0;
    if(!ReadFile(pipe,&request,sizeof(request),&read,nullptr) || read!=sizeof(request)) { closeChannel(); return; }
    Result result=Result::Identity;
    if(!bound) {
        try { if(bindRequest(request)) result=Result::Accepted; } catch(...) { sendResponse(Result::Identity,&request); closeChannel(); return; }
    } else {
        result=validate(request,binding,lastSequence,GetTickCount64(),retired);
        if(result==Result::Accepted) {
            if(!processOkay(agent,binding.identity.agent,binding.identity.agentCreated) ||
                !processOkay(fixtureProcess,binding.identity.fixture,binding.identity.fixtureCreated) ||
                GetCurrentProcessId()!=binding.identity.host || GetCurrentThreadId()!=binding.identity.thread ||
                !agrees(hostQuery(agent),hostQuery(fixtureProcess)) ||
                (hostQuery(agent).raw&~ULONG_PTR{3})!=binding.identity.host) result=Result::Identity;
        }
        if(result==Result::Accepted && request.operation==Op::DeniedControl) result=Result::AccessDenied;
        if(result==Result::Accepted) lastSequence=request.sequence;
    }
    if(result==Result::Accepted && request.operation==Op::SuppressAck) { sendResponse(Result::Received,&request); return; } // Explicit test-only ambiguity control.
    if(result==Result::Accepted && (request.operation==Op::Cancel || request.operation==Op::RestoreControl || request.operation==Op::CancelSuppressAck || request.operation==Op::CancelClose)) {
        if(request.operation==Op::CancelSuppressAck) sendResponse(Result::Received,&request);
        const auto keyboard=cancelRead(request.operation==Op::RestoreControl);
        if(request.operation!=Op::CancelSuppressAck) sendResponse(result,&request,&keyboard);
        if(request.operation==Op::CancelSuppressAck) retired=true; // Keep the pipe open for the explicit final-ack timeout.
        if(!keyboard.restored || !keyboard.equal || keyboard.error || request.operation==Op::CancelClose) closeChannel();
    } else sendResponse(result,&request);
    if(result==Result::Accepted && request.operation==Op::Close) closeChannel();
}
}
extern "C" __declspec(dllexport) LRESULT CALLBACK HostCancellationCallback(int code,WPARAM wParam,LPARAM lParam) {
    if(code>=0 && lParam) {
        const auto &message=*reinterpret_cast<const CWPSTRUCT *>(lParam);
        if(message.message==WM_NULL && InterlockedCompareExchange(&entered,1,0)==0) {
            try { dispatch(message); } catch(...) { closeChannel(); }
            InterlockedExchange(&entered,0);
        }
    }
    return CallNextHookEx(nullptr,code,wParam,lParam);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void *) {
    if(reason==DLL_PROCESS_ATTACH) module=instance;
    // Process-scoped handles are closed by explicit retirement or process exit.
    // No allocation, IPC, waits or manual remote unload under the loader lock.
    return TRUE;
}
