// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "WinTest.h"
#include "HostFreezeJson.h"
namespace h3freeze {
inline std::wstring launchNonce() {
    HCRYPTPROV provider=0; fidelity::check(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT),"launch randomness");
    uint64_t data[2]={}; const BOOL okay=CryptGenRandom(provider,sizeof(data),reinterpret_cast<BYTE *>(data)); CryptReleaseContext(provider,0);
    fidelity::check(okay!=FALSE,"launch nonce"); wchar_t text[33]={}; swprintf_s(text,L"%016llx%016llx",data[0],data[1]); return text;
}
class LaunchEnvironment {
    std::wstring previous;
    bool existed=false;
public:
    std::wstring nonce=launchNonce();
    LaunchEnvironment() {
        DWORD n=GetEnvironmentVariableW(L"VT7PTY_H3_EXPERIMENT_NONCE",nullptr,0);
        if(n) { std::vector<wchar_t> text(n); GetEnvironmentVariableW(L"VT7PTY_H3_EXPERIMENT_NONCE",text.data(),n); previous=text.data(); existed=true; }
        fidelity::check(SetEnvironmentVariableW(L"VT7PTY_H3_EXPERIMENT_NONCE",nonce.c_str())!=FALSE,"research launch environment");
    }
    ~LaunchEnvironment() { SetEnvironmentVariableW(L"VT7PTY_H3_EXPERIMENT_NONCE",existed ? previous.c_str() : nullptr); }
};
class Client {
    HANDLE agent_;
    fidelity::Handle pipe_;
    std::wstring out_;
    uint64_t nonce_[2]={},sequence_=0,cursor_=0,total_=0,created_=0,ownerCreated_=0;
    unsigned int record_=0;
    bool held_=false,active_=false,finished_=false;
    static uint64_t creation(HANDLE h) {
        FILETIME a={},b={},c={},d={}; fidelity::check(GetProcessTimes(h,&a,&b,&c,&d),"gate process time"); return (uint64_t{a.dwHighDateTime}<<32)|a.dwLowDateTime;
    }
public:
    Client(HANDLE agent,const std::wstring &nonce,const std::wstring &out):agent_(agent),out_(out) {
        fidelity::check(!out.empty(),"research output directory");
        nonce_[0]=_wcstoui64(nonce.substr(0,16).c_str(),nullptr,16); nonce_[1]=_wcstoui64(nonce.substr(16).c_str(),nullptr,16);
        created_=creation(agent); ownerCreated_=creation(GetCurrentProcess());
        const auto name=L"\\\\.\\pipe\\VT7Pty-H3-freeze-"+std::to_wstring(GetProcessId(agent))+L"-"+nonce;
        const auto deadline=GetTickCount64()+5000;
        while(GetTickCount64()<deadline) {
            pipe_.reset(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr));
            if(pipe_.value!=INVALID_HANDLE_VALUE) break;
            const DWORD error=GetLastError(); fidelity::check(error==ERROR_FILE_NOT_FOUND || error==ERROR_PIPE_BUSY,"owned research endpoint"); Sleep(2);
        }
        fidelity::check(pipe_.value!=INVALID_HANDLE_VALUE,"research endpoint deadline");
        ULONG peer=0; fidelity::check(GetNamedPipeServerProcessId(pipe_.value,&peer) && peer==GetProcessId(agent),"physical research agent peer");
        exchange(Command::Bind); drain();
    }
    ~Client() { finish(); }
    Response exchange(Command command,int cols=0,int rows=0) {
        fidelity::check(!finished_ && WaitForSingleObject(agent_,0)==WAIT_TIMEOUT,"live research agent");
        Request q; q.command=static_cast<uint32_t>(command); q.sequence=++sequence_; q.cursor=cursor_; q.deadline=GetTickCount64()+4000;
        q.nonce[0]=nonce_[0]; q.nonce[1]=nonce_[1]; q.agent=GetProcessId(agent_); q.controller=GetCurrentProcessId();
        q.agentCreated=created_; q.controllerCreated=ownerCreated_; q.leaseMs=20000; q.cols=cols; q.rows=rows;
        const auto before=clock(); fidelity::transfer(pipe_.value,&q,sizeof(q),true);
        Response r; fidelity::transfer(pipe_.value,&r,sizeof(r),false);
        const auto after=clock();
        fidelity::check(r.signature==magic && r.schema==version && r.bytes==sizeof(r) && r.sequence==q.sequence && r.agent==q.agent &&
            r.controller==q.controller && r.created==created_ && r.nonce[0]==nonce_[0] && r.nonce[1]==nonce_[1] && r.frequency>0 &&
            r.trace.count<=chunkCapacity && r.trace.next>=cursor_ && r.trace.next<=r.trace.total,"research response identity/framing");
        cursor_=r.trace.next; total_=r.trace.total; held_=r.held!=0; active_=r.lease!=0;
        fidelity::writeFile(out_+L"\\agent-"+std::to_wstring(++record_)+L"-request.json",
            "{\"Command\":"+std::to_string(q.command)+",\"RequestSequence\":"+number(q.sequence)+",\"ControllerCreated\":"+number(ownerCreated_)+
            ",\"AgentCreated\":"+number(created_)+",\"NonceLow\":"+number(nonce_[0])+",\"NonceHigh\":"+number(nonce_[1])+
            ",\"BeforeQpc\":"+number(before)+",\"AfterQpc\":"+number(after)+",\"RequestedCols\":"+std::to_string(cols)+
            ",\"RequestedRows\":"+std::to_string(rows)+",\"Response\":"+agentJson(r)+"}");
        fidelity::check(r.status==0,"research command accepted"); return r;
    }
    void drain() { unsigned int blocks=0; do { exchange(Command::Snapshot); fidelity::check(++blocks<=512,"bounded agent trace snapshot"); } while(cursor_<total_); }
    Response acquire(Command mode) { drain(); const auto r=exchange(mode); drain(); return r; }
    void release() { exchange(Command::Release); drain(); }
    bool finish() noexcept {
        if(finished_) return true;
        bool okay=true; std::string error;
        const bool alive=WaitForSingleObject(agent_,0)==WAIT_TIMEOUT;
        try { if(alive) { if(active_) release(); drain(); } } catch(const std::exception &e) { okay=false; error=e.what(); }
        try { fidelity::writeFile(out_+L"\\agent-finish.json","{\"AgentAliveAtCollection\":"+std::string(alive ? "true" : "false")+
            ",\"LastCursor\":"+number(cursor_)+",\"LastTotal\":"+number(total_)+",\"Held\":"+(held_ ? "true" : "false")+
            ",\"CollectionHealthy\":"+(okay ? "true" : "false")+",\"Error\":"+fidelity::json(error)+"}"); } catch(...) { okay=false; }
        finished_=true; pipe_.reset(); return okay;
    }
};
}
