// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
// Compiled only into the separately named research agent.
#include "HostFreezeProtocol.h"
#include "WinTest.h"
#include "../../src/agent/NamedPipe.h"
#include "../../src/agent/Win32Console.h"
#include <functional>
namespace h3freeze {
struct AgentScope {
    Win32Console &console; Kind end;
    AgentScope(Win32Console &c,Kind first,Kind last,int cols=0,int rows=0):console(c),end(last) { record(first,c.frozen(),cols,rows); }
    ~AgentScope() { record(end,console.frozen()); }
};
class AgentControl {
    NamedPipe &pipe_;
    Win32Console &console_;
    std::function<void(int,int)> resize_;
    fidelity::Handle owner_;
    uint64_t nonce_[2]={},ownerCreated_=0,created_=0,last_=0,lease_=0,expires_=0;
    DWORD ownerPid_=0;
    Command mode_=Command::Snapshot;
    bool enabled_=false,held_=false,active_=false;
    int pendingCols_=0,pendingRows_=0;
    static uint64_t creation(HANDLE h) {
        FILETIME a={},b={},c={},d={}; fidelity::check(GetProcessTimes(h,&a,&b,&c,&d),"research process time");
        return (uint64_t{a.dwHighDateTime}<<32)|a.dwLowDateTime;
    }
    void release(Kind reason) {
        record(reason,console_.frozen()); held_=false; active_=false; lease_=0; expires_=0;
        // Preserve the agent's existing unfreeze operation, including its cached-state behavior.
        if(console_.frozen()) console_.setFrozen(false);
        if(pendingCols_) { const int c=pendingCols_,r=pendingRows_; pendingCols_=pendingRows_=0; resize_(c,r); }
    }
public:
    AgentControl(NamedPipe &pipe,Win32Console &console,std::function<void(int,int)> resize):pipe_(pipe),console_(console),resize_(std::move(resize)) {
        wchar_t text[64]={}; const DWORD n=GetEnvironmentVariableW(L"VT7PTY_H3_EXPERIMENT_NONCE",text,64);
        if(!n) return;
        fidelity::check(n==32 && std::wstring(text).find_first_not_of(L"0123456789abcdef")==std::wstring::npos,"research launch nonce");
        wchar_t desktop[256]={}; DWORD bytes=0;
        fidelity::check(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&bytes) &&
            wcsncmp(desktop,L"VT7Pty-H3-",10)==0,"research agent private desktop");
        ownerPid_=fidelity::parentProcessId(GetCurrentProcessId());
        owner_.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,ownerPid_));
        if(!owner_.value) throw std::runtime_error("research controller handle");
        std::vector<wchar_t> image(32768); DWORD length=static_cast<DWORD>(image.size());
        fidelity::check(QueryFullProcessImageNameW(owner_.value,0,image.data(),&length),"research controller image");
        #if defined(VT7PTY_H3_GUARDS)
        const auto expected=fidelity::directory()+L"\\HostCancellationGuardProbe.exe";
#elif defined(VT7PTY_H3_COORDINATED)
        const auto expected=fidelity::directory()+L"\\HostCancellationCoordinatedProbe.exe";
#else
        const auto expected=fidelity::directory()+L"\\HostCancellationFreezeProbe.exe";
