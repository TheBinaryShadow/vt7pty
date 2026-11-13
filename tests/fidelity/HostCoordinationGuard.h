// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "HostFreezeClient.h"
#include "HostCancellationResearch.h"
#include "HostCancellationProtocol.h"
namespace h3coord {
inline HANDLE completionEvent(const std::wstring &name) {
    fidelity::Handle token; fidelity::check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value),"completion token");
    DWORD bytes=0; GetTokenInformation(token.value,TokenUser,nullptr,0,&bytes);
    std::vector<BYTE> user(bytes); fidelity::check(GetTokenInformation(token.value,TokenUser,user.data(),bytes,&bytes),"completion user");
    fidelity::LocalAllocation<wchar_t> sid;
    fidelity::check(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid,&sid.value),"completion SID");
    const auto acl=L"D:P(A;;GA;;;SY)(A;;GA;;;"+std::wstring(sid.value)+L")";
    fidelity::LocalAllocation<void> descriptor;
    fidelity::check(ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor.value,nullptr),"completion ACL");
    SECURITY_ATTRIBUTES attributes={sizeof(attributes),descriptor.value,FALSE};
    SetLastError(ERROR_SUCCESS);
    fidelity::Handle event(CreateEventW(&attributes,TRUE,FALSE,name.c_str()));
    fidelity::check(event.value && GetLastError()!=ERROR_ALREADY_EXISTS,"new private completion event");
    const HANDLE result=event.value; event.value=INVALID_HANDLE_VALUE; return result;
}
// Controller-side only. No agent IPC or waits run inside the host callback.
class Guard {
    h3freeze::Client &client_;
    std::wstring path_,hostRecord_;
    std::string case_,outcome_="Abandoned",error_;
    uint64_t sequence_=0,lease_=0,expires_=0,begin_=h3freeze::clock(),ready_=0,write_=0,signal_=0,completion_=0,ack_=0,end_=0;
    uint32_t operation_=0,mode_=0,held_=0,cached_=0;
    bool owned_=false,protected_=false,verified_=false,started_=false,finalAck_=false,released_=false,exitResolved_=false,done_=false;
    void save() {
        end_=h3freeze::clock();
        const auto boolean=[](bool value) { return value ? "true" : "false"; };
        fidelity::writeFile(path_,"{\"SchemaVersion\":1,\"Kind\":\"HostCancellationCoordination\",\"Case\":"+fidelity::json(case_)+
            ",\"HostRecord\":"+fidelity::json(cancelresearch::utf8(hostRecord_))+",\"RequestSequence\":"+h3freeze::number(sequence_)+
            ",\"Operation\":"+std::to_string(operation_)+",\"Lease\":"+h3freeze::number(lease_)+",\"Expires\":"+h3freeze::number(expires_)+
            ",\"Mode\":"+std::to_string(mode_)+",\"Held\":"+std::to_string(held_)+",\"CachedFrozen\":"+std::to_string(cached_)+
            ",\"BeginQpc\":"+h3freeze::number(begin_)+",\"ReadyQpc\":"+h3freeze::number(ready_)+",\"WriteBeginQpc\":"+h3freeze::number(write_)+
            ",\"SignalReturnQpc\":"+h3freeze::number(signal_)+",\"CompletionQpc\":"+h3freeze::number(completion_)+",\"AckQpc\":"+h3freeze::number(ack_)+",\"EndQpc\":"+h3freeze::number(end_)+
            ",\"OwnedLease\":"+boolean(owned_)+",\"Protected\":"+boolean(protected_)+",\"VerifiedBeforeWrite\":"+boolean(verified_)+
            ",\"HostWriteStarted\":"+boolean(started_)+",\"FinalAckObserved\":"+boolean(finalAck_)+",\"Released\":"+boolean(released_)+
            ",\"AgentExitResolvedLease\":"+boolean(exitResolved_)+",\"Outcome\":"+fidelity::json(outcome_)+",\"Error\":"+fidelity::json(error_)+
            ",\"MilestoneAccepted\":false,\"RetrySent\":false}");
        done_=true;
    }
public:
    Guard(h3freeze::Client &client,const std::wstring &output,const std::wstring &hostRecord,unsigned int index,
        const hostcancel::Request &request,const char *caseName):client_(client),path_(output+L"\\coordination-"+std::to_wstring(index)+L".json"),
        hostRecord_(hostRecord),case_(caseName),sequence_(request.sequence),operation_(static_cast<uint32_t>(request.operation)) {}
    ~Guard() { if(!done_) fail("coordination scope abandoned"); }
    void begin() {
        const auto prior=client_.lease();
        lease_=prior;
        auto proof=std::make_unique<h3freeze::Response>();
        if(client_.active()) {
            *proof=client_.exchange(h3freeze::Command::Snapshot);
            fidelity::check(prior!=0 && proof->lease==prior,"CoordinationUnavailable: lease ended before dispatch");
        } else {
            owned_=true;
            *proof=client_.acquire(h3freeze::Command::Quiet);
        }
        lease_=proof->lease; expires_=proof->expires; mode_=proof->mode; held_=proof->held; cached_=proof->cachedFrozen;
        fidelity::check(lease_!=0,"CoordinationUnavailable: missing lease");
        *proof=client_.exchange(h3freeze::Command::Snapshot);
        fidelity::check(proof->lease==lease_ && proof->mode==mode_,"CoordinationUnavailable: lease ended before dispatch");
        protected_=proof->held==1 && proof->cachedFrozen==0 &&
            (mode_==static_cast<uint32_t>(h3freeze::Command::Quiet) || mode_==static_cast<uint32_t>(h3freeze::Command::Coordinated));
        fidelity::check(!owned_ || protected_,"CoordinationUnavailable: unfrozen exclusion not granted");
        verified_=true; ready_=h3freeze::clock();
    }
    bool protects() const noexcept { return protected_; }
    void beforeWrite() {
        fidelity::check(verified_ && GetTickCount64()<expires_,"CoordinationUnavailable: lease ended before dispatch");
        write_=h3freeze::clock(); started_=true;
    }
    void signalReturned() {
        fidelity::check(started_ && GetTickCount64()<expires_,"CoordinationUnavailable: lease ended during dispatch");
        signal_=h3freeze::clock();
    }
    void observeCompletion(HANDLE event) {
        fidelity::check(WaitForSingleObject(event,2000)==WAIT_OBJECT_0,"OutcomeUnknown: callback completion deadline");
        fidelity::check(GetTickCount64()<expires_,"CoordinationUnavailable: lease ended during callback");
        completion_=h3freeze::clock();
    }
    void complete(hostcancel::Result expected) {
        fidelity::check(completion_!=0,"coordination completion requires callback boundary");
        ack_=h3freeze::clock(); finalAck_=expected==hostcancel::Result::Accepted;
        outcome_=finalAck_ ? "Acknowledged" : "ReceiptOnly";
        try {
            auto proof=std::make_unique<h3freeze::Response>(client_.exchange(h3freeze::Command::Snapshot));
            fidelity::check(proof->lease==lease_ && proof->mode==mode_,"CoordinationUnavailable: lease ended before collection");
            if(owned_) { client_.release(); released_=true; }
        } catch(...) {
            if(client_.alive() || GetTickCount64()>=expires_) throw;
            client_.resolveExitedLease(); exitResolved_=true;
        }
        save();
    }
    void fail(const char *reason) noexcept {
        if(done_) return;
        error_=reason;
        outcome_=started_ ? "OutcomeUnknown" : "RefusedBeforeWrite";
        // The completion marker covers the keyboard scope, never the final protocol result.
        // Without that boundary, leave the bounded lease for failure cleanup or expiry.
        if(owned_ && (!started_ || completion_)) {
            try {
                if(client_.alive() && client_.active()) { client_.release(); released_=true; }
                else if(!client_.alive()) { client_.resolveExitedLease(); exitResolved_=true; }
            } catch(const std::exception &e) { error_+="; release: "; error_+=e.what(); }
        }
        try { save(); } catch(...) {}
    }
};
}
