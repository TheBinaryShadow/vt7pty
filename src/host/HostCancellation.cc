// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "../shared/DebugClient.h"
#include "../shared/HostCancellationProtocol.h"
#include "../shared/HostIdentity.h"

namespace
{
using namespace vt7pty::internal;

HMODULE module = nullptr;
HANDLE pipe = INVALID_HANDLE_VALUE, owner = nullptr, boundary = nullptr;
cancel::Request binding;
uint64_t sequence = 0;
ATOM atom = 0;
HWND target = nullptr;
bool bound = false, retired = false;
volatile LONG entered = 0;

// One immutable context per owned host. A dedicated thread owns desktop
// assignment and serializes close requests with caller/agent lifetime signals.
// Nothing here dispatches C or touches keyboard state. The pinned module and
// process-owned handles remain valid until this conhost exits, even if the
// request channel has already retired.
struct Lifetime
{
    HANDLE agent = nullptr, caller = nullptr, request = nullptr, thread = nullptr;
    HDESK desktop = nullptr;
    HWND window = nullptr;
    volatile LONG result = 0; // 0 waiting, 1 WM_CLOSE posted, -1 delivery failed.
} lifetime;

DWORD WINAPI lifetimeEnded(void *) noexcept
{
    const auto fail = [](const char *stage) {
        const DWORD error = GetLastError();
        InterlockedExchange(&lifetime.result, -1);
        TRACE_EVENT(TraceSeverity::Error, TraceSubsystem::Process,
                    "host-lifetime failure stage=%s error=%u", stage, error);
        return DWORD{1};
    };
    // This is our own windowless thread, never a shared thread-pool worker.
    // Its desktop assignment ends with the thread, so no restoration can
    // contaminate an unrelated pool callback.
    if (!SetThreadDesktop(lifetime.desktop))
        return fail("desktop");
    const HANDLE objects[] = {lifetime.request, lifetime.agent, lifetime.caller};
    const DWORD result = WaitForMultipleObjects(3, objects, FALSE, INFINITE);
    if (result >= WAIT_OBJECT_0 + 3)
        return fail("wait");
    if (!PostMessageW(lifetime.window, WM_CLOSE, 0, 0))
        return fail("post-close");
    InterlockedExchange(&lifetime.result, 1);
    TRACE_EVENT(TraceSeverity::Info, TraceSubsystem::Process,
                "host-lifetime close posted trigger=%u", result - WAIT_OBJECT_0);
    return 0;
}

bool requestConsoleClose() noexcept
{
    // Accepted means queued, not that clients have exited. The same event
    // absorbs simultaneous requests; only the lifetime thread posts WM_CLOSE.
    if (InterlockedCompareExchange(&lifetime.result, 0, 0) < 0)
        return false;
    return lifetime.request && SetEvent(lifetime.request) != FALSE;
}

void watchLifetime(HANDLE caller)
{
    HMODULE pinned = nullptr;
    host::require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,
                                     host::modulePath(module).c_str(), &pinned) != FALSE,
                  L"Host lifetime module pin");
    const auto name = host::objectName(GetThreadDesktop(GetCurrentThreadId()));
    HDESK desktop = OpenDesktopW(name.c_str(), 0, FALSE,
                                 DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS);
    host::require(desktop != nullptr, L"Host lifetime desktop");
    HANDLE agentRaw = nullptr, callerRaw = nullptr;
    OwnedHandle request;
    if (!DuplicateHandle(GetCurrentProcess(), owner, GetCurrentProcess(), &agentRaw, SYNCHRONIZE,
                         FALSE, 0) ||
        !DuplicateHandle(GetCurrentProcess(), caller, GetCurrentProcess(), &callerRaw, SYNCHRONIZE,
                         FALSE, 0))
    {
        if (agentRaw)
            CloseHandle(agentRaw);
        if (callerRaw)
            CloseHandle(callerRaw);
        CloseDesktop(desktop);
        throwWindowsError(L"Host lifetime handles");
    }
    OwnedHandle agentCopy(agentRaw), callerCopy(callerRaw);
    request.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!request)
    {
        CloseDesktop(desktop);
        throwWindowsError(L"Host lifetime close event");
    }
    lifetime.agent = agentCopy.get();
    lifetime.caller = callerCopy.get();
    lifetime.request = request.get();
    lifetime.desktop = desktop;
    lifetime.window = target;
    lifetime.thread = CreateThread(nullptr, 0, lifetimeEnded, nullptr, 0, nullptr);
    if (!lifetime.thread)
    {
        lifetime.agent = lifetime.caller = lifetime.request = nullptr;
        lifetime.desktop = nullptr;
        CloseDesktop(desktop);
        throwWindowsError(L"Host lifetime thread");
    }
    // The worker may run immediately. Only host process exit releases these
    // now, never request retirement or UnhookWindowsHookEx.
    agentCopy.release();
    callerCopy.release();
    request.release();
}

void retire() noexcept
{
    for (HANDLE handle : {pipe, owner, boundary})
        if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    pipe = INVALID_HANDLE_VALUE;
    owner = boundary = nullptr;
    retired = true;
}

