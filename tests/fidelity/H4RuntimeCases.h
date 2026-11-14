// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once

std::string fileText(const std::wstring &path)
{
    std::ifstream file{std::filesystem::path(path)};
    std::string text((std::istreambuf_iterator<char>(file)), {});
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
        text.pop_back();
    return text;
}
std::wstring psLiteral(const std::wstring &text)
{
    std::wstring result = L"'";
    for (wchar_t ch : text)
    {
        result += ch;
        if (ch == L'\'')
            result += ch;
    }
    return result + L"'";
}
std::u32string row(Session &session)
{
    auto text = session.state.row(session.state.y());
    while (!text.empty() && text.back() == U' ')
        text.pop_back();
    return text;
}
void noRead(Context &context, bool completed)
{
    auto &s = context.start();
    if (completed)
    {
        startRead(s, "completed");
        finishRead(s, L"completed", context.out, L"completed");
    }
    s.input("\x03");
    check(s.until([&] { return s.observe().controlCount == 1; }), "event without pending read");
    writeFile(context.out + L"\\event.json", packetJson(s.observe()));
    nextRead(s, context.out, 1);
}
void defaultHandler(Context &context)
{
    auto &s = context.start();
    startRead(s, "default-handler");
    s.observe('D');
    s.input("\x03");
    check(s.until([&] { return WaitForSingleObject(s.child.value, 0) == WAIT_OBJECT_0; }),
          "default handler exits child");
    DWORD code = 0;
    check(GetExitCodeProcess(s.child.value, &code) && code == 0xc000013au,
          "default handler exit status");
    writeFile(context.out + L"\\default.json",
              "{\"ChildExited\":true,\"ExitCode\":" + std::to_string(code) +
                  ",\"HarnessTermination\":false}");
}
void transitions(Context &context)
{
    raw(context);
    auto &s = *context.session;
    startRead(s, "cooked-after-raw");
    s.input("\x03");
    const auto p = waitRead(s, 1);
    writeFile(context.out + L"\\transition.json", packetJson(p));
    check(canceled(p, 1), "raw-to-cooked route change");
    nextRead(s, context.out, 1);
}
unsigned int completedRaces = 0, abortedRaces = 0;
void completionRace(Context &context, DWORD enterDelay, DWORD cancelDelay)
{
    auto &s = context.start();
    startRead(s, "racekeep");
    Handle barrier(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    check(barrier.value != nullptr, "race barrier");
    std::string errorA, errorB;
    uint64_t enterBegin = 0, enterEnd = 0, cancelBegin = 0, cancelEnd = 0;
    std::atomic<unsigned int> ready = 0;
    std::thread enter([&] {
        ++ready;
        WaitForSingleObject(barrier.value, INFINITE);
        Sleep(enterDelay);
        enterBegin = h3trace::qpc();
        try
        {
            s.input("\r");
        }
        catch (const std::exception &e)
        {
            errorA = e.what();
        }
        enterEnd = h3trace::qpc();
    });
    std::thread cancel([&] {
        ++ready;
        WaitForSingleObject(barrier.value, INFINITE);
        Sleep(cancelDelay);
        cancelBegin = h3trace::qpc();
        try
        {
            s.input("\x03");
        }
        catch (const std::exception &e)
        {
            errorB = e.what();
        }
        cancelEnd = h3trace::qpc();
    });
    while (ready.load() != 2)
        Sleep(1);
    SetEvent(barrier.value);
    enter.join();
    cancel.join();
    check(errorA.empty() && errorB.empty(), "both input contenders complete");
    auto p = waitRead(s, 1);
    const bool aborted = canceled(p, 1);
    const bool completed = p.readSucceeded && !p.readError && p.controlCount == 1 &&
                           p.controlType == CTRL_C_EVENT &&
                           std::wstring(p.text, p.textCount) == L"racekeep\r\n";
    writeFile(context.out + L"\\race-read.json", packetJson(p));
    check(aborted || completed, "completion race has one valid read outcome");
    if (aborted)
        ++abortedRaces;
    else
        ++completedRaces;
    // Preserve and inspect the contender's possible residual Enter. Never flush.
    check(!s.observe('F').readReturned, "race input fence ready");
    s.input("z");
    p = waitRead(s, 1);
    unsigned int enters = 0, zDown = 0, zUp = 0;
    bool valid = p.readSucceeded && !p.readError && p.guardInputCount <= 6;
    for (uint32_t i = 0; i < p.guardInputCount; ++i)
    {
        const auto &k = p.guardInput[i];
        if (k.type != KEY_EVENT || k.repeat != 1)
        {
            valid = false;
            continue;
        }
        if (k.virtualKey == VK_RETURN && k.character == L'\r')
        {
            if (k.down)
                ++enters;
        }
        else if (k.virtualKey == 'Z' && k.character == 'z')
        {
            if (k.down)
                ++zDown;
            else
                ++zUp;
        }
        else
            valid = false;
    }
    writeFile(context.out + L"\\race-fence.json", packetJson(p));
    check(valid && enters <= 1 && zDown == 1 && zUp == 1,
          "race fence contains only planned Enter and z");
    nextRead(s, context.out, 1);
    writeFile(context.out + L"\\race.json",
              "{\"EnterDelayMs\":" + std::to_string(enterDelay) + ",\"CancelDelayMs\":" +
                  std::to_string(cancelDelay) + ",\"EnterBeginQpc\":" + h3trace::u64(enterBegin) +
                  ",\"EnterEndQpc\":" + h3trace::u64(enterEnd) + ",\"CancelBeginQpc\":" +
                  h3trace::u64(cancelBegin) + ",\"CancelEndQpc\":" + h3trace::u64(cancelEnd) +
                  ",\"Frequency\":" + h3trace::u64(h3trace::frequency()) +
                  ",\"Outcome\":" + json(aborted ? "Canceled" : "Completed") +
                  ",\"InputFencePassed\":true,\"NextReadPreserved\":true,\"ETXWrites\":1,"
                  "\"CompletionEnters\":1,\"RetrySent\":false}");
}
void closeInput(Context &context)
{
    auto &s = context.start();
    startRead(s, "closing-input");
    std::string inputError;
    DWORD inputCode = 0;
    bool closed = false;
    Handle barrier(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::thread writer([&] {
        WaitForSingleObject(barrier.value, INFINITE);
        try
        {
            s.input("\x03");
        }
        catch (const std::exception &e)
        {
            inputCode = GetLastError();
            inputError = e.what();
        }
    });
    std::thread closer([&] {
        WaitForSingleObject(barrier.value, INFINITE);
        closed = vt7pty_close(s.runtimeHandle(), nullptr) != FALSE;
    });
    SetEvent(barrier.value);
    writer.join();
    closer.join();
    check(closed, "concurrent input/close");
    check(inputError.empty() || inputCode == ERROR_BROKEN_PIPE || inputCode == ERROR_NO_DATA ||
              inputCode == ERROR_PIPE_NOT_CONNECTED,
          "only a closed input pipe may reject the racing write");
    writeFile(context.out + L"\\close-input.json",
              "{\"Closed\":true,\"InputCompleted\":" + boolean(inputError.empty()) +
                  ",\"InputError\":" + json(inputError) +
                  ",\"InputErrorCode\":" + std::to_string(inputCode) + ",\"RetrySent\":false}");
}
void neighbor(Context &context)
{
    auto &s = context.start();
    startRead(s, "NEIGHBOR_KEEP");
    for (unsigned int i = 0; i < 3; ++i)
    {
        Context other(context.out + L"\\neighbor-" + std::to_wstring(i));
        cooked(other, "other");
        check(other.finish(), "neighbor cleanup");
        check(!s.observe().readReturned && s.observe().controlCount == 0,
              "neighbor C leaves original read pending");
    }
    finishRead(s, L"NEIGHBOR_KEEP", context.out, L"neighbor-read");
    nextRead(s, context.out, 0);
}
void cycles(Context &context)
{
    DWORD before = 0, after = 0;
    check(GetProcessHandleCount(GetCurrentProcess(), &before), "initial handle count");
    for (unsigned int i = 0; i < 6; ++i)
    {
        Context cycle(context.out + L"\\cycle-" + std::to_wstring(i));
        cooked(cycle, "cycle");
        check(cycle.finish(), "recreated session cleanup");
    }
    check(GetProcessHandleCount(GetCurrentProcess(), &after), "final handle count");
    writeFile(context.out + L"\\resources.json",
              "{\"Cycles\":6,\"HandlesBefore\":" + std::to_string(before) +
                  ",\"HandlesAfter\":" + std::to_string(after) + ",\"AllowedGrowth\":2}");
    check(after <= before + 2, "recreated sessions release caller handles");
}
struct Shell
{
    Session &s;
    char kind;
    bool cmd;
    uint32_t generation = 0, commandIndex = 0;
    std::u32string prompt = U"H3_CMD>";
    std::string token, metadata;
    Shell(Context &context, char variant) : s(*context.session), kind(variant), cmd(variant == 'M')
    {
        const auto identity = s.observe(kind);
        check(identity.shellId && identity.shellAlive, "shell created");
        s.descendant.reset(
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE,
                        identity.shellId));
        check(s.descendant.value &&
                  parentProcessId(identity.shellId) == GetProcessId(s.child.value) &&
                  agrees(hostQuery(s.descendant.value), hostQuery(s.agent.value)),
              "retained owned shell");
        check(s.until([&] { return atPrompt(); }, 20000), "shell initial prompt");
        if (cmd)
        {
            token = "H4_CMD_" + std::to_string(GetProcessId(s.child.value));
            setCmdPrompt();
            metadata = "System32 cmd.exe";
        }
        else
        {
            const auto path = context.out + L"\\shell-identity.txt";
            const auto previous = generation;
            const auto query =
                L"$m=Get-Module PSReadLine; $v='none'; $cfg='native'; "
                L"if($m){$v=$m.Version.ToString();$o=Get-PSReadLineOption;$cfg=$m.Path+'|'+$o."
                L"EditMode.ToString()+'|'+$o.HistorySaveStyle.ToString()}; "
                L"[IO.File]::WriteAllText(" +
                psLiteral(path) +
                L",$PSVersionTable.PSVersion.ToString()+'|'+$v+'|'+$Host.Name+'|'+$cfg)";
            s.input(utf8(query + L"\r"));
            check(s.until(
                      [&] {
                          metadata = fileText(path);
                          return metadata.find("|ConsoleHost|") != std::string::npos &&
                                 atPrompt() && generation > previous;
                      },
                      10000),
                  "shell editor identity");
            if (kind == 'P' || kind == 'L')
                check(metadata.starts_with("7.2.24|"), "pinned PowerShell 7");
            if (kind == 'L')
                check(metadata.find("|none|") == std::string::npos &&
                          metadata.ends_with("|Windows|SaveNothing"),
                      "bundled PSReadLine configuration");
            else
                check(metadata.find("|none|ConsoleHost|native") != std::string::npos,
                      "native ConsoleHost configuration");
        }
    }
    bool atPrompt()
    {
        const auto text = row(s);
        if (cmd)
            return text == prompt;
        if (!text.starts_with(U"H3_PS[") || !text.ends_with(U"]>"))
            return false;
        uint32_t value = 0;
        for (size_t i = 6; i + 2 < text.size(); ++i)
        {
            if (text[i] < U'0' || text[i] > U'9' || value > 100000)
                return false;
            value = value * 10 + (text[i] - U'0');
        }
        generation = value;
        return value != 0;
    }
    void setCmdPrompt()
    {
        prompt.clear();
        for (unsigned char ch : token)
            prompt += ch;
        prompt += U'>';
        s.input("prompt " + token + "$G\r");
        check(s.until([&] { return atPrompt(); }), "unique CMD prompt barrier");
    }
    bool continueCommand(const std::wstring &out, int expectedExit = -1)
    {
        const auto path = out + L"\\continued.txt";
        const auto previous = generation;
        auto line = cmd ? L"echo CONTINUE>" + quote(path)
                        : L"[IO.File]::WriteAllText(" + psLiteral(path) + L",'CONTINUE')";
        if (expectedExit >= 0)
        {
            const auto status = out + L"\\child-exit.txt";
            line = (cmd ? L"echo %errorlevel%>" + quote(status) + L" & "
                        : L"[IO.File]::WriteAllText(" + psLiteral(status) +
                              L",$LASTEXITCODE.ToString()); ") +
                   line;
        }
        if (cmd)
        {
            token += "_" + std::to_string(++commandIndex);
            prompt.clear();
            for (unsigned char ch : token)
                prompt += ch;
            prompt += U'>';
            line += L" & prompt ";
            for (unsigned char ch : token)
                line += ch;
            line += L"$G";
        }
        s.input(utf8(line + L"\r"));
        return s.until([&] {
            return fileText(path) == "CONTINUE" && atPrompt() && (cmd || generation > previous) &&
                   (expectedExit < 0 ||
                    fileText(out + L"\\child-exit.txt") == std::to_string(expectedExit));
        });
    }
    void quit()
    {
        s.input("exit 0\r");
        check(s.until([&] { return WaitForSingleObject(s.descendant.value, 0) == WAIT_OBJECT_0; }),
              "shell exits");
        DWORD code = 1;
        check(GetExitCodeProcess(s.descendant.value, &code) && code == 0, "shell exit status");
    }
};
void shellPrompt(Context &context, char kind, bool partial)
{
    auto &s = context.start();
    Shell shell(context, kind);
    const auto bad = context.out + L"\\canceled-command.txt";
    if (partial)
    {
        const auto line = shell.cmd ? L"echo BAD>" + quote(bad)
                                    : L"[IO.File]::WriteAllText(" + psLiteral(bad) + L",'BAD')";
        s.input(utf8(line));
        check(s.until([&] { return !shell.atPrompt(); }), "partial command echoed");
    }
    const auto before = s.observe();
    const auto generation = shell.generation;
    const auto feeds = s.lineFeeds;
    writeFile(context.out + L"\\before.json", packetJson(before));
    s.input("\x03");
    const bool fresh = s.until(
        [&] {
            return shell.atPrompt() &&
                   (shell.cmd ? s.lineFeeds > feeds : shell.generation > generation);
        },
        3000);
    // This Enter is a post-cancellation assertion, not part of cancellation.
    // A retained line would execute the forbidden marker.
    s.input("\r");
    const bool continued = shell.continueCommand(context.out);
    const auto after = s.observe();
    writeFile(context.out + L"\\after.json", packetJson(after));
    const bool discarded = GetFileAttributesW(bad.c_str()) == INVALID_FILE_ATTRIBUTES;
    const bool alive = WaitForSingleObject(s.descendant.value, 0) == WAIT_TIMEOUT;
    const bool raw = (before.inputMode & ENABLE_PROCESSED_INPUT) == 0;
    writeFile(context.out + L"\\shell.json",
              "{\"Identity\":" + json(shell.metadata) + ",\"FreshPrompt\":" + boolean(fresh) +
                  ",\"CanceledCommandAbsent\":" + boolean(discarded) +
                  ",\"NextCommand\":" + boolean(continued) + ",\"Alive\":" + boolean(alive) +
                  ",\"RawEditor\":" + boolean(raw) + ",\"ETXWrites\":1,\"RetrySent\":false}");
    check(fresh && discarded && continued && alive, "shell cancellation and continuation");
    check(kind == 'L' ? raw && after.controlCount == 0 : !raw && after.controlCount == 1,
          "editor route and fixture control count");
    shell.quit();
}
struct Child
{
    Session &s;
    Handle pipe;
    DWORD pid = 0;
    Child(Context &context, Shell &shell) : s(*context.session)
    {
        const auto name = L"\\\\.\\pipe\\VT7Pty-H4-child-" +
                          std::to_wstring(GetCurrentProcessId()) + L"-" +
                          std::to_wstring(h3trace::qpc());
        pipe.reset(privateServer(name));
        const auto exe = imagePath(GetCurrentProcess());
        s.input(utf8((shell.cmd ? L"" : L"& ") + quote(exe) + L" --fixture " + quote(name) + L" " +
                     std::to_wstring(GetCurrentProcessId()) + L"\r"));
        connect(pipe.value);
        ULONG peer = 0;
        check(GetNamedPipeClientProcessId(pipe.value, &peer), "active child peer");
        pid = peer;
        s.guardChild.reset(OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid));
        check(s.guardChild.value && parentProcessId(pid) == GetProcessId(s.descendant.value) &&
                  _wcsicmp(imagePath(s.guardChild.value).c_str(), exe.c_str()) == 0 &&
                  agrees(hostQuery(s.guardChild.value), hostQuery(s.agent.value)),
              "retained active native child");
        const auto p = observe();
        check(std::wstring(p.fixtureDesktop) == applicationDesktop &&
                  windowOn(GetThreadDesktop(GetCurrentThreadId()),
                           reinterpret_cast<HWND>(p.fixtureGui)),
              "GUI-capable descendant stays ordinary");
        writeFile(context.out + L"\\child-identity.json", packetJson(p));
    }
    IdentityPacket read()
    {
        IdentityPacket p;
        transfer(pipe.value, &p, sizeof(p), false);
        check(p.magic == 0x33494856 && p.schema == 6 && p.processId == pid &&
                  p.parentId == GetProcessId(s.descendant.value) && p.textCount <= 127 &&
                  p.keyCount <= 16 && p.trace.eventCount <= 64 && p.guardInputCount <= 32 &&
                  (p.query.raw & ~ULONG_PTR{3}) == GetProcessId(s.host.value) &&
                  p.consoleCount == 4,
              "active child packet");
        return p;
    }
    IdentityPacket observe(char command = 'I')
    {
        transfer(pipe.value, &command, 1, true);
        return read();
    }
    void quit()
    {
        char q = 'Q';
        transfer(pipe.value, &q, 1, true);
        check(s.until([&] { return WaitForSingleObject(s.guardChild.value, 0) == WAIT_OBJECT_0; }),
              "active child quits");
    }
};
void activeChild(Context &context, char kind, bool cookedRead)
{
    auto &s = context.start();
    Shell shell(context, kind);
    Child child(context, shell);
    auto p = child.observe(cookedRead ? 'R' : 'J');
    check(!p.readReturned && !p.controlCount, "active child starts idle");
    if (cookedRead)
    {
        s.input("childpending");
        check(s.until([&] { return s.state.contains(U"HOST_INPUT>childpending"); }),
              "active child pending echo");
        p = child.observe();
        check(!p.readReturned && p.trace.readEntered, "active child pending read");
    }
    writeFile(context.out + L"\\child-pending.json", packetJson(p));
    s.input("\x03");
    if (cookedRead)
    {
        check(s.until(
                  [&] {
                      p = child.observe();
                      return p.readReturned && p.controlCount >= 1;
                  },
                  3000),
              "active child read cancels");
        writeFile(context.out + L"\\child-cancel.json", packetJson(p));
        check(canceled(p, 1), "active child one canceled read");
        child.observe('R');
        s.input("CHILD_NEXT\r");
        check(s.until([&] {
            p = child.observe();
            return p.readReturned;
        }),
              "active child next read");
        writeFile(context.out + L"\\child-next.json", packetJson(p));
        check(p.readSucceeded && !p.readError && p.controlCount == 1 &&
                  std::wstring(p.text, p.textCount) == L"CHILD_NEXT\r\n",
              "active child next line intact");
        child.quit();
    }
    else
    {
        p = child.read();
        writeFile(context.out + L"\\child-event.json", packetJson(p));
        check(p.controlCount == 1 && p.controlType == CTRL_C_EVENT && !p.readReturned,
              "event-only child one event");
        check(s.until([&] { return WaitForSingleObject(s.guardChild.value, 0) == WAIT_OBJECT_0; }),
              "event-only child exits itself");
        check(s.until([&] { return s.state.contains(U"GUARD_CHILD_FINAL"); }),
              "event-only child final output");
    }
    DWORD code = 0;
    check(GetExitCodeProcess(s.guardChild.value, &code) && code == 37, "active child exit 37");
    check(s.until([&] { return shell.atPrompt(); }, 10000), "parent prompt recovers");
    const bool continued = shell.continueCommand(context.out, 37);
    check(continued && WaitForSingleObject(s.descendant.value, 0) == WAIT_TIMEOUT,
          "parent reports 37 and continues");
    check(s.until([&] { return s.observe().controlCount == 1; }), "parent fixture one event");
    writeFile(context.out + L"\\parent.json", packetJson(s.observe()));
    writeFile(context.out + L"\\child.json", "{\"Identity\":" + json(shell.metadata) +
                                                 ",\"ChildExitCode\":37,\"ParentContinued\":true,"
                                                 "\"ParentAlive\":true,\"CookedReadRequired\":" +
                                                 boolean(cookedRead) +
                                                 ",\"ETXWrites\":1,\"RetrySent\":false}");
    shell.quit();
}
struct ActorPacket
{
    uint32_t magic = 0x34413456, agent = 0, child = 0, host = 0;
    uint64_t agentCreated = 0, childCreated = 0, hostCreated = 0, window = 0;
};
int lifetimeActor(const std::wstring &pipeName, DWORD server)
{
    Handle channel(CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
    ULONG peer = 0;
    check(channel.value != INVALID_HANDLE_VALUE &&
              GetNamedPipeServerProcessId(channel.value, &peer) && peer == server,
          "lifetime actor server");
    Session s(1, L"", VT7PTY_FLAG_HOST_CANCELLATION);
    startRead(s, "caller-death");
    const auto p = s.observe();
    ActorPacket packet;
    packet.agent = GetProcessId(s.agent.value);
    packet.child = GetProcessId(s.child.value);
    packet.host = GetProcessId(s.host.value);
    packet.agentCreated = creation(s.agent.value);
    packet.childCreated = creation(s.child.value);
    packet.hostCreated = creation(s.host.value);
    packet.window = p.window;
    transfer(channel.value, &packet, sizeof(packet), true);
    char command = 0;
    transfer(channel.value, &command, 1, false, 120000);
    check(command == 'Q', "normal lifetime actor quit");
    s.input("\r");
    check(s.until([&] { return s.observe().readReturned != 0; }),
          "normal enclosing-job read completion");
    s.quit();
    return 37;
}
void callerDeath(Context &context, bool both, bool jobControl = false)
{
    const auto name = L"\\\\.\\pipe\\VT7Pty-H4-actor-" + std::to_wstring(GetCurrentProcessId()) +
                      L"-" + std::to_wstring(h3trace::qpc());
    Handle channel(privateServer(name));
    const auto exe = imagePath(GetCurrentProcess());
    const auto command = quote(exe) + L" --lifetime-actor " + quote(name) + L" " +
                         std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> line(command.begin(), command.end());
    line.push_back(0);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION pi = {};
    Handle job;
    if (jobControl)
    {
        job.reset(CreateJobObjectW(nullptr, nullptr));
        check(job.value != nullptr, "enclosing job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        check(SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits,
                                      sizeof(limits)),
              "enclosing job lifetime");
    }
    check(CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, context.out.c_str(),
                         &startup, &pi),
          "owned lifetime actor");
    Handle actor(pi.hProcess), actorThread(pi.hThread), agent, child, host;
    bool cleaned = true;
    try
    {
        if (jobControl)
            check(AssignProcessToJobObject(job.value, actor.value), "actor enters enclosing job");
        check(ResumeThread(actorThread.value) != static_cast<DWORD>(-1), "start lifetime actor");
        connect(channel.value);
        ULONG peer = 0;
        check(GetNamedPipeClientProcessId(channel.value, &peer) && peer == pi.dwProcessId,
              "owned lifetime actor peer");
        ActorPacket packet;
        transfer(channel.value, &packet, sizeof(packet), false);
        check(packet.magic == 0x34413456 && parentProcessId(packet.agent) == pi.dwProcessId &&
                  parentProcessId(packet.child) == packet.agent,
              "lifetime actor chain");
        agent.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE,
                                FALSE, packet.agent));
        child.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE,
                                FALSE, packet.child));
        host.reset(
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, packet.host));
        check(agent.value && child.value && host.value &&
                  creation(agent.value) == packet.agentCreated &&
                  creation(child.value) == packet.childCreated &&
                  creation(host.value) == packet.hostCreated,
              "retained lifetime actor identities");
        check(_wcsicmp(imagePath(child.value).c_str(), exe.c_str()) == 0 &&
                  _wcsicmp(imagePath(agent.value).c_str(),
                           (directory() + L"\\VT7Pty-Agent.exe").c_str()) == 0 &&
                  agrees(hostQuery(agent.value), hostQuery(child.value)) &&
                  (hostQuery(agent.value).raw & ~ULONG_PTR{3}) == packet.host,
              "lifetime actor images/association");
        Desktop desktop;
        desktop.find(reinterpret_cast<HWND>(packet.window));
        if (jobControl)
        {
            for (HANDLE process : {actor.value, agent.value, child.value})
            {
                BOOL inJob = FALSE;
                check(IsProcessInJob(process, job.value, &inJob) && inJob,
                      "runtime preserves enclosing job");
            }
            char q = 'Q';
            transfer(channel.value, &q, 1, true);
        }
        else
        {
            std::thread other;
            if (both)
                other = std::thread([&] { TerminateProcess(agent.value, 123); });
            const BOOL stopped = TerminateProcess(actor.value, 123);
            if (other.joinable())
                other.join();
            check(stopped != FALSE, "terminate only created caller");
        }
        for (HANDLE process : {actor.value, agent.value, child.value, host.value})
            check(WaitForSingleObject(process, 10000) == WAIT_OBJECT_0,
                  "caller/agent death and observed host cleanup");
        if (jobControl)
        {
            DWORD code = 0;
            check(GetExitCodeProcess(actor.value, &code) && code == 37,
                  "normal enclosing-job actor status");
        }
        writeFile(context.out + L"\\caller.json",
                  "{\"Agent\":" + std::to_string(packet.agent) + ",\"Child\":" +
                      std::to_string(packet.child) + ",\"Host\":" + std::to_string(packet.host) +
                      ",\"SimultaneousAgentDeath\":" + boolean(both) +
                      ",\"EnclosingJobPreserved\":" + boolean(jobControl) +
                      ",\"AllOwnedProcessesExited\":true,\"HostExitedWithoutTermination\":true}");
    }
    catch (...)
    {
        for (HANDLE process : {child.value, agent.value, actor.value})
            if (process && process != INVALID_HANDLE_VALUE &&
                WaitForSingleObject(process, 0) != WAIT_OBJECT_0)
            {
                cleaned = false;
                TerminateProcess(process, 125);
                WaitForSingleObject(process, 5000);
            }
        context.cleaned = cleaned;
        throw;
    }
}
void impersonationDenied(Context &context)
{
    check(ImpersonateSelf(SecurityImpersonation), "controlled thread impersonation");
    vt7pty_error_ptr_t error = nullptr;
    auto *config = vt7pty_config_new(VT7PTY_FLAG_HOST_CANCELLATION, nullptr);
    vt7pty_t *session = vt7pty_open(config, &error);
    const auto message = error ? utf8(vt7pty_error_msg(error)) : "";
    const auto code = vt7pty_error_code(error);
    check(RevertToSelf(), "end controlled impersonation");
    vt7pty_config_free(config);
    vt7pty_error_free(error);
    if (session)
        vt7pty_free(session);
    writeFile(context.out + L"\\denial.json", "{\"Denied\":" + boolean(!session) +
                                                  ",\"ErrorCode\":" + std::to_string(code) +
                                                  ",\"Message\":" + json(message) + "}");
    check(!session && message.find("unimpersonated") != std::string::npos,
          "actual impersonated caller fails closed");
}

