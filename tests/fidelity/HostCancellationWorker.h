// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "HostCancellationChannel.h"
#include <fstream>
#include <filesystem>
namespace {

struct Registration {
    uint32_t kind=1,sequence=0;
    Identity identity;
    uint32_t shell=0,ready=0;
    uint64_t shellCreated=0;
};
HANDLE parentPipe=INVALID_HANDLE_VALUE;
uint32_t registrationSequence=0;
void registerSession(Session &s,Channel &c) {
    Registration r; r.identity=c.binding.identity; r.sequence=++registrationSequence;
    if(s.descendant.value && s.descendant.value!=INVALID_HANDLE_VALUE) {
        r.shell=GetProcessId(s.descendant.value); r.shellCreated=creation(s.descendant.value);
    }
    transfer(parentPipe,&r,sizeof(r),true);
    char ack=0; transfer(parentPipe,&ack,1,false); check(ack=='A',"parent lifetime registration");
}
bool keyboardOkay(const Response &r,bool failure=false) {
    const auto &e=r.keyboard;
    return e.captured && e.applied && e.restored && e.equal && !e.error &&
        memcmp(e.before,e.after,256)==0 && (e.during[VK_CONTROL]&0x80) && (e.during[VK_LCONTROL]&0x80) &&
        !(e.during[VK_SHIFT]&0x80) && !(e.during[VK_MENU]&0x80) &&
        e.sent==(failure ? 0u : 1u) && e.injectedFailure==(failure ? 1u : 0u);
}
bool canceledRead(const IdentityPacket &p,uint32_t events) {
    // ReadConsoleW can succeed while setting ERROR_OPERATION_ABORTED.
    // Preserve BOOL, last-error, count and handler observation independently.
    return p.readReturned && p.textCount==0 && p.readError==ERROR_OPERATION_ABORTED &&
        p.controlCount==events && p.controlType==CTRL_C_EVENT;
}
void cancellationOracle() {
    protocolOracle(); unsigned int checks=13;
    auto test=[&](bool passed,const char *name) { check(passed,name); ++checks; };
    Request r; r.nonce[0]=1; r.sequence=2; r.deadline=2000;
    for(Op op:{Op::Cancel,Op::RestoreControl,Op::CancelSuppressAck,Op::CancelClose,Op::DeniedControl}) {
        r.operation=op; test(validate(r,r,1,1000,false)==Result::Accepted,"cancellation operation oracle");
    }
    IdentityPacket p; p.readReturned=1; p.readSucceeded=1; p.readError=ERROR_OPERATION_ABORTED; p.controlCount=1; p.controlType=CTRL_C_EVENT;
    test(canceledRead(p,1),"successful aborted read");
    p.readSucceeded=0; test(canceledRead(p,1),"failed aborted read");
    auto bad=p; bad.textCount=1; test(!canceledRead(bad,1),"reject retained text");
    bad=p; bad.controlCount=2; test(!canceledRead(bad,1),"reject duplicate event");
    bad=p; bad.readError=0; test(!canceledRead(bad,1),"reject normal empty read");
    bad=p; bad.readReturned=0; test(!canceledRead(bad,1),"reject pending read");
    bad=p; bad.controlType=CTRL_BREAK_EVENT; test(!canceledRead(bad,1),"reject Ctrl Break");
    Response response; auto &e=response.keyboard;
    e.captured=e.applied=e.restored=e.equal=e.sent=1; e.during[VK_CONTROL]=e.during[VK_LCONTROL]=0x80;
    test(keyboardOkay(response),"restored keyboard oracle");
    auto changed=response; changed.keyboard.after[10]=1; test(!keyboardOkay(changed),"reject mismatching snapshot");
    changed=response; changed.keyboard.restored=0; test(!keyboardOkay(changed),"reject failed restore");
    changed=response; changed.keyboard.error=5; test(!keyboardOkay(changed),"reject operation error");
    changed=response; changed.keyboard.during[VK_SHIFT]=0x80; test(!keyboardOkay(changed),"reject extra modifier");
    changed=response; changed.keyboard.sent=0; test(!keyboardOkay(changed),"reject missing key delivery");
    changed.keyboard.injectedFailure=1; test(keyboardOkay(changed,true),"injected failure restores without input");
    changed.keyboard.sent=1; test(!keyboardOkay(changed,true),"reject input during failure control");
    printf("Host cancellation oracle: %u checks passed\n",checks);
}

void settle(Session &s,DWORD ms=200) {
    const auto end=GetTickCount64()+ms;
    s.until([&] { return GetTickCount64()>=end; },ms+1000);
}
bool captureCancellation(Session &s,const std::wstring &out,const std::wstring &name,uint32_t events) {
    IdentityPacket p;
    s.until([&] { p=s.observe(); return p.readReturned && p.controlCount>=events; },2000);
    settle(s); p=s.observe();
    writeFile(out+L"\\"+name+L"-read.json",packetJson(p));
    const bool passed=canceledRead(p,events);
    if(!p.readReturned) {
        // This is recovery after a recorded failure, never a cancellation pass.
        s.input("\r");
        check(s.until([&] { p=s.observe(); return p.readReturned!=0; }),"boundary Enter recovery");
        writeFile(out+L"\\"+name+L"-recovery.json",packetJson(p));
    }
    return passed;
}
bool nextRead(Session &s,const std::wstring &out,const std::wstring &name,uint32_t events) {
    const auto ready=s.observe('R'); check(!ready.readReturned,"next read readiness");
    s.input("NEXT_PROBE\r"); IdentityPacket p;
    const bool returned=s.until([&] { p=s.observe(); return p.readReturned!=0; },2000);
    writeFile(out+L"\\"+name+L"-next.json",packetJson(p));
    const bool okay=returned && p.readSucceeded && !p.readError && std::wstring(p.text,p.textCount)==L"NEXT_PROBE\r\n" && p.controlCount==events;
    if(!returned) { s.input("\r"); check(s.until([&] { p=s.observe(); return p.readReturned!=0; }),"next-read recovery"); writeFile(out+L"\\"+name+L"-next-recovery.json",packetJson(p)); }
    return okay;
}
std::wstring psLiteral(const std::wstring &text) {
    std::wstring result=L"'"; for(wchar_t c:text) { result+=c; if(c==L'\'') result+=c; } return result+L"'";
}
std::u32string trim(std::u32string text) { while(!text.empty() && text.back()==U' ') text.pop_back(); return text; }
std::string fileText(const std::wstring &path) {
    std::ifstream file{std::filesystem::path(path)}; std::string text; std::getline(file,text);
    while(!text.empty() && (text.back()=='\r' || text.back()=='\n' || text.back()==' ')) text.pop_back();
    return text;
}
struct CaseResult { std::string name,error; bool passed=false,cleaned=false,hostExited=false,unhooked=false; };
CaseResult caseRun(const std::wstring &output,const std::string &name,unsigned int index,
    const std::function<bool(Session &,Channel &,const std::wstring &)> &body) {
    const std::wstring path=output+L"\\"+std::wstring(name.begin(),name.end());
    check(CreateDirectoryW(path.c_str(),nullptr),"new case directory");
    CaseResult result; result.name=name;
    std::unique_ptr<Session> session; std::unique_ptr<Channel> channel;
    try {
        session=std::make_unique<Session>(index);
        channel=std::make_unique<Channel>(*session,path,L"host");
        registerSession(*session,*channel);
        result.passed=body(*session,*channel,path);
        if(!result.passed) result.error="Case did not meet acceptance; inspect its raw records.";
    } catch(const std::exception &e) { result.error=e.what(); }
    if(channel) { channel->detach(); result.unhooked=channel->unhooked; }
    if(session) {
        result.cleaned=session->cleanup();
        result.hostExited=session->host.value && session->host.value!=INVALID_HANDLE_VALUE && WaitForSingleObject(session->host.value,5000)==WAIT_OBJECT_0;
        writeFile(path+L"\\stream.bin",session->raw);
    }
    result.passed=result.passed && result.cleaned && result.hostExited && result.unhooked && result.error.empty();
    return result;
}
void startOwnedShell(Session &s,Channel &c,bool cmd) {
    auto p=s.observe(cmd ? 'M' : 'W');
    Handle shellCandidate(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,p.shellId));
    check(shellCandidate.value && parentProcessId(p.shellId)==GetProcessId(s.child.value),"owned shell descendant");
    wchar_t system[MAX_PATH]={}; check(GetSystemDirectoryW(system,MAX_PATH)>0,"system directory");
    const auto expected=std::wstring(system)+(cmd ? L"\\cmd.exe" : L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    check(_wcsicmp(imagePath(shellCandidate.value).c_str(),expected.c_str())==0,"exact system shell");
    s.descendant.reset(shellCandidate.value); shellCandidate.value=INVALID_HANDLE_VALUE;
    registerSession(s,c);
}
bool shellCase(Session &s,Channel &c,const std::wstring &out,bool cmd,bool partial) {
    startOwnedShell(s,c,cmd); IdentityPacket p;
    const auto prompt=cmd ? U"H3_CMD>" : U"H3_PS>";
    auto atPrompt=[&] { return trim(s.state.row(s.state.y()))==prompt; };
    check(s.until(atPrompt,20000),"shell initial prompt");
    std::string metadata=cmd ? "System32 cmd.exe" : "";
    if(!cmd) {
        const auto identity=out+L"\\shell-identity.txt";
        s.input(utf8(L"[IO.File]::WriteAllText("+psLiteral(identity)+L",$PSVersionTable.PSVersion.ToString()+'|'+@((Get-Module PSReadLine)).Count.ToString()+'|'+$Host.Name)\r"));
        check(s.until([&] { return GetFileAttributesW(identity.c_str())!=INVALID_FILE_ATTRIBUTES && atPrompt(); },10000),"native shell identity");
        metadata=fileText(identity); check(metadata.ends_with("|0|ConsoleHost"),"native ConsoleHost editor");
    }
    const auto canceled=out+L"\\canceled-command.txt",continued=out+L"\\continued-command.txt";
    check(GetFileAttributesW(canceled.c_str())==INVALID_FILE_ATTRIBUTES && GetFileAttributesW(continued.c_str())==INVALID_FILE_ATTRIBUTES,"new shell markers");
    if(partial) {
        s.input(utf8(cmd ? L"echo BAD>"+quote(canceled) : L"[IO.File]::WriteAllText("+psLiteral(canceled)+L",'BAD')"));
        check(s.until([&] { return !atPrompt(); }),"partial shell input");
        settle(s);
    }
    const auto before=s.lineFeeds;
    const auto r=c.exchange(c.next(Op::Cancel),Result::Accepted,"shell-cancel");
    const bool restored=keyboardOkay(r);
    const bool fresh=s.until([&] { return s.lineFeeds>before && atPrompt(); },2000);
    settle(s); p=s.observe();
    writeFile(out+L"\\event-observation.json",packetJson(p));
    // Boundary Enter exposes retained text even when a cosmetic prompt appeared.
    s.input("\r"); settle(s); check(s.until(atPrompt),"boundary prompt");
    s.input(utf8((cmd ? L"echo CONTINUE>"+quote(continued) : L"[IO.File]::WriteAllText("+psLiteral(continued)+L",'CONTINUE')")+L"\r"));
    const bool usable=s.until([&] { return GetFileAttributesW(continued.c_str())!=INVALID_FILE_ATTRIBUTES && atPrompt(); });
    const bool discarded=GetFileAttributesW(canceled.c_str())==INVALID_FILE_ATTRIBUTES;
    const bool content=usable && fileText(continued)=="CONTINUE";
    const bool alive=WaitForSingleObject(s.descendant.value,0)==WAIT_TIMEOUT;
    writeFile(out+L"\\shell.json","{\"ShellIdentity\":"+json(metadata)+",\"FreshPrompt\":"+(fresh ? "true" : "false")+
        ",\"CanceledCommandAbsent\":"+(discarded ? "true" : "false")+",\"NextCommand\":"+(content ? "true" : "false")+
        ",\"ShellAlive\":"+(alive ? "true" : "false")+",\"KeyboardRestored\":"+(restored ? "true" : "false")+
        ",\"FixtureControlCount\":"+std::to_string(p.controlCount)+"}");
    c.close(); s.input("exit 0\r");
    check(s.until([&] { return WaitForSingleObject(s.descendant.value,0)==WAIT_OBJECT_0; }),"shell exit");
    DWORD exit=0; check(GetExitCodeProcess(s.descendant.value,&exit) && exit==0,"shell exit status");
    s.quit();
    return restored && fresh && discarded && content && alive && p.controlCount==1 && p.controlType==CTRL_C_EVENT;
}
int suite(const std::wstring &output) {
    std::vector<CaseResult> results;
    unsigned int index=0;
    auto add=[&](const std::string &name,const auto &body) { results.push_back(caseRun(output,name,++index,body)); };
    add("cooked-repeated",[](Session &s,Channel &c,const std::wstring &out) {
        bool passed=true;
        for(uint32_t i=1;i<=3;++i) {
            const auto name=L"cancel-"+std::to_wstring(i);
            startRead(s,i==2 ? "" : "H3_DISCARD_"+std::to_string(i)); settle(s);
            const auto pending=s.observe(); check(!pending.readReturned && pending.controlCount==i-1,"pending read and previous event barrier");
            writeFile(out+L"\\"+name+L"-pending.json",packetJson(pending));
            if(i==1) {
                passed=keyboardOkay(c.exchange(c.next(Op::RestoreControl),Result::Accepted,"failure-restores-state"),true) && passed;
                auto denied=c.next(Op::DeniedControl); c.exchange(denied,Result::AccessDenied,"injected-access-denial");
                auto expired=c.next(Op::Cancel); expired.deadline=GetTickCount64(); c.exchange(expired,Result::Expired,"expired-cancel");
                check(!s.observe().readReturned && !s.observe().controlCount,"rejections do not cancel");
            }
            const auto request=c.next(Op::Cancel);
            passed=keyboardOkay(c.exchange(request,Result::Accepted,"cancel-read")) && passed;
            c.exchange(request,Result::Sequence,"duplicate-cancel");
            passed=captureCancellation(s,out,name,i) && passed;
            passed=nextRead(s,out,name,i) && passed;
        }
        c.close(); s.quit(); return passed;
    });
    add("no-pending-read",[](Session &s,Channel &c,const std::wstring &out) {
        bool passed=keyboardOkay(c.exchange(c.next(Op::Cancel),Result::Accepted,"no-read-cancel"));
        s.until([&] { return s.observe().controlCount!=0; }); settle(s);
        const auto p=s.observe(); writeFile(out+L"\\no-read.json",packetJson(p));
        passed=passed && p.controlCount==1 && p.controlType==CTRL_C_EVENT && !p.readReturned && p.readIndex==0;
        passed=nextRead(s,out,L"no-read",1) && passed;
        c.close(); s.quit(); return passed;
    });
    add("completed-before-cancel",[](Session &s,Channel &c,const std::wstring &out) {
        startRead(s,"COMPLETED"); finishRead(s,L"COMPLETED",out,L"before");
        bool passed=keyboardOkay(c.exchange(c.next(Op::Cancel),Result::Accepted,"cancel-after-read"));
        s.until([&] { return s.observe().controlCount!=0; }); settle(s);
        const auto p=s.observe(); writeFile(out+L"\\after.json",packetJson(p));
        passed=passed && p.readSucceeded && !p.readError && std::wstring(p.text,p.textCount)==L"COMPLETED\r\n" && p.controlCount==1;
        passed=nextRead(s,out,L"completed",1) && passed;
        c.close(); s.quit(); return passed;
    });
    add("receipt-without-ack",[](Session &s,Channel &c,const std::wstring &out) {
        startRead(s,"AMBIGUOUS");
        c.ambiguityControl(true); c.detach();
        const bool canceled=captureCancellation(s,out,L"ambiguous",1);
        const bool next=nextRead(s,out,L"ambiguous",1);
        s.quit(); return canceled && next && c.blocked();
    });
    add("close-in-request",[](Session &s,Channel &c,const std::wstring &out) {
        startRead(s,"CLOSE_REQUEST");
        const bool restored=keyboardOkay(c.exchange(c.next(Op::CancelClose),Result::Accepted,"cancel-and-retire"));
        c.detach();
        const bool canceled=captureCancellation(s,out,L"closed",1);
        const bool next=nextRead(s,out,L"closed",1);
        s.quit(); return restored && canceled && next && c.blocked();
    });
    add("default-handler",[](Session &s,Channel &c,const std::wstring &out) {
        s.observe('D');
        const bool restored=keyboardOkay(c.exchange(c.next(Op::Cancel),Result::Accepted,"default-handler"));
        check(s.until([&] { return WaitForSingleObject(s.child.value,0)==WAIT_OBJECT_0; }),"default handler exit");
        DWORD exit=0; check(GetExitCodeProcess(s.child.value,&exit),"default handler status");
        writeFile(out+L"\\default.json","{\"ExitCode\":"+std::to_string(exit)+",\"KeyboardRestored\":"+(restored ? "true" : "false")+"}");
        c.detach(); return restored && exit==0xc000013au;
    });
    add("raw-input-guard",[](Session &s,Channel &c,const std::wstring &out) {
        const auto ready=s.observe('U'); check(!(ready.inputMode&ENABLE_PROCESSED_INPUT) && !ready.readReturned,"raw readiness");
        s.input("\x03z"); IdentityPacket p;
        check(s.until([&] { p=s.observe(); return p.readReturned!=0; }),"raw read completion");
        writeFile(out+L"\\raw.json",packetJson(p));
        std::vector<Key> characters; bool modifiers=true;
        for(uint32_t i=0;i<p.keyCount;++i) {
            if(p.keys[i].character) characters.push_back(p.keys[i]);
            else modifiers=modifiers && p.keys[i].virtualKey==VK_CONTROL && p.keys[i].repeat==1;
        }
        const bool passed=p.readSucceeded && !p.readError && !p.controlCount && p.controlType==0xffffffffu && modifiers && characters.size()==2 &&
            characters[0].character==3 && characters[0].virtualKey=='C' && (characters[0].modifiers&LEFT_CTRL_PRESSED) && characters[0].repeat==1 &&
            characters[1].character=='z' && characters[1].virtualKey=='Z' && !(characters[1].modifiers&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED)) && characters[1].repeat==1;
        c.close(); s.quit(); return passed;
    });
    add("shell-owner-exit",[](Session &s,Channel &c,const std::wstring &out) {
        startOwnedShell(s,c,true);
        check(s.until([&] { return trim(s.state.row(s.state.y()))==U"H3_CMD>"; }),"shell alive before fixture exit");
        // Deliberately terminate only the owned fixture, as a fault injection.
        // Its job must clean the shell even when fixture destructors cannot run.
        check(TerminateProcess(s.child.value,73),"injected fixture death");
        const bool fixtureExited=s.until([&] { return WaitForSingleObject(s.child.value,0)==WAIT_OBJECT_0; });
        const bool shellExited=s.until([&] { return WaitForSingleObject(s.descendant.value,0)==WAIT_OBJECT_0; });
        DWORD fixtureExit=0,shellExit=0;
        check(GetExitCodeProcess(s.child.value,&fixtureExit) && GetExitCodeProcess(s.descendant.value,&shellExit),"fault injection statuses");
        writeFile(out+L"\\owner-exit.json","{\"FixtureExited\":"+std::string(fixtureExited ? "true" : "false")+
            ",\"FixtureExit\":"+std::to_string(fixtureExit)+",\"ShellExited\":"+(shellExited ? "true" : "false")+
            ",\"ShellExit\":"+std::to_string(shellExit)+",\"ExplicitShellTermination\":false}");
        c.detach(); return fixtureExited && shellExited && fixtureExit==73;
    });
    for(bool cmd:{true,false}) for(bool partial:{false,true}) {
        add(std::string(cmd ? "cmd-" : "powershell-")+(partial ? "partial" : "empty"),
            [=](Session &s,Channel &c,const std::wstring &out) { return shellCase(s,c,out,cmd,partial); });
    }
    bool passed=results.size()==12;
    std::string rows="[";
    for(const auto &r:results) {
        if(rows.size()>1) rows+=',';
        passed=passed && r.passed;
        rows+="{\"Case\":"+json(r.name)+",\"Passed\":"+(r.passed ? "true" : "false")+
            ",\"OwnedProcessesCleaned\":"+(r.cleaned ? "true" : "false")+",\"HostExited\":"+(r.hostExited ? "true" : "false")+
            ",\"HookRemoved\":"+(r.unhooked ? "true" : "false")+",\"Error\":"+json(r.error)+"}";
    }
    writeFile(output+L"\\cases.json","{\"SchemaVersion\":1,\"Passed\":"+std::string(passed ? "true" : "false")+",\"Cases\":"+rows+"]}");
    return passed ? 0 : 1;
}
int neighbor(const std::wstring &output) {
    Session s(100); Channel c(s,output,L"neighbor"); registerSession(s,c);
    startRead(s,"BRAVO_KEEP");
    Registration ready; ready.kind=2; ready.ready=1; transfer(parentPipe,&ready,sizeof(ready),true);
    unsigned int observations=0;
    for(;;) {
        char command=0; transfer(parentPipe,&command,1,false,180000);
        const auto p=s.observe();
        check(!p.readReturned && !p.controlCount,"neighbor remains pending without events"); ++observations;
        if(command=='Q') break;
        check(command=='I',"neighbor observation command");
    }
    c.close(); finishRead(s,L"BRAVO_KEEP",output,L"neighbor-read"); s.quit();
    const bool exited=WaitForSingleObject(s.host.value,5000)==WAIT_OBJECT_0;
    writeFile(output+L"\\neighbor.json","{\"Passed\":"+std::string(exited && observations>1 ? "true" : "false")+
        ",\"PendingObservations\":"+std::to_string(observations)+",\"HostExited\":"+(exited ? "true" : "false")+"}");
    writeFile(output+L"\\stream.bin",s.raw);
    return exited && observations>1 ? 0 : 1;
}