void reply(cancel::Result result, const cancel::Request *request = nullptr,
           const cancel::Keyboard *keys = nullptr)
{
    cancel::Response response;
    response.result = result;
    response.nonce[0] = binding.nonce[0];
    response.nonce[1] = binding.nonce[1];
    response.acceptedSequence = sequence;
    response.identity = binding.identity;
    response.identity.host = GetCurrentProcessId();
    response.identity.hostThread = GetCurrentThreadId();
    response.identity.hostCreated = host::creation(GetCurrentProcess());
    response.identity.window = reinterpret_cast<uintptr_t>(target);
    if (request)
        response.sequence = request->sequence;
    if (keys)
        response.keyboard = *keys;
    DWORD written = 0;
    if (!WriteFile(pipe, &response, sizeof(response), &written, nullptr) ||
        written != sizeof(response))
        retire();
}

void open(const CWPSTRUCT &message)
{
    if (message.wParam < 0xc000 || message.wParam > 0xffff || message.lParam <= 0 ||
        static_cast<uint64_t>(message.lParam) > MAXDWORD)
        return;
    wchar_t name[256] = {};
    if (!GlobalGetAtomNameW(static_cast<ATOM>(message.wParam), name, 256))
        return;
    const DWORD pid = static_cast<DWORD>(message.lParam);
    const auto prefix = L"VT7Pty-H4-" + std::to_wstring(pid) + L"-";
    const std::wstring value(name);
    if (value.size() != prefix.size() + 32 || value.compare(0, prefix.size(), prefix) != 0 ||
        value.find_first_not_of(L"0123456789abcdef", prefix.size()) != std::wstring::npos)
        return;
    OwnedHandle candidate(CreateFileW((L"\\\\.\\pipe\\" + value).c_str(),
                                      GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                                      nullptr));
    if (!candidate)
        return;
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    host::require(SetNamedPipeHandleState(candidate.get(), &mode, nullptr, nullptr) != FALSE,
                  L"Nonblocking host channel");
    ULONG server = 0;
    host::require(GetNamedPipeServerProcessId(candidate.get(), &server) && server == pid,
                  L"Host channel owner");
    OwnedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
    host::require(static_cast<bool>(process), L"Host controller open");
    host::require(_wcsicmp(host::image(process.get()).c_str(),
                           (host::directory(module) + L"\\VT7Pty-Agent.exe").c_str()) == 0,
                  L"Exact colocated host controller");
    const host::Token ours(GetCurrentProcess()), theirs(process.get());
    host::require(ours.same(theirs) && ours.integrity == SECURITY_MANDATORY_MEDIUM_RID &&
                      host::privateDesktop(),
                  L"Host controller token and isolated desktop");
    OwnedHandle completion(
        OpenEventW(EVENT_MODIFY_STATE, FALSE, (L"Local\\" + value + L"-completion").c_str()));
    host::require(static_cast<bool>(completion), L"Host completion event");
    binding.nonce[0] = _wcstoui64(value.substr(prefix.size(), 16).c_str(), nullptr, 16);
    binding.nonce[1] = _wcstoui64(value.substr(prefix.size() + 16, 16).c_str(), nullptr, 16);
    binding.identity.agent = pid;
    binding.identity.agentCreated = host::creation(process.get());
    pipe = candidate.release();
    owner = process.release();
    boundary = completion.release();
    target = message.hwnd;
    atom = static_cast<ATOM>(message.wParam);
    reply(cancel::Result::Hello);
}

bool bind(const cancel::Request &request)
{
    const auto &id = request.identity;
    if (!cancel::frame(request, GetTickCount64()) || request.operation != cancel::Operation::Bind ||
        request.sequence != 1 || request.nonce[0] != binding.nonce[0] ||
        request.nonce[1] != binding.nonce[1] || id.agent != binding.identity.agent ||
        id.agentCreated != binding.identity.agentCreated || id.host != GetCurrentProcessId() ||
        id.hostThread != GetCurrentThreadId() ||
        id.hostCreated != host::creation(GetCurrentProcess()) ||
        id.window != reinterpret_cast<uintptr_t>(target))
        return false;
    host::live(owner, id.agent, id.agentCreated);
    host::require(host::consoleHost(owner) == GetCurrentProcessId(),
                  L"Controller console association");
    OwnedHandle thread(
        OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id.agentThread));
    host::require(thread && GetProcessIdOfThread(thread.get()) == id.agent, L"Controller thread");
    host::require(host::objectName(GetThreadDesktop(id.agentThread)) ==
                      host::objectName(GetThreadDesktop(GetCurrentThreadId())),
                  L"Controller shares isolated backing desktop");
    DWORD pid = 0;
    const DWORD tid = GetWindowThreadProcessId(target, &pid);
    host::require(pid == GetCurrentProcessId() && tid == GetCurrentThreadId(),
                  L"Actual host window dispatch");
    OwnedHandle caller(
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id.caller));
    host::require(static_cast<bool>(caller), L"Session caller open");
    host::live(caller.get(), id.caller, id.callerCreated);
    const host::Token callerToken(caller.get()), agentToken(owner);
    host::require(callerToken.same(agentToken) && id.callerCreated <= id.agentCreated &&
                      host::parent(id.agent) == id.caller,
                  L"Session caller chain");
    watchLifetime(caller.get());
    binding = request;
    sequence = 1;
    bound = true;
    return true;
}