int startupCheck(const std::wstring &out)
{
    auto *config = vt7pty_config_new(VT7PTY_FLAG_HOST_CANCELLATION, nullptr);
    vt7pty_config_set_agent_timeout(config, 5000);
    vt7pty_error_ptr_t error = nullptr;
    auto *failed = vt7pty_open(config, &error);
    const auto code = vt7pty_error_code(error);
    const auto message = error ? utf8(vt7pty_error_msg(error)) : "";
    vt7pty_config_free(config);
    vt7pty_error_free(error);
    if (failed)
        vt7pty_free(failed);
    check(!failed && code != 0, "missing or wrong helper fails startup");
    config = vt7pty_config_new(0, nullptr);
    auto *plain = vt7pty_open(config, nullptr);
    vt7pty_config_free(config);
    check(plain != nullptr, "flagless session does not require host module");
    Handle agent;
    check(DuplicateHandle(GetCurrentProcess(), vt7pty_agent_process(plain), GetCurrentProcess(),
                          &agent.value, 0, FALSE, DUPLICATE_SAME_ACCESS),
          "flagless agent handle");
    const auto query = hostQuery(agent.value);
    check(valid(query), "flagless host");
    Handle host(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                            static_cast<DWORD>(query.raw & ~ULONG_PTR{3})));
    check(host.value != nullptr, "flagless host handle");
    check(vt7pty_close(plain, nullptr), "close flagless session");
    vt7pty_free(plain);
    check(WaitForSingleObject(agent.value, 10000) == WAIT_OBJECT_0 &&
              WaitForSingleObject(host.value, 10000) == WAIT_OBJECT_0,
          "flagless shutdown");
    writeFile(
        out + L"\\startup.json",
        "{\"OptInDenied\":true,\"ErrorCode\":" + std::to_string(code) +
            ",\"Message\":" + json(message) +
            ",\"FlaglessOpenPassed\":true,\"OwnedAgentExited\":true,\"ObservedHostExited\":true}");
    return 0;
}
void helperFailure(Context &context, bool wrong)
{
    const auto bin = context.out + L"\\isolated";
    check(CreateDirectoryW(bin.c_str(), nullptr), "isolated missing-helper directory");
    for (const wchar_t *name : {L"RuntimeCancellationTest.exe", L"VT7Pty.dll", L"VT7Pty-Agent.exe"})
        check(CopyFileW((directory() + L"\\" + name).c_str(), (bin + L"\\" + name).c_str(), TRUE),
              "copy matched startup controls");
    if (wrong)
        check(CopyFileW((directory() + L"\\VT7Pty.dll").c_str(),
                        (bin + L"\\VT7Pty-Host.dll").c_str(), TRUE),
              "real DLL with wrong export surface");
    const auto exe = bin + L"\\RuntimeCancellationTest.exe";
    const auto command = quote(exe) + L" --startup-check " + quote(context.out);
    std::vector<wchar_t> line(command.begin(), command.end());
    line.push_back(0);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION pi = {};
    check(CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, context.out.c_str(), &startup, &pi),
          "owned startup actor");
    Handle process(pi.hProcess), thread(pi.hThread);
    if (WaitForSingleObject(process.value, 20000) != WAIT_OBJECT_0)
    {
        TerminateProcess(process.value, 125);
        WaitForSingleObject(process.value, 5000);
        throw std::runtime_error("startup actor deadline");
    }
    DWORD code = 1;
    check(GetExitCodeProcess(process.value, &code) && code == 0,
          "startup failure and flagless control");
    // Keep raw startup observations, not redundant copies of the package binaries.
    for (const wchar_t *name :
         {L"RuntimeCancellationTest.exe", L"VT7Pty.dll", L"VT7Pty-Agent.exe", L"VT7Pty-Host.dll"})
    {
        const auto path = bin + L"\\" + name;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            check(DeleteFileW(path.c_str()), "remove isolated copied test binary");
    }
    check(RemoveDirectoryW(bin.c_str()), "remove empty isolated test directory");
}
struct Collector
{
    Handle process, thread, discard, error, job;
    bool stopped = false;
    explicit Collector(const std::wstring &out)
    {
        job.reset(CreateJobObjectW(nullptr, nullptr));
        check(job.value != nullptr, "collector containment job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        check(SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits,
                                      sizeof(limits)),
              "collector containment lifetime");
        SECURITY_ATTRIBUTES attributes = {sizeof(attributes), nullptr, TRUE};
        discard.reset(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &attributes, OPEN_EXISTING, 0, nullptr));
        error.reset(CreateFileW((out + L"\\collector-errors.txt").c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ, &attributes, CREATE_NEW, 0, nullptr));
        check(discard.value != INVALID_HANDLE_VALUE && error.value != INVALID_HANDLE_VALUE,
              "collector output handles");
        const auto exe = directory() + L"\\VT7Pty-DebugServer.exe";
        const auto command = quote(exe) + L" --output " + quote(out + L"\\runtime-trace.jsonl") +
                             L" --max-bytes 16777216";
        std::vector<wchar_t> line(command.begin(), command.end());
        line.push_back(0);
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = discard.value;
        startup.hStdOutput = discard.value;
        startup.hStdError = error.value;
        PROCESS_INFORMATION pi = {};
        check(CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, directory().c_str(),
                             &startup, &pi),
              "owned runtime trace collector");
        process.reset(pi.hProcess);
        thread.reset(pi.hThread);
        if (!AssignProcessToJobObject(job.value, process.value))
        {
            TerminateProcess(process.value, 125);
            WaitForSingleObject(process.value, 5000);
            throw std::runtime_error("collector job assignment");
        }
        check(ResumeThread(thread.value) != static_cast<DWORD>(-1), "start owned collector");
        if (WaitForSingleObject(process.value, 300) != WAIT_TIMEOUT)
            throw std::runtime_error(
                "runtime collector unavailable; close an existing debug collector before testing");
    }
    void stop() noexcept
    {
        if (stopped)
            return;
        stopped = true;
        if (process.value != INVALID_HANDLE_VALUE &&
            WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT)
        {
            Sleep(100);
            TerminateProcess(process.value, 0);
            WaitForSingleObject(process.value, 5000);
        }
        error.reset();
        discard.reset();
    }
    ~Collector()
    {
        stop();
    }
};
