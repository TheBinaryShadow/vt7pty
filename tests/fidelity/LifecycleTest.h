// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

// Included by FidelityTest.cc to share its session, stream and fixture readers.
using LifecycleClock = std::chrono::steady_clock;

int64_t lifecycleElapsed(LifecycleClock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(LifecycleClock::now() - start).count();
}

std::string lifecycleRecipes() {
    std::string result = "{\"SchemaVersion\":1,\"SnapshotBytes\":" + std::to_string(sizeof(Packet)) +
        ",\"OutputRows\":" + std::to_string(lifecycleRows) + ",\"Samples\":" + std::to_string(lifecycleSamples) +
        ",\"IdleMilliseconds\":" + std::to_string(lifecycleIdleMs) +
        ",\"ShutdownMilliseconds\":" + std::to_string(lifecycleShutdownMs) + ",\"Cases\":[";
    for (const auto &test : lifecycleCases) {
        if (&test != lifecycleCases) result += ',';
        result += "{\"Name\":" + json(utf8(test.name)) +
            ",\"Area\":" + json(test.mode == LifecycleMode::Measure ? "F09" : "F08") + '}';
    }
    return result + "]}";
}

std::string lifecycleJobContext(const Session &session) {
    const auto membership = [](HANDLE process) {
        BOOL member = FALSE;
        check(IsProcessInJob(process, nullptr, &member),"lifecycle job membership");
        return member ? "true" : "false";
    };
    return std::string("{\"Caller\":") + membership(GetCurrentProcess()) +
        ",\"Agent\":" + membership(session.agent.value) + ",\"Child\":" + membership(session.child.value) + '}';
}

void lifecycleDrain(LifecycleMode mode,Session &session,Observations &observations) {
    const bool powershell = mode == LifecycleMode::PowerShellDrain;
    const bool missingTail = mode == LifecycleMode::MissingTail;
    const auto exe = systemProgram(powershell ? L"WindowsPowerShell\\v1.0\\powershell.exe" : L"cmd.exe");
    const auto rows = std::to_wstring(lifecycleRows);
    const auto status = std::to_wstring(lifecycleExitCode);
    const auto command = powershell ? quote(exe) + L" -NoLogo -NoProfile -NonInteractive -Command " +
        quote(L"for ($i=1; $i -le " + rows + L"; $i++) {[Console]::WriteLine('VT7ROW_'+$i+'_END')}; "
            L"[Console]::WriteLine('VT7DRAIN_END'); exit " + status) :
        quote(exe) + L" /D /Q /C \"(for /L %i in (1,1," + rows + L") do @echo VT7ROW_%i_END)& " +
            (missingTail ? L"" : L"echo VT7DRAIN_END& ") + L"exit /b " + status + L"\"";
    session.replyToQueries = false;
    DWORD win32 = 0;
    vt7pty_result_t error = VT7PTY_ERROR_SUCCESS;
    check(session.trySpawn(exe,command,VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN|VT7PTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN,
        win32,error),"start lifecycle drain workload");
    const bool exited = WaitForSingleObject(session.child.value, lifecycleShutdownMs) == WAIT_OBJECT_0;
    DWORD exitCode = STILL_ACTIVE;
    check(GetExitCodeProcess(session.child.value,&exitCode),"lifecycle workload status");
    const bool drained = session.until([&] { return session.outputEof(); }, lifecycleShutdownMs);
    const bool agentExited = WaitForSingleObject(session.agent.value, lifecycleShutdownMs) == WAIT_OBJECT_0;
    observations.add("ChildExitedBeforeReader",exited);
    observations.add("OutputEof",drained);
    observations.add("AgentExitedAfterDrain",agentExited);
    observations.fields += ",\"ChildExitCode\":" + std::to_string(exitCode);

    size_t position = 0;
    int matched = 0;
    for (int i=1;i<=lifecycleRows;++i) {
        const std::string token = "VT7ROW_" + std::to_string(i) + "_END";
        const size_t next = session.raw.find("VT7ROW_",position);
        if (next == std::string::npos || session.raw.compare(next,token.size(),token) != 0) break;
        position = next + token.size();
        ++matched;
    }
    const bool ordered = matched == lifecycleRows && session.raw.find("VT7ROW_",position) == std::string::npos;
    const auto tail = session.raw.find("VT7DRAIN_END");
    const bool final = tail != std::string::npos && tail >= position &&
        session.raw.find("VT7DRAIN_END",tail+1) == std::string::npos;
    observations.add("RowsMatched",ordered);
    observations.add("FinalMarkerMatched",final);
    if (!exited || !drained || !agentExited || exitCode != lifecycleExitCode || !ordered || !final) {
        if (missingTail && exited && drained && agentExited && exitCode == lifecycleExitCode && ordered && !final)
            throw Mismatch("final marker missing from drained output");
        throw Mismatch("delayed-reader drain, status or agent exit differs");
    }
}

