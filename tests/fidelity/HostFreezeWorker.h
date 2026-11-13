// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
// Included in the research worker after the existing case/read helpers.
unsigned int freezeRotation() {
    wchar_t text[16]={}; const DWORD n=GetEnvironmentVariableW(L"VT7PTY_H3_ORDER",text,16);
    if(!n) return 0;
    check(n==1 && text[0]>=L'0' && text[0]<=L'2',"research rotation"); return static_cast<unsigned int>(text[0]-L'0');
}
std::string freezeModeName(h3freeze::Command mode) {
    switch(mode) {
    case h3freeze::Command::Quiet:return "quiet";
    case h3freeze::Command::Selected:return "selected";
    case h3freeze::Command::Coordinated:return "coordinated";
    case h3freeze::Command::Sham:return "delay-control";
    default:throw std::runtime_error("unknown research condition");
    }
}
bool freezePhase(Session &s,Channel &c,const std::wstring &out,const std::wstring &label,h3freeze::Command mode,bool pending,uint32_t expectedEvents) {
    struct PhaseStorage { h3freeze::Response grant,deferred,lease; IdentityPacket before,deadline,after; };
    auto storage=std::make_unique<PhaseStorage>();
    const bool loaded=mode==h3freeze::Command::Coordinated || mode==h3freeze::Command::Sham;
    if(loaded) {
        s.observe('B');
        check(s.until([&] { return s.observe().activityWrites>=3; }),"activity started before grant");
        s.gate->exchange(h3freeze::Command::Resize,82,26);
        s.gate->exchange(h3freeze::Command::Resize,80,25);
    }
    c.snapshot(); s.gate->drain();
    auto &grant=storage->grant; grant=s.gate->acquire(mode);
    const auto acquired=h3trace::qpc();
    // Same declared 50 ms observation delay in each arm. This is a control variable, never a cancellation fix.
    const auto delayEnd=GetTickCount64()+50;
    s.until([&] { return GetTickCount64()>=delayEnd; },1000);
    if(loaded) {
        auto &deferred=storage->deferred; deferred=s.gate->exchange(h3freeze::Command::Resize,88,27);
        check(deferred.pendingCols==88 && deferred.pendingRows==27,"resize recorded as deferred, not applied");
    }
    auto &before=storage->before; before=s.observe();
    writeFile(out+L"\\"+label+L"-before.json",packetJson(before));
    const bool publicQueriesOkay=before.trace.selectionError==0 && before.trace.inputError==0 && (before.inputMode&ENABLE_PROCESSED_INPUT);
    const bool selectionMatches=publicQueriesOkay && (mode==h3freeze::Command::Selected ?
        (grant.held==1 && grant.cachedFrozen==1 && (before.trace.selectionFlags&CONSOLE_SELECTION_IN_PROGRESS)) :
        mode==h3freeze::Command::Sham ? grant.held==0 :
        (grant.held==1 && grant.cachedFrozen==0 && before.trace.selectionFlags==0));
    const auto sendAt=h3trace::qpc();
    const auto responseStorage=c.exchangeOwned(c.next(Op::Cancel),Result::Accepted,"freeze-condition");
    const auto &response=*responseStorage;
    const bool keyboard=keyboardOkay(response);
    auto &deadline=storage->deadline;
    const auto predicate=[&](const IdentityPacket &p) {
        if(pending) return canceledRead(p,expectedEvents);
        return p.readReturned && p.readSucceeded && !p.readError && std::wstring(p.text,p.textCount)==L"COMPLETED\r\n" &&
            p.controlCount==expectedEvents && p.controlType==CTRL_C_EVENT;
    };
    const bool atDeadline=s.until([&] { deadline=s.observe(); return predicate(deadline); },2000);
    writeFile(out+L"\\"+label+L"-deadline.json",packetJson(deadline));
    auto &leaseAtDeadline=storage->lease; leaseAtDeadline=s.gate->exchange(h3freeze::Command::Snapshot);
    const bool leaseIntact=grant.lease!=0 && leaseAtDeadline.lease==grant.lease;
    c.snapshot(); s.gate->drain();
    const auto releaseBegin=h3trace::qpc();
    s.gate->release();
    const auto releaseEnd=h3trace::qpc();
    auto &after=storage->after; after=s.observe();
    writeFile(out+L"\\"+label+L"-released.json",packetJson(after)); c.snapshot();
    if(!atDeadline) after=diagnosticFollowUp(s,out,label,expectedEvents,pending);
    // The first deadline's result is retained even if the later predicate becomes true.
    if(pending && !after.readReturned) {
        s.input("\r"); check(s.until([&] { after=s.observe(); return after.readReturned!=0; }),"focused read recovery");
        writeFile(out+L"\\"+label+L"-recovery.json",packetJson(after));
    }
    bool activity=true,resize=true;
    if(loaded) {
        resize=s.until([&] { after=s.observe(); return after.columns==88 && after.rows==27; });
        const auto writes=after.activityWrites; const auto bytes=s.raw.size();
        activity=s.until([&] { const auto p=s.observe(); return !p.activityError && p.activityWrites>writes && s.raw.size()>bytes; });
        s.observe('E');
        s.gate->exchange(h3freeze::Command::Resize,80,25);
        resize=s.until([&] { const auto p=s.observe(); return p.columns==80 && p.rows==25; }) && resize;
    }
    const bool next=pending ? nextRead(s,out,label,after.controlCount) : true;
    c.snapshot(); s.gate->drain();
    const bool passed=atDeadline && keyboard && selectionMatches && leaseIntact && next && activity && resize;
    writeFile(out+L"\\"+label+L"-condition.json","{\"SchemaVersion\":1,\"Condition\":"+json(freezeModeName(mode))+
        ",\"PendingRead\":"+(pending ? "true" : "false")+",\"CharacterizationOnly\":true,\"EvidenceCollected\":true"+
        ",\"GrantQpc\":"+h3trace::u64(acquired)+",\"SendQpc\":"+h3trace::u64(sendAt)+",\"ReleaseBeginQpc\":"+h3trace::u64(releaseBegin)+
        ",\"ReleaseEndQpc\":"+h3trace::u64(releaseEnd)+",\"DelayControlMs\":50,\"RequestedLeaseMs\":20000"+
        ",\"GrantedLease\":"+h3trace::u64(grant.lease)+",\"GrantedMode\":"+std::to_string(grant.mode)+",\"GrantedHeld\":"+std::to_string(grant.held)+
        ",\"GrantedCachedFrozen\":"+std::to_string(grant.cachedFrozen)+",\"SelectionBefore\":"+std::to_string(before.trace.selectionFlags)+
        ",\"LeaseActiveAtDeadline\":"+(leaseIntact ? "true" : "false")+",\"ExpectedEvents\":"+std::to_string(expectedEvents)+",\"OriginalDeadlineMet\":"+(atDeadline ? "true" : "false")+
        ",\"KeyboardRestored\":"+(keyboard ? "true" : "false")+",\"ConditionObserved\":"+(selectionMatches ? "true" : "false")+
        ",\"NextReadPreserved\":"+(next ? "true" : "false")+",\"ActivityResumed\":"+(activity ? "true" : "false")+
        ",\"ResizeResumed\":"+(resize ? "true" : "false")+",\"CancellationPassed\":"+(passed ? "true" : "false")+
        ",\"MilestoneAccepted\":false}");
    return passed;
}
bool freezeCase(Session &s,Channel &c,const std::wstring &out,h3freeze::Command mode,bool pending) {
    startRead(s,pending ? "H3_FOCUSED_PENDING" : "COMPLETED");
    if(pending) { settle(s); const auto p=s.observe(); check(!p.readReturned && p.trace.readEntered,"focused pending entry and echo barrier"); }
    else finishRead(s,L"COMPLETED",out,L"completed-before");
    const bool result=freezePhase(s,c,out,L"focus",mode,pending,1);
    c.close(); s.quit(); return result;
}
bool freezeBracket(Session &s,Channel &c,const std::wstring &out) {
    startRead(s,"COMPLETED"); finishRead(s,L"COMPLETED",out,L"completed-before");
    const bool first=freezePhase(s,c,out,L"positive-before",h3freeze::Command::Quiet,false,1);
    const auto baseline=s.observe().controlCount;
    const bool selected=freezePhase(s,c,out,L"selected",h3freeze::Command::Selected,false,baseline+1);
    const auto resumed=s.observe().controlCount;
    const bool last=freezePhase(s,c,out,L"positive-after",h3freeze::Command::Quiet,false,resumed+1);
    writeFile(out+L"\\bracket.json","{\"PositiveBefore\":"+std::string(first ? "true" : "false")+
        ",\"SelectedCancellation\":"+(selected ? "true" : "false")+",\"PositiveAfter\":"+(last ? "true" : "false")+
        ",\"CharacterizationOnly\":true,\"SameFixture\":true}");
    c.close(); s.quit(); return first && selected && last;
}
void freezeOracle() {
    auto ring=std::make_unique<h3freeze::Ring<h3freeze::WindowEvent>>();
    for(uint32_t i=0;i<h3freeze::ringCapacity+3;++i) { h3freeze::WindowEvent e; e.message=i; ring->add(e); }
    h3freeze::Chunk<h3freeze::WindowEvent> chunk; ring->read(0,chunk);
    check(chunk.lost==3 && chunk.count==h3freeze::chunkCapacity && chunk.events[0].sequence==4,"ring overwrite is explicit");
    const auto cursor=chunk.next; ring->read(cursor,chunk); check(chunk.lost==0 && chunk.events[0].sequence==cursor+1,"paged trace has no duplication");
    ring->read(ring->total,chunk); check(!chunk.count && !chunk.lost && chunk.next==ring->total,"empty trace cursor");
    puts("Host freeze oracle: 3 checks passed");
}
