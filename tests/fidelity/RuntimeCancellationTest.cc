// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#define VT7PTY_H4_RUNTIME
#define VT7PTY_H3_DIAGNOSTIC
#define VT7PTY_H3_GUARDS
#include "../../src/shared/HostCancellationProtocol.h"
#include "../../src/shared/DesktopState.h"
#include "../../src/shared/GenRandom.h"
#include <psapi.h>
#include "GenVersion.h"
#include "HostCancellationResearch.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace fidelity;
using namespace cancelresearch;
namespace
{
std::wstring outRoot, applicationDesktop;
bool rootEvidenceCreated = false;
std::string desktopJson();
bool desktopPreflight(const std::wstring &path);
unsigned int nextSession = 0;
std::string boolean(bool v)
{
    return v ? "true" : "false";
}
std::wstring nameOf(HANDLE object)
{
    wchar_t text[256] = {};
    DWORD bytes = 0;
    check(GetUserObjectInformationW(object, UOI_NAME, text, sizeof(text), &bytes), "object name");
    return text;
}
bool windowOn(HDESK desktop, HWND expected)
{
    struct State
    {
        HWND expected;
        bool found = false;
    } state{expected};
    check(EnumDesktopWindows(
              desktop,
              [](HWND w, LPARAM data) -> BOOL {
                  auto &s = *reinterpret_cast<State *>(data);
                  if (w == s.expected)
                      s.found = true;
                  return TRUE;
              },
              reinterpret_cast<LPARAM>(&state)),
          "desktop window enumeration");
    return state.found;
}
struct Desktop
{
    HDESK value = nullptr;
    std::wstring name;
    ~Desktop()
    {
        if (value)
            CloseDesktop(value);
    }
    void find(HWND window)
    {
        struct State
        {
            HWND window;
            HDESK value = nullptr;
            std::wstring name;
            unsigned int count = 0;
        } state{window};
        check(EnumDesktopsW(
                  GetProcessWindowStation(),
                  [](LPWSTR name, LPARAM data) -> BOOL {
                      auto &s = *reinterpret_cast<State *>(data);
                      if (wcsncmp(name, L"VT7Pty-Console-", 15) != 0)
                          return TRUE;
                      const HDESK desktop =
                          OpenDesktopW(name, 0, FALSE, DESKTOP_ENUMERATE | DESKTOP_READOBJECTS);
                      if (!desktop)
                          return TRUE;
                      struct Match
                      {
                          HWND window;
                          bool found = false;
                      } match{s.window};
                      const BOOL ok = EnumDesktopWindows(
                          desktop,
                          [](HWND w, LPARAM p) -> BOOL {
                              auto &m = *reinterpret_cast<Match *>(p);
                              if (w == m.window)
                                  m.found = true;
                              return TRUE;
                          },
                          reinterpret_cast<LPARAM>(&match));
                      if (ok && match.found)
                      {
                          ++s.count;
                          if (s.value)
                              CloseDesktop(s.value);
                          s.value = desktop;
                          s.name = name;
                      }
                      else
                          CloseDesktop(desktop);
                      return TRUE;
                  },
                  reinterpret_cast<LPARAM>(&state)),
              "enumerate backing desktops");
        value = state.value;
        name = state.name;
        check(state.count == 1 && value != nullptr, "unique backing console desktop");
    }
};
struct Context
{
    std::wstring out;
    std::unique_ptr<Session> session;
    Desktop desktop;
    bool cleaned = true, hostExited = true;
    std::string error;
    explicit Context(const std::wstring &path) : out(path)
    {
        check(CreateDirectoryW(out.c_str(), nullptr), "new case directory");
    }
    Session &start()
    {
        check(SetEnvironmentVariableW(L"VT7PTY_H3_GUARD_OUTPUT", out.c_str()),
              "case-local editor history");
        session = std::make_unique<Session>(++nextSession, L"", VT7PTY_FLAG_HOST_CANCELLATION);
        auto &s = *session;
        check(nameOf(GetThreadDesktop(GetCurrentThreadId())) == applicationDesktop,
              "caller desktop unchanged");
        check(observeIdentity(s, out, L"identity"), "runtime identity");
        const auto p = s.observe();
        check(std::wstring(p.fixtureDesktop) == applicationDesktop,
              "application stays on caller desktop");
        check(
            windowOn(GetThreadDesktop(GetCurrentThreadId()), reinterpret_cast<HWND>(p.fixtureGui)),
            "application GUI stays on ordinary desktop");
        desktop.find(reinterpret_cast<HWND>(p.window));
        check(desktop.name != applicationDesktop, "backing console is isolated");
        writeFile(out + L"\\desktop.json",
                  "{\"ApplicationDesktop\":" + json(utf8(applicationDesktop)) +
                      ",\"BackingDesktop\":" + json(utf8(desktop.name)) +
                      ",\"ApplicationGuiWindow\":" + std::to_string(p.fixtureGui) +
                      ",\"ConsoleWindow\":" + std::to_string(p.window) +
                      ",\"ApplicationGuiOrdinary\":true,\"HostWindowPrivate\":true}");
        return s;
    }
    bool finish(bool quit = true)
    {
        if (!session)
            return true;
        try
        {
            if (quit)
                session->quit();
            else
                check(session->cleanup(), "closed-session cleanup");
        }
        catch (const std::exception &e)
        {
            error = e.what();
            session->cleanup();
        }
        cleaned = session->cleanupOkay;
        if (session->host.value && session->host.value != INVALID_HANDLE_VALUE)
            hostExited = WaitForSingleObject(session->host.value, 10000) == WAIT_OBJECT_0;
        if(!session->profilePrivateOutput)writeFile(out + L"\\stream.bin", session->raw);
        writeFile(out + L"\\lifetime.json",
                  "{\"Agent\":" + std::to_string(GetProcessId(session->agent.value)) +
                      ",\"Fixture\":" + std::to_string(GetProcessId(session->child.value)) +
                      ",\"Host\":" + std::to_string(GetProcessId(session->host.value)) +
                      ",\"OwnedProcessesCleaned\":" + boolean(cleaned) +
                      ",\"HostExited\":" + boolean(hostExited) + ",\"Error\":" + json(error) + "}");
        return cleaned && hostExited && error.empty();
    }
};
bool canceled(const IdentityPacket &p, uint32_t count)
{
    return p.readReturned && p.readError == ERROR_OPERATION_ABORTED && p.textCount == 0 &&
           p.controlCount == count && p.controlType == CTRL_C_EVENT;
}
IdentityPacket waitRead(Session &s, uint32_t events)
{
    IdentityPacket p;
    check(s.until(
              [&] {
                  p = s.observe();
                  bool handlerExited = events==0;
                  for(uint32_t i=0;i<p.trace.eventCount;++i)
                      handlerExited = handlerExited || (p.trace.events[i].kind==h3trace::HandlerExit &&
                          p.trace.events[i].detail==events);
                  return p.readReturned && p.controlCount >= events && handlerExited;
              },
              3000),
          "read/event deadline");
    return p;
}
void nextRead(Session &s, const std::wstring &out, uint32_t count)
{
    auto p = s.observe('R');
    check(!p.readReturned, "next read starts pending");
    s.input("NEXT_PROBE\r");
    p = waitRead(s, count);
    writeFile(out + L"\\next-read.json", packetJson(p));
    check(p.readSucceeded && p.readError == 0 && p.controlCount == count &&
              std::wstring(p.text, p.textCount) == L"NEXT_PROBE\r\n",
          "next read exactly preserved");
}
void cooked(Context &c, const std::string &text, unsigned int repetitions = 1)
{
    auto &s = c.start();
    for (unsigned int n = 1; n <= repetitions; ++n)
    {
        startRead(s, text);
        s.input("\x03");
        const auto p = waitRead(s, n);
        writeFile(c.out + L"\\cancel-" + std::to_wstring(n) + L".json", packetJson(p));
        check(canceled(p, n), "cooked read canceled exactly once");
    }
    nextRead(s, c.out, repetitions);
}
void raw(Context &c)
{
    auto &s = c.start();
    check(!s.observe('U').readReturned, "raw read starts");
    s.input("\x03z");
    const auto p = waitRead(s, 0);
    writeFile(c.out + L"\\raw-read.json", packetJson(p));
    check(p.controlCount == 0 && p.keyCount == 3 && p.keys[0].character == 0 &&
              p.keys[0].virtualKey == VK_CONTROL && (p.keys[0].modifiers & LEFT_CTRL_PRESSED) &&
              p.keys[1].character == 3 && p.keys[1].virtualKey == 'C' &&
              (p.keys[1].modifiers & LEFT_CTRL_PRESSED) && p.keys[2].character == 'z' &&
              p.keys[2].virtualKey == 'Z' &&
              !(p.keys[2].modifiers & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)),
          "raw modifier, C and next key preserved");
    std::vector<GuardInputRecord> keys;
    for (uint32_t n = 0; n < p.guardInputCount; ++n)
        if (p.guardInput[n].type == KEY_EVENT)
            keys.push_back(p.guardInput[n]);
    check(keys.size() == 5 && keys[0].down && keys[0].virtualKey == VK_CONTROL && keys[1].down &&
              keys[1].virtualKey == 'C' && !keys[2].down && keys[2].virtualKey == 'C' &&
              !keys[3].down && keys[3].virtualKey == VK_CONTROL && keys[4].down &&
              keys[4].virtualKey == 'Z',
          "raw input record order");
    nextRead(s, c.out, 0);
}
void burst(Context &c)
{
    auto &s = c.start();
    check(!s.observe('R').readReturned, "burst pending read");
    s.input(std::string("burstold") + '\x03');
    auto p = waitRead(s, 1);
    writeFile(c.out + L"\\burst-read.json", packetJson(p));
    check(canceled(p, 1), "same-write text canceled");
    nextRead(s, c.out, 1);
}
void closeConcurrent(Context &c)
{
    auto &s = c.start();
    startRead(s, "closing");
    auto *pty = s.runtimeHandle();
    std::atomic<bool> stop = false;
    std::atomic<unsigned int> successes = 0, closed = 0, unexpected = 0;
    std::thread queries([&] {
        for (unsigned int i = 0; i < 200 && !stop.load(); ++i)
        {
            vt7pty_error_ptr_t error = nullptr;
            if (vt7pty_set_size(pty, 80 + static_cast<int>(i % 2), 25, &error))
                ++successes;
            else if (vt7pty_error_code(error) == VT7PTY_ERROR_LOST_CONNECTION)
                ++closed;
            else
                ++unexpected;
            vt7pty_error_free(error);
            Sleep(1);
        }
    });
    Sleep(5);
    vt7pty_error_ptr_t error = nullptr;
    const BOOL ok = vt7pty_close(pty, &error);
    const auto code = vt7pty_error_code(error);
    vt7pty_error_free(error);
    stop = true;
    queries.join();
    check(ok && code == 0 && unexpected == 0, "close serializes with concurrent RPCs");
    check(vt7pty_close(pty, nullptr), "close idempotent");
    error = nullptr;
    check(!vt7pty_set_size(pty, 80, 25, &error) &&
              vt7pty_error_code(error) == VT7PTY_ERROR_LOST_CONNECTION,
          "closed RPC rejected");
    vt7pty_error_free(error);
    check(vt7pty_agent_process(pty) != nullptr && vt7pty_conin_name(pty) != nullptr,
          "borrowed identities retained until free");
    writeFile(c.out + L"\\close.json",
              "{\"CloseSucceeded\":true,\"Idempotent\":true,\"PostCloseRpcRejected\":true,"
              "\"AllocationRetained\":true,\"ConcurrentRpcSuccesses\":" +
                  std::to_string(successes.load()) +
                  ",\"ConcurrentRpcClosed\":" + std::to_string(closed.load()) +
                  ",\"UnexpectedRpcErrors\":" + std::to_string(unexpected.load()) + "}");
}
void agentDeath(Context &c)
{
    auto &s = c.start();
    startRead(s, "agent-death");
    check(TerminateProcess(s.agent.value, 123), "terminate only the owned agent");
    check(WaitForSingleObject(s.agent.value, 5000) == WAIT_OBJECT_0, "owned agent stopped");
    const bool childExit = WaitForSingleObject(s.child.value, 3000) == WAIT_OBJECT_0;
    const bool hostExit = WaitForSingleObject(s.host.value, 1000) == WAIT_OBJECT_0;
    writeFile(c.out + L"\\agent-death.json",
              "{\"OwnedAgentTerminated\":true,\"ChildExitedWithoutHarnessTermination\":" +
                  boolean(childExit) + ",\"HostExitedWithoutTermination\":" + boolean(hostExit) +
                  "}");
    check(childExit && hostExit, "agent death must close its attached clients and host");
}
#include "H4RuntimeCases.h"
#include "H4QualificationCases.h"
#include "InputFidelityCases.h"
#include "ProfileCases.h"
#include "ProfileManual.h"
void runtimeOracle()
{
    using namespace vt7pty::internal::cancel;
    manualEncodingOracle();
    unsigned int count = 0;
    const auto test = [&](bool ok) {
        check(ok, "runtime protocol oracle");
        ++count;
    };
    Request base;
    base.nonce[0] = 1;
    base.nonce[1] = 2;
    base.sequence = 2;
    base.deadline = 2000;
    base.operation = Operation::Cancel;
    base.identity.agent = 3;
    base.identity.host = 4;
    test(next(base, base, 1, 1000));
    auto q = base;
    q.signature = 0;
    test(!next(q, base, 1, 1000));
    q = base;
    ++q.schema;
    test(!next(q, base, 1, 1000));
    q = base;
    --q.bytes;
    test(!next(q, base, 1, 1000));
    q = base;
    ++q.nonce[0];
    test(!next(q, base, 1, 1000));
    q = base;
    ++q.identity.host;
    test(!next(q, base, 1, 1000));
    q = base;
    ++q.identity.hostCreated;
    test(!next(q, base, 1, 1000));
    test(!next(base, base, 2, 1000));
    q = base;
    ++q.sequence;
    test(!next(q, base, 1, 1000));
    q = base;
    q.sequence = 0;
    test(!next(q, base, UINT64_MAX, 1000));
    q = base;
    q.deadline = 1000;
    test(!next(q, base, 1, 1000));
    q = base;
    q.deadline = 6001;
    test(!next(q, base, 1, 1000));
    q = base;
    q.operation = Operation::Bind;
    test(!next(q, base, 1, 1000));
    q = base;
    q.operation = static_cast<Operation>(999);
    test(!next(q, base, 1, 1000));
    q = base;
    q.operation = Operation::Close;
    test(next(q, base, 1, 1000));
    q = base;
    q.identity.reserved = 1;
    test(!frame(q, 1000));
    test(!next(q, base, 1, 1000));
    Keyboard keys;
    keys.captured = keys.applied = keys.sent = keys.restored = keys.equal = 1;
    keys.during[VK_CONTROL] = keys.during[VK_LCONTROL] = 0x80;
    test(restoredKeyboard(keys));
    auto bad = keys;
    bad.after[0] = 1;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.during[VK_MENU] = 0x80;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.error = 5;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.sent = 0;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.restored = 0;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.captured = 2;
    test(!restoredKeyboard(bad));
    bad = keys;
    bad.during['Q'] = 1;
    test(!restoredKeyboard(bad));
    h3trace::Timeline timeline;
    for(unsigned int i=0;i<100;++i) timeline.append(h3trace::ReadEntry,i);
    h3trace::Observation observation;
    h3trace::observe(timeline,observation);
    test(observation.eventCount==64 && observation.oldestSequence==37 && observation.nextSequence==101);
    test(observation.events[0].sequence==37 && observation.events[63].sequence==100 && observation.overflow==0);
    printf("Runtime cancellation oracle: %u checks passed, plus four keyboard-bridge checks\n", count);
}
struct Case
{
    std::string name;
    std::function<void(Context &)> action;
    bool close = false;
};
int run(const std::wstring &out, const std::string &filter = "")
{
    runtimeOracle();
    outRoot = out;
    check(CreateDirectoryW(outRoot.c_str(), nullptr), "new runtime result directory");
    rootEvidenceCreated = true;
    applicationDesktop = nameOf(GetThreadDesktop(GetCurrentThreadId()));
    check(desktopPreflight(outRoot + L"\\preflight.json"), "ordinary desktop precondition");
    wchar_t layout[KL_NAMELENGTH] = {};
    check(GetKeyboardLayoutNameW(layout) != FALSE, "caller keyboard layout");
    check(SetEnvironmentVariableW(L"VT7PTY_DEBUG", L"trace"), "enable runtime evidence trace");
    Collector collector(outRoot);
    std::vector<Case> cases = {
        {"desktop-boundary", [](Context &c) { c.start(); }},
        {"cooked-empty", [](Context &c) { cooked(c, ""); }},
        {"cooked-partial", [](Context &c) { cooked(c, "discardme"); }},
        {"cooked-repeated", [](Context &c) { cooked(c, "repeated", 3); }},
        {"raw-input", raw},
        {"input-burst", burst},
        {"no-pending-read", [](Context &c) { noRead(c, false); }},
        {"completed-before-cancel", [](Context &c) { noRead(c, true); }},
        {"default-handler", defaultHandler, true},
        {"mode-transitions", transitions},
        {"race-enter-biased", [](Context &c) { completionRace(c, 0, 100); }},
        {"race-concurrent", [](Context &c) { completionRace(c, 0, 0); }},
        {"race-cancel-biased", [](Context &c) { completionRace(c, 100, 0); }},
        {"race-outcomes",
         [](Context &c) {
             writeFile(c.out + L"\\coverage.json",
                       "{\"Completed\":" + std::to_string(completedRaces) +
                           ",\"Canceled\":" + std::to_string(abortedRaces) + "}");
             check(completedRaces && abortedRaces && completedRaces + abortedRaces == 3,
                   "both completion outcomes");
         }},
        {"close-rpc", closeConcurrent, true},
        {"close-input", closeInput, true},
        {"agent-death", agentDeath, true},
        {"caller-death", [](Context &c) { callerDeath(c, false); }},
        {"caller-and-agent-death", [](Context &c) { callerDeath(c, true); }},
        {"enclosing-job", [](Context &c) { callerDeath(c, false, true); }},
        {"neighbor-isolation", neighbor},
        {"recreate-resources", cycles},
        {"impersonation-denied", impersonationDenied},
        {"helper-missing", [](Context &c) { helperFailure(c, false); }},
        {"helper-wrong", [](Context &c) { helperFailure(c, true); }}};
    wchar_t legacy[2] = {};
    const bool ps7 =
        !(GetEnvironmentVariableW(L"VT7PTY_H4_LEGACY", legacy, 2) == 1 && legacy[0] == L'1');
    for (char kind : {'M', 'W', 'P', 'L'})
    {
        if (!ps7 && (kind == 'P' || kind == 'L'))
            continue;
        const std::string shell = kind == 'M'   ? "cmd"
                                  : kind == 'W' ? "powershell"
                                  : kind == 'P' ? "pwsh-native"
                                                : "pwsh-readline";
        for (bool partial : {false, true})
        {
            const auto name = shell + (partial ? "-partial" : "-empty");
            // Case owns its string because the inventory is constructed dynamically.
            cases.push_back({name, [=](Context &c) { shellPrompt(c, kind, partial); }});
        }
        for (bool cookedRead : {false, true})
            cases.push_back({"child-" + shell + (cookedRead ? "-cooked" : "-event"),
                             [=](Context &c) { activeChild(c, kind, cookedRead); }});
    }
    if (!filter.empty())
    {
        cases.erase(std::remove_if(cases.begin(), cases.end(),
                                   [&](const Case &c) { return c.name != filter; }),
                    cases.end());
        check(cases.size() == 1, "one selected runtime case");
    }
    std::string rows = "[";
    unsigned int passed = 0;
    for (size_t i = 0; i < cases.size(); ++i)
    {
        if (i)
            rows += ',';
        Context c(outRoot + L"\\" + std::wstring(cases[i].name.begin(), cases[i].name.end()));
        bool ok = false;
        std::string error;
        try
        {
            cases[i].action(c);
            ok = true;
        }
        catch (const std::exception &e)
        {
            error = e.what();
        }
        try
        {
            ok = c.finish(ok && !cases[i].close) && ok;
        }
        catch (const std::exception &e)
        {
            ok = false;
            if (error.empty())
                error = e.what();
        }
        if (error.empty() && !c.error.empty())
            error = c.error;
        if (ok)
            ++passed;
        rows += "{\"Case\":" + json(cases[i].name) + ",\"Passed\":" + boolean(ok) +
                ",\"OwnedProcessesCleaned\":" + boolean(c.cleaned) +
                ",\"HostExited\":" + boolean(c.hostExited) + ",\"Error\":" + json(error) + "}";
        printf("%s: %s %s\n", ok ? "PASS" : "FAIL", cases[i].name.c_str(), error.c_str());
    }
    collector.stop();
    rows += "]";
    writeFile(outRoot + L"\\cases.json",
              "{\"Kind\":\"H4RuntimeCases\",\"SchemaVersion\":1,\"Cases\":" + rows + "}");
    writeFile(outRoot + L"\\probe.json",
              "{\"Kind\":\"H4RuntimeProbe\",\"SchemaVersion\":1,\"QualificationRevision\":1,\"Status\":" +
                  json(passed == cases.size() ? "Pass" : "Fail") + ",\"Passed\":" +
                  std::to_string(passed) + ",\"Total\":" + std::to_string(cases.size()) +
                  ",\"PackageVersion\":" + json(GenVersion_Version) +
                  ",\"SourceCommit\":" + json(GenVersion_Commit) +
                  ",\"ApplicationDesktop\":" + json(utf8(applicationDesktop)) +
                  ",\"CallerKeyboardLayout\":" + json(utf8(layout)) +
                  ",\"OracleChecks\":27,\"RuntimeIntegrated\":true,\"H4Accepted\":false}");
    return passed == cases.size() ? 0 : 1;
}
void recordNativeFailure(const std::string &message) noexcept
{
    if(!rootEvidenceCreated)return;
    try {
        const auto path=outRoot+L"\\failure.json";
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)return;
        writeFile(path,"{\"Kind\":\"H4NativeFailure\",\"Status\":\"Fail\",\"SourceCommit\":"+
            json(GenVersion_Commit)+",\"Error\":"+json(message)+",\"H4Accepted\":false}");
    }catch(...){}
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        if(argc==4 && std::wstring(argv[1])==L"--profiles-manual")return profilecases::manual(argv[3],std::wstring(argv[2])==L"pwsh");
        if(argc==4 && std::wstring(argv[1])==L"--profiles-manual-smoke")return profilecases::smoke(argv[3],std::wstring(argv[2])==L"pwsh");
        if(argc==4 && std::wstring(argv[1])==L"--profiles-manual-smoke-worker")return profilecases::manual(argv[3],std::wstring(argv[2])==L"pwsh",true);
        if(argc==4 && std::wstring(argv[1])==L"--profile-child")return profilecases::child(argv[2],utf8(argv[3]));
        if(argc==2 && std::wstring(argv[1])==L"--profiles-list"){profilecases::list();return 0;}
        if(argc==3 && std::wstring(argv[1])==L"--profiles-publication-self-test")return profilecases::publicationOracle(argv[2]);
        if(argc==2 && std::wstring(argv[1])==L"--profiles-self-test"){profilecases::oracle();return 0;}
        if(argc==3 && std::wstring(argv[1])==L"--profiles-run")return profilecases::run(argv[2]);
        if(argc==5 && std::wstring(argv[1])==L"--input-break-child")
            return inputfidelity::breakChild(argv[2],static_cast<DWORD>(_wcstoui64(argv[3],nullptr,10)),std::wstring(argv[4])==L"1");
        if(argc==2 && std::wstring(argv[1])==L"--input-self-test"){inputcases::oracle();return 0;}
        if(argc==2 && std::wstring(argv[1])==L"--input-list"){printf("%s\n",inputcases::listJson().c_str());return 0;}
        if(argc==3 && std::wstring(argv[1])==L"--input-run")return inputcases::run(argv[2]);
        if(argc==4 && std::wstring(argv[1])==L"--input-case")return inputcases::run(argv[3],utf8(argv[2]));
        if (argc == 2 && std::wstring(argv[1]) == L"--manual-self-test") {
            manualDisplayOracle();return 0;
        }
        if (argc == 4 && std::wstring(argv[1]) == L"--manual-smoke")
            return runKeyboard(argv[3],argv[2],true);
        if (argc == 3 && std::wstring(argv[1]) == L"--preflight")
            return desktopPreflight(argv[2]) ? 0 : 2;
        if (argc == 3 && std::wstring(argv[1]) == L"--qualify")
            return runQualification(argv[2]);
        if (argc == 4 && std::wstring(argv[1]) == L"--soak")
            return runOptInSoak(argv[3], static_cast<unsigned int>(_wtoi(argv[2])));
        if (argc == 4 && std::wstring(argv[1]) == L"--keyboard")
            return runKeyboard(argv[3], argv[2]);
        if (argc == 3 && std::wstring(argv[1]) == L"--session-smoke")
            return runSessionCheck(argv[2],true);
        if (argc == 3 && std::wstring(argv[1]) == L"--session-check")
            return runSessionCheck(argv[2]);
        if (argc == 3 && std::wstring(argv[1]) == L"--access-check")
            return runAccessCheck(argv[2]);
        if (argc == 2 && std::wstring(argv[1]) == L"--self-test")
        {
            runtimeOracle();
            return 0;
        }
        if (argc == 4 && std::wstring(argv[1]) == L"--fixture")
        {
            runtimeFixtureGui =
                CreateWindowExW(0, L"STATIC", L"VT7Pty runtime GUI guard", WS_POPUP, 0, 0, 1, 1,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            check(runtimeFixtureGui != nullptr, "hidden ordinary GUI guard");
            const int code = fixture(argv[2], static_cast<DWORD>(_wcstoui64(argv[3], nullptr, 10)));
            DestroyWindow(runtimeFixtureGui);
            return code;
        }
        if (argc == 4 && std::wstring(argv[1]) == L"--lifetime-actor")
            return lifetimeActor(argv[2], static_cast<DWORD>(_wcstoui64(argv[3], nullptr, 10)));
        if (argc == 3 && std::wstring(argv[1]) == L"--startup-check")
            return startupCheck(argv[2]);
        if (argc == 4 && std::wstring(argv[1]) == L"--case")
            return run(argv[3], utf8(argv[2]));
        check(argc == 3 && std::wstring(argv[1]) == L"--run", "usage: --run DIRECTORY");
        return run(argv[2]);
    }
    catch (const Exception &e)
    {
        recordNativeFailure(utf8(e.what()));
        fprintf(stderr, "Runtime cancellation: %s\n", utf8(e.what()).c_str());
        return 1;
    }
    catch (const std::exception &e)
    {
        recordNativeFailure(e.what());
        fprintf(stderr, "Runtime cancellation: %s\n", e.what());
        return 1;
    }
}