void lifecycleRecover(const std::wstring &resultDirectory,Session &session,Observations &observations) {
    const auto missing = resultDirectory + L"\\absent-lifecycle-image.exe";
    check(GetFileAttributesW(missing.c_str()) == INVALID_FILE_ATTRIBUTES,"missing executable control exists");
    DWORD win32 = 0;
    vt7pty_result_t api = VT7PTY_ERROR_SUCCESS;
    const bool started = session.trySpawn(missing,quote(missing),VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN,win32,api);
    observations.add("UnexpectedChildHandle",session.child.value != nullptr);
    observations.add("UnexpectedStart",started);
    observations.fields += ",\"SpawnApiError\":" + std::to_string(api) +
        ",\"SpawnWin32Error\":" + std::to_string(win32);
    if (started || session.child.value != nullptr || api != VT7PTY_ERROR_SPAWN_CREATE_PROCESS_FAILED ||
        win32 != ERROR_FILE_NOT_FOUND) throw Mismatch("incorrect missing executable result");
    session.replyToQueries = false;
    const auto exe = systemProgram(L"cmd.exe");
    session.spawn(exe,quote(exe) + L" /D /Q /C \"echo VT7_RECOVERED& exit /b 37\"");
    session.finish(lifecycleExitCode);
    const bool recovered = session.raw.find("VT7_RECOVERED") != std::string::npos;
    observations.add("Recovered",recovered);
    if (!recovered) throw Mismatch("replacement application output missing");
}

class LifecycleFixture {
public:
    LifecycleFixture(Session &session,std::wstring_view mode,std::wstring_view label) {
        const auto name = L"\\\\.\\pipe\\VT7Pty.Lifecycle." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::wstring(label);
        pipe_.reset(privateServer(name));
        const auto exe = directory() + L"\\FidelityFixture.exe";
        session.spawn(exe, quote(exe) + L" " + std::wstring(mode) + L" " + quote(name) + L" " +
            std::to_wstring(GetCurrentProcessId()));
        connect(pipe_.value);
        ULONG client = 0;
        check(GetNamedPipeClientProcessId(pipe_.value,&client) && client == GetProcessId(session.child.value),
            "lifecycle fixture identity");
        receive();
    }

    const Packet &snapshot(char command) {
        send(command);
        return receive();
    }

    void send(char command) { transfer(pipe_.value,&command,1,true); }

private:
    const Packet &receive() {
        transfer(pipe_.value,packet_.get(),sizeof(Packet),false);
        validate(*packet_);
        return *packet_;
    }
    Handle pipe_;
    std::unique_ptr<Packet> packet_ = std::make_unique<Packet>();
};

