// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
// Research only: no hooks, host messages, remote writes or input cancellation.
#include "HostResearch.h"
using namespace fidelity;
using namespace hostresearch;
namespace {
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
