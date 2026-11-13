// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
// Included after the existing worker helpers, only in the coordinated probe.
DWORD neighborSoakMs() {
    wchar_t text[16]={}; const DWORD n=GetEnvironmentVariableW(L"VT7PTY_H3_NEIGHBOR_SOAK_MS",text,16);
    if(!n) return 0;
    wchar_t *end=nullptr; const auto value=wcstoul(text,&end,10);
    check(n<16 && end && !*end && value<=90000,"bounded neighbor development soak");
    return value;
}
struct NeighborCollector {
    Session &session; Channel &channel;
    uint64_t last=GetTickCount64(),maxGap=0,maxDuration=0;
    unsigned int drains=0;
    void service() {
        const auto now=GetTickCount64();
        maxGap=std::max(maxGap,now-last);
        channel.snapshot(); session.gate->drain(); session.pump();
        maxDuration=std::max(maxDuration,GetTickCount64()-now);
        last=GetTickCount64(); ++drains;
    }
    char command() {
        char value=0; DWORD bytes=0;
        Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr)); if(!event.value) throw std::runtime_error("neighbor command event");
        OVERLAPPED ov={}; ov.hEvent=event.value;
        const BOOL read=ReadFile(parentPipe,&value,1,&bytes,&ov);
        bool pending=!read && GetLastError()==ERROR_IO_PENDING;
        check(read || pending,"neighbor command read");
        try {
            const auto deadline=GetTickCount64()+420000;
            while(pending) {
                const DWORD result=WaitForSingleObject(event.value,250);
                if(result==WAIT_OBJECT_0) { pending=false; check(GetOverlappedResult(parentPipe,&ov,&bytes,FALSE),"neighbor command completion"); break; }
                check(result==WAIT_TIMEOUT && GetTickCount64()<deadline,"neighbor command deadline");
                service();
            }
            check(bytes==1,"neighbor command size");
            if(GetTickCount64()-last>=250) service();
            return value;
        } catch(...) {
            if(pending) { CancelIoEx(parentPipe,&ov); DWORD ignored=0; try { complete(parentPipe,ov,ignored,10000); } catch(...) {} }
            throw;
        }
    }
    void write(const std::wstring &out) {
        check(drains>0,"periodic neighbor collection occurred");
        writeFile(out+L"\\neighbor-collector.json","{\"SchemaVersion\":1,\"Periodic\":true,\"IntervalMs\":250,\"Drains\":"+std::to_string(drains)+
            ",\"MaxGapMs\":"+std::to_string(maxGap)+",\"MaxDrainDurationMs\":"+std::to_string(maxDuration)+",\"StorageCapacity\":4096,\"CapacityIncreased\":false}");
    }
};
bool coordinationExpiry(Session &s,Channel &c,const std::wstring &out) {
    startRead(s,"COORDINATION_EXPIRES"); settle(s);
    auto grant=std::make_unique<h3freeze::Response>(s.gate->acquire(h3freeze::Command::Quiet,1000));
    s.gate->exchange(h3freeze::Command::Resize,82,26);
    const auto deadline=GetTickCount64()+1250;
    check(s.until([&] { return GetTickCount64()>=deadline; },2500),"lease expiry wait");
    bool refused=false;
    try { c.exchangeOwned(c.next(Op::Cancel),Result::Accepted,"expired-coordination"); }
    catch(const std::runtime_error &error) { refused=std::string(error.what()).starts_with("CoordinationUnavailable: lease ended before dispatch"); }
    check(refused && c.blocked(),"expired coordination refuses before dispatch");
    bool retry=false; try { c.exchangeOwned(c.next(Op::Cancel),Result::Accepted,"must-not-retry-expired-coordination"); } catch(...) { retry=true; }
    const auto before=s.observe();
    check(!before.readReturned && !before.controlCount && s.gate->lease()==0,"expiry preserved read without an event");
    check(s.until([&] { const auto p=s.observe(); return p.columns==82 && p.rows==26; }),"expiry applies deferred resize");
    s.gate->exchange(h3freeze::Command::Resize,80,25);
    check(s.until([&] { const auto p=s.observe(); return p.columns==80 && p.rows==25; }),"expiry restores dimensions");
    writeFile(out+L"\\expiry-before-recovery.json",packetJson(s.observe()));
    finishRead(s,L"COORDINATION_EXPIRES",out,L"expiry-recovery");
    s.gate->drain(); c.detach();
    writeFile(out+L"\\coordination-expiry.json","{\"SchemaVersion\":1,\"Lease\":"+h3freeze::number(grant->lease)+
        ",\"RequestedLeaseMs\":1000,\"DispatchRefusedBeforeWrite\":true,\"DispatchDisabled\":true,\"RetryRefused\":"+std::string(retry ? "true" : "false")+
        ",\"ControlCountBeforeRecovery\":0,\"PendingReadPreserved\":true,\"DeferredResizeApplied\":true,\"NextInputPreserved\":true,\"FallbackControlSent\":false}");
    s.quit(); return retry;
}
