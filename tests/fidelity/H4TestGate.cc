// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
// Test-only, thread-scoped interception of the retained console host.
// It observes keyboard state and bounds a pause; it never writes keyboard state.
#include "H4TestGate.h"
#include <string>
#include <vector>
namespace
{
HMODULE module = nullptr;
HANDLE mapping = nullptr, ready = nullptr, entered = nullptr, release = nullptr,
       finished = nullptr, controller = nullptr;
h4gate::State *state = nullptr;
HWND window = nullptr;
bool outer = false;
uint64_t created(HANDLE process)
{
    FILETIME c = {}, e = {}, k = {}, u = {};
    if (!GetProcessTimes(process, &c, &e, &k, &u)) return 0;
    return (static_cast<uint64_t>(c.dwHighDateTime) << 32) | c.dwLowDateTime;
}
bool sameToken(HANDLE other)
{
    HANDLE a = nullptr, b = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &a)) return false;
    if (!OpenProcessToken(other, TOKEN_QUERY, &b)) { CloseHandle(a); return false; }
    alignas(TOKEN_USER) BYTE ua[512] = {}, ub[512] = {};
    alignas(TOKEN_MANDATORY_LABEL) BYTE ia[512] = {}, ib[512] = {};
    TOKEN_STATISTICS sa = {}, sb = {};
    DWORD na = 0, nb = 0, xa = 0, xb = 0;
    const bool ok =
        GetTokenInformation(a, TokenUser, ua, sizeof(ua), &na) &&
        GetTokenInformation(b, TokenUser, ub, sizeof(ub), &nb) &&
        GetTokenInformation(a, TokenIntegrityLevel, ia, sizeof(ia), &na) &&
        GetTokenInformation(b, TokenIntegrityLevel, ib, sizeof(ib), &nb) &&
        GetTokenInformation(a, TokenStatistics, &sa, sizeof(sa), &na) &&
        GetTokenInformation(b, TokenStatistics, &sb, sizeof(sb), &nb) &&
        GetTokenInformation(a, TokenSessionId, &xa, sizeof(xa), &na) &&
        GetTokenInformation(b, TokenSessionId, &xb, sizeof(xb), &nb);
    bool equal = false;
    if (ok)
    {
        const auto sidA = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(ia)->Label.Sid;
        const auto sidB = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(ib)->Label.Sid;
        equal = EqualSid(reinterpret_cast<TOKEN_USER *>(ua)->User.Sid,
                         reinterpret_cast<TOKEN_USER *>(ub)->User.Sid) &&
                EqualSid(sidA, sidB) && xa == xb &&
                sa.AuthenticationId.LowPart == sb.AuthenticationId.LowPart &&
                sa.AuthenticationId.HighPart == sb.AuthenticationId.HighPart &&
                *GetSidSubAuthority(sidA, *GetSidSubAuthorityCount(sidA)-1) ==
                    SECURITY_MANDATORY_MEDIUM_RID;
    }
    CloseHandle(a); CloseHandle(b);
    return equal;
}
void initialize(const CWPSTRUCT &m)
{
    if (state || m.lParam <= 0 || m.wParam < 0xc000 || m.wParam > 0xffff) return;
    wchar_t name[128] = {};
    if (!GlobalGetAtomNameW(static_cast<ATOM>(m.wParam), name, 128)) return;
    const std::wstring id(name), prefix(h4gate::prefix);
    if (id.size() != prefix.size()+32 || id.compare(0,prefix.size(),prefix) != 0 ||
        id.find_first_not_of(L"0123456789abcdef",prefix.size()) != std::wstring::npos) return;
    const auto full=L"Local\\"+id;
    HANDLE map=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,full.c_str());
    if (!map) return;
    auto *candidate=static_cast<h4gate::State *>(MapViewOfFile(map,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(h4gate::State)));
    if (!candidate) { CloseHandle(map); return; }
    DWORD pid=0;
    const DWORD tid=GetWindowThreadProcessId(m.hwnd,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,
                               static_cast<DWORD>(m.lParam));
    const bool eligible=candidate->magic==h4gate::signature &&
        candidate->bytes==sizeof(h4gate::State) && candidate->mode>=1 && candidate->mode<=3 &&
        candidate->controller==static_cast<DWORD>(m.lParam) &&
        candidate->host==GetCurrentProcessId() && candidate->thread==GetCurrentThreadId() &&
        candidate->window==reinterpret_cast<uintptr_t>(m.hwnd) &&
        candidate->hostCreated==created(GetCurrentProcess()) &&
        pid==GetCurrentProcessId() && tid==GetCurrentThreadId() &&
        process && candidate->controllerCreated==created(process) &&
        WaitForSingleObject(process,0)==WAIT_TIMEOUT && sameToken(process);
    if (!eligible)
    {
        if(process) CloseHandle(process);
        UnmapViewOfFile(candidate); CloseHandle(map); return;
    }
    HANDLE r=OpenEventW(EVENT_MODIFY_STATE,FALSE,(full+L"-ready").c_str());
    HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,(full+L"-entered").c_str());
    HANDLE q=OpenEventW(SYNCHRONIZE,FALSE,(full+L"-release").c_str());
    HANDLE f=OpenEventW(EVENT_MODIFY_STATE,FALSE,(full+L"-finished").c_str());
    HMODULE pinned=nullptr;
    if (!r || !e || !q || !f ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCWSTR>(module),&pinned))
    {
        for(HANDLE h:{r,e,q,f,process}) if(h) CloseHandle(h);
        UnmapViewOfFile(candidate); CloseHandle(map); return;
    }
    mapping=map; state=candidate; controller=process; ready=r; entered=e; release=q; finished=f;
    window=m.hwnd;
    state->actualHost=GetCurrentProcessId(); state->actualThread=GetCurrentThreadId();
    InterlockedExchange(&state->initialized,1); SetEvent(ready);
}
void hold()
{
    if(InterlockedCompareExchange(&state->entered,1,0)!=0) return;
    state->enteredTick=GetTickCount64();
    SetEvent(entered);
    const HANDLE waits[]={release,controller};
    if(WaitForMultipleObjects(2,waits,FALSE,5000)!=WAIT_OBJECT_0)
        InterlockedExchange(&state->error,ERROR_TIMEOUT);
    state->releasedTick=GetTickCount64();
}
bool productMessage(const CWPSTRUCT &m)
{
    if(m.message!=WM_NULL || m.hwnd!=window || m.lParam!=state->agent ||
        m.wParam<0xc000 || m.wParam>0xffff) return false;
    wchar_t text[256]={};
    if(!GlobalGetAtomNameW(static_cast<ATOM>(m.wParam),text,256)) return false;
    const auto prefix=L"VT7Pty-H4-"+std::to_wstring(state->agent)+L"-";
    return std::wstring(text).starts_with(prefix);
}
}
extern "C" __declspec(dllexport) LRESULT CALLBACK H4TestGateCallback(int code,WPARAM wp,LPARAM lp)
{
    if(code<0 || !lp) return CallNextHookEx(nullptr,code,wp,lp);
    const auto &m=*reinterpret_cast<const CWPSTRUCT *>(lp);
    try
    {
        if(m.message==RegisterWindowMessageW(h4gate::initName)) initialize(m);
        if(state && productMessage(m) && !outer && !state->finished)
        {
            state->beforeValid=GetKeyboardState(state->before);
            outer=true;
            if(state->mode==1) hold();
            const auto result=CallNextHookEx(nullptr,code,wp,lp);
            outer=false;
            state->afterValid=GetKeyboardState(state->after);
            InterlockedExchange(&state->finished,1); SetEvent(finished);
            return result;
        }
        if(state && outer && m.hwnd==window && m.message==WM_KEYDOWN && m.wParam=='C')
        {
            InterlockedIncrement(&state->cMessages);
            state->duringValid=GetKeyboardState(state->during);
            if(state->mode>=2) hold();
        }
    }
    catch(...) { if(state) InterlockedExchange(&state->error,ERROR_GEN_FAILURE); }
    return CallNextHookEx(nullptr,code,wp,lp);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void *)
{
    if(reason==DLL_PROCESS_ATTACH) module=instance;
    return TRUE;
}