void lifecycleClose(Session &session,Observations &observations) {
    Session neighbor;
    try {
        LifecycleFixture selected(session,L"STATE",L"close");
        LifecycleFixture survivor(neighbor,L"STATE",L"neighbor");
        observations.fields += ",\"JobMembership\":" + lifecycleJobContext(session);
        const auto module = GetModuleHandleW(L"VT7Pty.dll");
        check(module != nullptr,"loaded VT7Pty module");
        const auto close = std::bit_cast<decltype(&vt7pty_close)>(GetProcAddress(module,"vt7pty_close"));
        const auto begin = LifecycleClock::now();
        bool closeContract = true;
        if (close) {
            const BOOL retired = close(session.runtimeHandle(),nullptr);
            const BOOL repeated = close(session.runtimeHandle(),nullptr);
            vt7pty_error_ptr_t error = nullptr;
            const BOOL resized = vt7pty_set_size(session.runtimeHandle(),100,30,&error);
            closeContract = retired && repeated && !resized && error &&
                vt7pty_error_code(error) == VT7PTY_ERROR_LOST_CONNECTION;
            if (error) vt7pty_error_free(error);
        }
        const bool stopped = session.cleanup();
        const auto shutdownUs = lifecycleElapsed(begin);
        const auto &packet = survivor.snapshot('S');
        const bool neighborAlive = WaitForSingleObject(neighbor.child.value,0) == WAIT_TIMEOUT &&
            packet.columns == 80 && packet.rows == 25 &&
            neighbor.until([&] { return neighbor.state.contains(U"STATE_READY"); });
        survivor.send('Q');
        neighbor.finish();
        const bool neighborDrain = neighbor.state.contains(U"FINAL_DRAIN");
        observations.raw = neighbor.raw;
        observations.add("ExplicitCloseAvailable",close != nullptr);
        observations.add("CloseContract",closeContract);
        observations.add("SelectedExited",stopped);
        observations.add("NeighborAlive",neighborAlive);
        observations.add("NeighborFinalDrain",neighborDrain);
        observations.fields += ",\"ShutdownMicroseconds\":" + std::to_string(shutdownUs);
        observations.cleanup = stopped && neighbor.cleanup();
        if (!closeContract || !stopped || !neighborAlive || !neighborDrain ||
            shutdownUs > lifecycleShutdownMs * 1000ll) throw Mismatch("close or neighboring session differs");
    } catch (...) {
        if (observations.raw.empty()) observations.raw = neighbor.raw;
        const bool neighborCleaned = neighbor.cleanup();
        observations.cleanup = observations.cleanup && neighborCleaned;
        throw;
    }
}