#endif
        fidelity::check(_wcsicmp(std::wstring(image.data(),length).c_str(),expected.c_str())==0,"exact research controller");
        ownerCreated_=creation(owner_.value); created_=creation(GetCurrentProcess());
        nonce_[0]=_wcstoui64(std::wstring(text,16).c_str(),nullptr,16); nonce_[1]=_wcstoui64(text+16,nullptr,16);
        pipe_.openServerPipe((L"\\\\.\\pipe\\VT7Pty-H3-freeze-"+std::to_wstring(GetCurrentProcessId())+L"-"+text).c_str(),NamedPipe::OpenMode::Duplex,65536,65536);
        pipe_.setReadBufferSize(sizeof(Request)*2); enabled_=true;
    }
    bool held() const noexcept { return held_; }
    bool ours(NamedPipe &p) const noexcept { return enabled_ && &p==&pipe_; }
    void tick() {
        if(!enabled_) return;
        if(WaitForSingleObject(owner_.value,0)!=WAIT_TIMEOUT || pipe_.isClosed()) {
            if(active_) release(PeerClosed);
            return;
        }
        if(active_ && GetTickCount64()>=expires_) release(Expired);
    }
    bool deferResize(int c,int r) {
        if(!held_) return false;
        pendingCols_=c; pendingRows_=r; record(DeferredResize,console_.frozen(),c,r); return true;
    }
    void poll() {
        tick();
        while(pipe_.bytesAvailable()>=sizeof(Request)) {
            Request q; fidelity::check(pipe_.read(&q,sizeof(q))==sizeof(q),"research frame");
            DWORD peer=0; const bool physical=pipe_.researchClientPid(peer) && peer==ownerPid_;
            auto out=std::make_unique<Response>(); auto &r=*out;
            r.agent=GetCurrentProcessId(); r.controller=ownerPid_; r.created=created_; r.thread=GetCurrentThreadId(); r.frequency=h3freeze::frequency();
            r.nonce[0]=nonce_[0]; r.nonce[1]=nonce_[1]; r.sequence=q.sequence;
            const auto now=GetTickCount64();
            const auto op=static_cast<Command>(q.command);
            const bool valid=physical && q.signature==magic && q.schema==version && q.bytes==sizeof(q) &&
                q.nonce[0]==nonce_[0] && q.nonce[1]==nonce_[1] && q.controller==ownerPid_ && q.controllerCreated==ownerCreated_ &&
                q.agent==GetCurrentProcessId() && q.agentCreated==created_ && q.sequence!=0 && q.sequence==last_+1 &&
                q.deadline>now && q.deadline-now<=5000 && q.cursor<=agentTrace.total &&
                q.command>=1 && q.command<=8 && (q.command<3 || q.command>6 || (q.leaseMs>=1000 && q.leaseMs<=20000));
            r.status=valid ? 0u : 1u;
            if(valid) {
                ++last_;
                if(op==Command::Quiet || op==Command::Selected || op==Command::Coordinated || op==Command::Sham) {
                    if(active_) r.status=2;
                    else {
                        mode_=op; active_=true; lease_=q.sequence; expires_=now+q.leaseMs;
                        if(op==Command::Sham) record(ShamGrant,console_.frozen());
                        else {
                            fidelity::check(!console_.frozen(),"grant between complete freeze scopes");
                            held_=true;
                            if(op==Command::Selected) console_.setFrozen(true);
                            record(Grant,console_.frozen());
                        }
                    }
                } else if(op==Command::Release) {
                    if(active_) release(h3freeze::Release);
                } else if(op==Command::Resize) {
                    if(q.cols<40 || q.cols>120 || q.rows<15 || q.rows>40) r.status=3;
                    else if(active_) { pendingCols_=q.cols; pendingRows_=q.rows; record(DeferredResize,console_.frozen(),q.cols,q.rows); }
                    else resize_(q.cols,q.rows);
                }
            }
            r.held=held_ ? 1u : 0u; r.cachedFrozen=console_.frozen() ? 1u : 0u; r.mode=static_cast<uint32_t>(mode_);
            r.lease=lease_; r.expires=expires_; r.pendingCols=pendingCols_; r.pendingRows=pendingRows_;
            agentTrace.read(q.cursor,r.trace);
            // Existing asynchronous NamedPipe machinery handles backpressure outside the host hook.
            fidelity::check(pipe_.bytesToSend()<sizeof(Response)*2,"bounded research replies");
            pipe_.write(&r,sizeof(r));
            if(!valid) { if(active_) release(PeerClosed); pipe_.closePipe(); return; }
        }
    }
};
}