struct Keys
{
    cancel::Keyboard &evidence;
    bool attempted = false;
    explicit Keys(cancel::Keyboard &value) : evidence(value)
    {
        evidence.captured = GetKeyboardState(evidence.before) != FALSE;
        host::require(evidence.captured != 0, L"Host keyboard capture");
    }
    ~Keys()
    {
        if (!attempted)
            return;
        evidence.restored = SetKeyboardState(evidence.before) != FALSE;
        if (!evidence.restored)
            evidence.error = GetLastError();
        evidence.equal =
            GetKeyboardState(evidence.after) && memcmp(evidence.before, evidence.after, 256) == 0;
    }
    void apply()
    {
        BYTE state[256];
        memcpy(state, evidence.before, 256);
        for (unsigned int vk : {VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL,
                                VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN})
            state[vk] &= 1;
        state[VK_CONTROL] |= 0x80;
        state[VK_LCONTROL] |= 0x80;
        attempted = true; // Restore after any attempted write, including a failed call.
        evidence.applied = SetKeyboardState(state) != FALSE;
        host::require(evidence.applied != 0, L"Host temporary Ctrl state");
        host::require(GetKeyboardState(evidence.during) &&
                          memcmp(state, evidence.during, 256) == 0 &&
                          (GetKeyState(VK_CONTROL) & 0x8000),
                      L"Host Ctrl state verified");
    }
};

cancel::Keyboard cancelRead()
{
    cancel::Keyboard evidence;
    try
    {
        Keys scope(evidence);
        scope.apply();
        const UINT scan = MapVirtualKeyW('C', MAPVK_VK_TO_VSC);
        host::require(scan > 0 && scan < 256, L"Host C scan code");
        SendMessageW(target, WM_KEYDOWN, 'C', static_cast<LPARAM>((scan << 16) | 1u));
        evidence.sent = 1;
    }
    catch (...)
    {
        if (!evidence.error)
            evidence.error = GetLastError() ? GetLastError() : ERROR_GEN_FAILURE;
    }
    return evidence;
}

void dispatch(const CWPSTRUCT &message)
{
    if (retired || !host::privateDesktop())
        return;
    if (pipe == INVALID_HANDLE_VALUE)
    {
        open(message);
        return;
    }
    if (message.hwnd != target || message.wParam != atom ||
        message.lParam != binding.identity.agent)
        return;
    if (WaitForSingleObject(owner, 0) != WAIT_TIMEOUT)
    {
        retire();
        return;
    }
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
    {
        retire();
        return;
    }
    if (!available)
        return;
    if (available != sizeof(cancel::Request))
    {
        retire();
        return;
    }
    cancel::Request request;
    DWORD read = 0;
    if (!ReadFile(pipe, &request, sizeof(request), &read, nullptr) || read != sizeof(request))
    {
        retire();
        return;
    }
    bool accepted = false;
    if (!bound)
        accepted = bind(request);
    else if (cancel::next(request, binding, sequence, GetTickCount64()))
    {
        host::live(owner, binding.identity.agent, binding.identity.agentCreated);
        host::require(host::consoleHost(owner) == GetCurrentProcessId() &&
                          GetCurrentThreadId() == binding.identity.hostThread,
                      L"Live host binding");
        sequence = request.sequence;
        accepted = true;
    }
    if (!accepted)
    {
        reply(cancel::Result::Rejected, &request);
        return;
    }
    if (request.operation == cancel::Operation::Cancel)
    {
        const auto keys = cancelRead();
        if (!SetEvent(boundary))
        {
            retire();
            return;
        }
        reply(keys.sent && keys.restored && keys.equal && !keys.error ? cancel::Result::Accepted
                                                                      : cancel::Result::Failure,
              &request, &keys);
        if (!keys.restored || !keys.equal || keys.error)
            retire();
    }
    else if (request.operation == cancel::Operation::Close)
    {
        const bool queued = requestConsoleClose();
        reply(queued ? cancel::Result::Accepted : cancel::Result::Failure, &request);
        retire();
    }
    else
    {
        reply(cancel::Result::Accepted, &request);
    }
}
} // namespace

extern "C" __declspec(dllexport) LRESULT CALLBACK VT7PtyHostCallback(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && lp)
    {
        const auto &message = *reinterpret_cast<const CWPSTRUCT *>(lp);
        if (message.message == WM_NULL && InterlockedCompareExchange(&entered, 1, 0) == 0)
        {
            try
            {
                dispatch(message);
            }
            catch (...)
            {
                retire();
            }
            InterlockedExchange(&entered, 0);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *)
{
    if (reason == DLL_PROCESS_ATTACH)
        module = instance;
    return TRUE;
}