uint64_t fileTimeValue(FILETIME value) {
    return (static_cast<uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

std::string processResources(HANDLE process) {
    FILETIME creation={}, exit={}, kernel={}, user={};
    DWORD handles = 0;
    PROCESS_MEMORY_COUNTERS_EX memory={};
    check(GetProcessTimes(process,&creation,&exit,&kernel,&user),"lifecycle CPU sample");
    check(GetProcessHandleCount(process,&handles),"lifecycle handle sample");
    check(K32GetProcessMemoryInfo(process,reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory)),
        "lifecycle memory sample");
    return "{\"Cpu100ns\":" + std::to_string(fileTimeValue(kernel) + fileTimeValue(user)) +
        ",\"CreationTime\":" + std::to_string(fileTimeValue(creation)) + ",\"Handles\":" + std::to_string(handles) +
        ",\"PrivateBytes\":" + std::to_string(memory.PrivateUsage) +
        ",\"WorkingSetBytes\":" + std::to_string(memory.WorkingSetSize) + '}';
}

std::string lifecycleResources(const Session &session) {
    return "{\"Caller\":" + processResources(GetCurrentProcess()) +
        ",\"Agent\":" + processResources(session.agent.value) +
        ",\"Child\":" + processResources(session.child.value) + '}';
}

void lifecycleMeasure(const std::wstring &resultDirectory,Session &session,Observations &observations) {
    LifecycleFixture fixture(session,L"LIFECYCLE_MEASURE",L"measure");
    if (!session.until([&] { return session.state.contains(U"STATE_READY"); }))
        throw Mismatch("lifecycle measurement fixture did not become ready");
    observations.fields += ",\"JobMembership\":" + lifecycleJobContext(session);
    const auto idleBefore = lifecycleResources(session);
    const auto idleStart = LifecycleClock::now();
    session.until([&] { return lifecycleElapsed(idleStart) >= lifecycleIdleMs * 1000ll; },lifecycleIdleMs+2000);
    const auto idleUs = lifecycleElapsed(idleStart);
    const auto idleAfter = lifecycleResources(session);
    const auto activeStart = LifecycleClock::now();
    std::string samples;
    for (int index=0;index<lifecycleSamples;++index) {
        const char key = static_cast<char>('a' + index);
        const auto inputStart = LifecycleClock::now();
        session.input(std::string_view(&key,1));
        if (!session.until([&] { return fixture.snapshot('S').keyCount > static_cast<uint32_t>(index); }))
            throw Mismatch("input receipt deadline");
        const auto inputUs = lifecycleElapsed(inputStart);
        const int columns = index % 2 == 0 ? 100 : 80;
        const int rows = index % 2 == 0 ? 30 : 25;
        const auto resizeStart = LifecycleClock::now();
        session.resize(columns,rows);
        const auto resizeUs = lifecycleElapsed(resizeStart);
        const auto barrierStart = LifecycleClock::now();
        const auto &packet = fixture.snapshot('B');
        const auto ascii = utf8(lifecycleBarrierMarker(index+1));
        const std::u32string marker(ascii.begin(),ascii.end());
        const bool barrier = session.until([&] { return session.state.contains(marker); });
        const auto barrierUs = lifecycleElapsed(barrierStart);
        const auto sequence = std::to_wstring(index+1);
        writeFile(resultDirectory + L"\\console-" + sequence + L".bin",
            std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
        if (packet.keyCount != static_cast<uint32_t>(index+1) ||
            packet.keys[index].character != static_cast<uint32_t>(key) || packet.keys[index].repeat != 1)
            throw Mismatch("input sample differs from expected key or repeat count");
        if (packet.columns != static_cast<uint32_t>(columns) || packet.rows != static_cast<uint32_t>(rows) ||
            packet.bufferColumns != packet.columns || !barrier)
            throw Mismatch("measurement resize or VT barrier differs");
        if (!samples.empty()) samples += ',';
        samples += "{\"InputUs\":" + std::to_string(inputUs) + ",\"ResizeUs\":" + std::to_string(resizeUs) +
            ",\"BarrierUs\":" + std::to_string(barrierUs) + ",\"CapturedBytes\":" + std::to_string(session.raw.size()) + '}';
    }
    const auto activeUs = lifecycleElapsed(activeStart);
    const auto activeAfter = lifecycleResources(session);
    observations.fields += ",\"Measurements\":[" + samples + "],\"IdleUs\":" + std::to_string(idleUs) +
        ",\"ActiveUs\":" + std::to_string(activeUs) + ",\"IdleBefore\":" + idleBefore +
        ",\"IdleAfter\":" + idleAfter + ",\"ActiveAfter\":" + activeAfter;
    fixture.send('Q');
    session.finish();
    const bool final = session.state.contains(U"FINAL_DRAIN");
    observations.add("FinalMarkerMatched",final);
    if (!final) throw Mismatch("measurement final marker missing");
}

void lifecycleCase(const LifecycleCase &test,const std::wstring &resultDirectory,Observations &observations) {
    Session session;
    try {
        switch (test.mode) {
        case LifecycleMode::CmdDrain:
        case LifecycleMode::PowerShellDrain:
        case LifecycleMode::MissingTail:
            lifecycleDrain(test.mode,session,observations);
            break;
        case LifecycleMode::Recover:
            lifecycleRecover(resultDirectory,session,observations);
            break;
        case LifecycleMode::Close:
            lifecycleClose(session,observations);
            break;
        case LifecycleMode::Measure:
            lifecycleMeasure(resultDirectory,session,observations);
            break;
        }
    } catch (...) {
        if (observations.raw.empty()) observations.raw = session.raw;
        const bool cleaned = session.cleanup();
        observations.cleanup = observations.cleanup && cleaned;
        throw;
    }
    if (observations.raw.empty()) observations.raw = session.raw;
    const bool cleaned = session.cleanup();
    observations.cleanup = observations.cleanup && cleaned;
    if (!observations.cleanup) throw Mismatch("owned process exceeded cleanup deadline");
}
