// Copyright (c) 2011-2016 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "../include/vt7pty.h"

#include "../shared/Protocol.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/GenRandom.h"
#include "../shared/OwnedHandle.h"
#include "../shared/StringBuilder.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsSecurity.h"
#include "../shared/WindowsVersion.h"
#include "../shared/Assert.h"
#include "../shared/Exception.h"
#include "../shared/Version.h"

#include "AgentLocation.h"
#include "ClientException.h"
#include "VT7PtyInternal.h"



/*****************************************************************************
 * Error handling -- translate C++ exceptions to an optional error object
 * output and log the result. */

static const vt7pty_error_s kOutOfMemory = {
    VT7PTY_ERROR_OUT_OF_MEMORY,
    L"Out of memory",
    nullptr
};

static const vt7pty_error_s kBadRpcPacket = {
    VT7PTY_ERROR_UNSPECIFIED,
    L"Bad RPC packet",
    nullptr
};

static const vt7pty_error_s kUncaughtException = {
    VT7PTY_ERROR_UNSPECIFIED,
    L"Uncaught C++ exception",
    nullptr
};

/* Gets the error code from the error object. */
VT7PTY_API vt7pty_result_t vt7pty_error_code(vt7pty_error_ptr_t err) {
    return err != nullptr ? err->code : VT7PTY_ERROR_SUCCESS;
}

/* Returns a textual representation of the error.  The string is freed when
 * the error is freed. */
VT7PTY_API LPCWSTR vt7pty_error_msg(vt7pty_error_ptr_t err) {
    if (err != nullptr) {
        if (err->msgStatic != nullptr) {
            return err->msgStatic;
        } else {
            ASSERT(err->msgDynamic != nullptr);
            std::wstring *msgPtr = err->msgDynamic->get();
            ASSERT(msgPtr != nullptr);
            return msgPtr->c_str();
        }
    } else {
        return L"Success";
    }
}

/* Free the error object.  Every error returned from the VT7Pty API must be
 * freed. */
VT7PTY_API void vt7pty_error_free(vt7pty_error_ptr_t err) {
    if (err != nullptr && err->msgDynamic != nullptr) {
        delete err->msgDynamic;
        delete err;
    }
}

static void translateException(vt7pty_error_ptr_t *&err) {
    vt7pty_error_ptr_t ret = nullptr;
    try {
        try {
            throw;
        } catch (const ReadBuffer::DecodeError&) {
            ret = const_cast<vt7pty_error_ptr_t>(&kBadRpcPacket);
        } catch (const ClientException &e) {
            std::unique_ptr<vt7pty_error_t> obj(new vt7pty_error_t);
            obj->code = e.code();
            obj->msgStatic = nullptr;
            obj->msgDynamic =
                new std::shared_ptr<std::wstring>(e.whatSharedStr());
            ret = obj.release();
        } catch (const VT7PtyException &e) {
            std::unique_ptr<vt7pty_error_t> obj(new vt7pty_error_t);
            std::shared_ptr<std::wstring> msg(new std::wstring(e.what()));
            obj->code = VT7PTY_ERROR_UNSPECIFIED;
            obj->msgStatic = nullptr;
            obj->msgDynamic = new std::shared_ptr<std::wstring>(msg);
            ret = obj.release();
        }
    } catch (const std::bad_alloc&) {
        ret = const_cast<vt7pty_error_ptr_t>(&kOutOfMemory);
    } catch (...) {
        ret = const_cast<vt7pty_error_ptr_t>(&kUncaughtException);
    }
    trace("libvt7pty error: code=%u msg='%s'",
        static_cast<unsigned>(ret->code),
        utf8FromWide(vt7pty_error_msg(ret)).c_str());
    if (err != nullptr) {
        *err = ret;
    } else {
        vt7pty_error_free(ret);
    }
}

#define API_TRY \
    if (err != nullptr) { *err = nullptr; } \
    try

#define API_CATCH(ret) \
    catch (...) { translateException(err); return (ret); }



/*****************************************************************************
 * Configuration of a new agent. */

VT7PTY_API vt7pty_config_t *
vt7pty_config_new(UINT64 flags, vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT((flags & VT7PTY_FLAG_MASK) == flags);
        std::unique_ptr<vt7pty_config_t> ret(new vt7pty_config_t);
        ret->flags = flags;
        return ret.release();
    } API_CATCH(nullptr)
}

VT7PTY_API void vt7pty_config_free(vt7pty_config_t *cfg) {
    delete cfg;
}

VT7PTY_API void
vt7pty_config_set_initial_size(vt7pty_config_t *cfg, int cols, int rows) {
    ASSERT(cfg != nullptr && cols > 0 && rows > 0);
    cfg->cols = cols;
    cfg->rows = rows;
}

VT7PTY_API void
vt7pty_config_set_mouse_mode(vt7pty_config_t *cfg, int mouseMode) {
    ASSERT(cfg != nullptr &&
        mouseMode >= VT7PTY_MOUSE_MODE_NONE &&
        mouseMode <= VT7PTY_MOUSE_MODE_FORCE);
    cfg->mouseMode = mouseMode;
}

VT7PTY_API void
vt7pty_config_set_agent_timeout(vt7pty_config_t *cfg, DWORD timeoutMs) {
    ASSERT(cfg != nullptr && timeoutMs > 0);
    cfg->timeoutMs = timeoutMs;
}



/*****************************************************************************
 * Agent I/O. */

namespace {

// Once an I/O operation fails with ERROR_IO_PENDING, the caller *must* wait
// for it to complete, even after calling CancelIo on it!  See
// https://blogs.msdn.microsoft.com/oldnewthing/20110202-00/?p=11613.  This
// class enforces that requirement.
class PendingIo {
    HANDLE m_file;
    OVERLAPPED &m_over;
    bool m_finished;
public:
    // The file handle and OVERLAPPED object must live as long as the PendingIo
    // object.
    PendingIo(HANDLE file, OVERLAPPED &over) :
        m_file(file), m_over(over), m_finished(false) {}
    ~PendingIo() {
        if (!m_finished) {
            // We're not usually that interested in CancelIo's return value.
            // In any case, we must not throw an exception in this dtor.
            CancelIo(m_file);
            waitForCompletion();
        }
    }
    std::tuple<BOOL, DWORD> waitForCompletion(DWORD &actual) noexcept {
        m_finished = true;
        const BOOL success =
            GetOverlappedResult(m_file, &m_over, &actual, TRUE);
        return std::make_tuple(success, GetLastError());
    }
    std::tuple<BOOL, DWORD> waitForCompletion() noexcept {
        DWORD actual = 0;
        return waitForCompletion(actual);
    }
};

} // anonymous namespace

static void handlePendingIo(vt7pty_t &wp, OVERLAPPED &over, BOOL &success,
                            DWORD &lastError, DWORD &actual) {
    if (!success && lastError == ERROR_IO_PENDING) {
        PendingIo io(wp.controlPipe.get(), over);
        const HANDLE waitHandles[2] = { wp.ioEvent.get(),
                                        wp.agentProcess.get() };
        DWORD waitRet = WaitForMultipleObjects(
            2, waitHandles, FALSE, wp.agentTimeoutMs);
        if (waitRet != WAIT_OBJECT_0) {
            // The I/O is still pending.  Cancel it, close the I/O event, and
            // throw an exception.
            if (waitRet == WAIT_OBJECT_0 + 1) {
                throw ClientException(VT7PTY_ERROR_AGENT_DIED, L"agent died");
            } else if (waitRet == WAIT_TIMEOUT) {
                throw ClientException(VT7PTY_ERROR_AGENT_TIMEOUT,
                                      L"agent timed out");
            } else if (waitRet == WAIT_FAILED) {
                throwWindowsError(L"WaitForMultipleObjects failed");
            } else {
                ASSERT(false &&
                    "unexpected WaitForMultipleObjects return value");
            }
        }
        std::tie(success, lastError) = io.waitForCompletion(actual);
    }
}

static void handlePendingIo(vt7pty_t &wp, OVERLAPPED &over, BOOL &success,
                            DWORD &lastError) {
    DWORD actual = 0;
    handlePendingIo(wp, over, success, lastError, actual);
}

static void handleReadWriteErrors(vt7pty_t &wp, BOOL success, DWORD lastError,
                                  const wchar_t *genericErrMsg) {
    if (!success) {
        // If the pipe connection is broken after it's been connected, then
        // later I/O operations fail with ERROR_BROKEN_PIPE (reads) or
        // ERROR_NO_DATA (writes).  With Wine, they may also fail with
        // ERROR_PIPE_NOT_CONNECTED.  See this gist[1].
        //
        // [1] https://gist.github.com/rprichard/8dd8ca134b39534b7da2733994aa07ba
        if (lastError == ERROR_BROKEN_PIPE || lastError == ERROR_NO_DATA ||
                lastError == ERROR_PIPE_NOT_CONNECTED) {
            throw ClientException(VT7PTY_ERROR_LOST_CONNECTION,
                L"lost connection to agent");
        } else {
            throwWindowsError(genericErrMsg, lastError);
        }
    }
}

// Calls ConnectNamedPipe to wait until the agent connects to the control pipe.
static void
connectControlPipe(vt7pty_t &wp) {
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = ConnectNamedPipe(wp.controlPipe.get(), &over);
    DWORD lastError = GetLastError();
    handlePendingIo(wp, over, success, lastError);
    if (!success && lastError == ERROR_PIPE_CONNECTED) {
        success = TRUE;
    }
    if (!success) {
        throwWindowsError(L"ConnectNamedPipe failed", lastError);
    }
}

static void writeData(vt7pty_t &wp, const void *data, size_t amount) {
    // Perform a single pipe write.
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = WriteFile(wp.controlPipe.get(), data, amount,
                             &actual, &over);
    DWORD lastError = GetLastError();
    if (!success) {
        handlePendingIo(wp, over, success, lastError, actual);
        handleReadWriteErrors(wp, success, lastError, L"WriteFile failed");
        ASSERT(success);
    }
    // TODO: Can a partial write actually happen somehow?
    ASSERT(actual == amount && "WriteFile wrote fewer bytes than requested");
}

static inline WriteBuffer newPacket() {
    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0); // Reserve space for size.
    return packet;
}

static void writePacket(vt7pty_t &wp, WriteBuffer &packet) {
    const auto &buf = packet.buf();
    packet.replaceRawValue<uint64_t>(0, buf.size());
    writeData(wp, buf.data(), buf.size());
}

static size_t readData(vt7pty_t &wp, void *data, size_t amount) {
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = ReadFile(wp.controlPipe.get(), data, amount,
                            &actual, &over);
    DWORD lastError = GetLastError();
    if (!success) {
        handlePendingIo(wp, over, success, lastError, actual);
        handleReadWriteErrors(wp, success, lastError, L"ReadFile failed");
    }
    return actual;
}

static void readAll(vt7pty_t &wp, void *data, size_t amount) {
    while (amount > 0) {
        const size_t chunk = readData(wp, data, amount);
        ASSERT(chunk <= amount && "readData result is larger than amount");
        data = reinterpret_cast<char*>(data) + chunk;
        amount -= chunk;
    }
}

static uint64_t readUInt64(vt7pty_t &wp) {
    uint64_t ret = 0;
    readAll(wp, &ret, sizeof(ret));
    return ret;
}

// Returns a reply packet's payload.
static ReadBuffer readPacket(vt7pty_t &wp) {
    const uint64_t packetSize = readUInt64(wp);
    if (packetSize < sizeof(packetSize) || packetSize > SIZE_MAX) {
        throwVT7PtyException(L"Agent RPC error: invalid packet size");
    }
    const size_t payloadSize = packetSize - sizeof(packetSize);
    std::vector<char> bytes(payloadSize);
    readAll(wp, bytes.data(), bytes.size());
    return ReadBuffer(std::move(bytes));
}

static OwnedHandle createControlPipe(const std::wstring &name) {
    const auto sd = createPipeSecurityDescriptorOwnerFullControl();
    if (!sd) {
        throwVT7PtyException(
            L"could not create the control pipe's SECURITY_DESCRIPTOR");
    }
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    HANDLE ret = CreateNamedPipeW(name.c_str(),
        /*dwOpenMode=*/
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE |
            FILE_FLAG_OVERLAPPED,
        /*dwPipeMode=*/PIPE_REJECT_REMOTE_CLIENTS,
        /*nMaxInstances=*/1,
        /*nOutBufferSize=*/8192,
        /*nInBufferSize=*/256,
        /*nDefaultTimeOut=*/30000,
        &sa);
    if (ret == INVALID_HANDLE_VALUE) {
        throwWindowsError(L"CreateNamedPipeW failed");
    }
    return OwnedHandle(ret);
}



/*****************************************************************************
 * Start the agent. */

static OwnedHandle createEvent() {
    // manual reset, initially unset
    HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (h == nullptr) {
        throwWindowsError(L"CreateEventW failed");
    }
    return OwnedHandle(h);
}

// For debugging purposes, provide a way to keep the console on the main window
// station, visible.
static bool shouldShowConsoleWindow() {
    char buf[32];
    return GetEnvironmentVariableA("VT7PTY_SHOW_CONSOLE", buf, sizeof(buf)) > 0;
}

static bool shouldSpecifyHideFlag() {
    const bool force = hasDebugFlag("force_sw_hide");
    const bool suppress = hasDebugFlag("no_sw_hide");
    bool ret = !shouldShowConsoleWindow();
    if (force && suppress) {
        trace("error: Both the force_sw_hide and no_sw_hide flags are set");
    } else if (force) {
        ret = true;
    } else if (suppress) {
        ret = false;
    }
    return ret;
}

static OwnedHandle startAgentProcess(
        const std::wstring &controlPipeName,
        const std::wstring &params,
        DWORD creationFlags,
        DWORD &agentPid) {
    const std::wstring exePath = findAgentProgram();
    const std::wstring cmdline =
        (WStringBuilder(256)
            << L"\"" << exePath << L"\" "
            << controlPipeName << L' '
            << params).str_moved();

    auto cmdlineV = vectorWithNulFromString(cmdline);
    // Start the agent.
    STARTUPINFOW sui = {};
    sui.cb = sizeof(sui);

    if (shouldSpecifyHideFlag()) {
        sui.dwFlags |= STARTF_USESHOWWINDOW;
        sui.wShowWindow = SW_HIDE;
    }
    PROCESS_INFORMATION pi = {};
    const BOOL success =
        CreateProcessW(exePath.c_str(),
                       cmdlineV.data(),
                       nullptr, nullptr,
                       /*bInheritHandles=*/FALSE,
                       /*dwCreationFlags=*/creationFlags,
                       nullptr, nullptr,
                       &sui, &pi);
    if (!success) {
        const DWORD lastError = GetLastError();
        const auto errStr =
            (WStringBuilder(256)
                << L"VT7Pty-Agent CreateProcess failed: cmdline='" << cmdline
                << L"' err=0x" << whexOfInt(lastError)).str_moved();
        throw ClientException(
            VT7PTY_ERROR_AGENT_CREATION_FAILED, errStr.c_str());
    }
    CloseHandle(pi.hThread);
    TRACE("Created agent successfully, pid=%u, cmdline=%s",
          static_cast<unsigned int>(pi.dwProcessId),
          utf8FromWide(cmdline).c_str());
    agentPid = pi.dwProcessId;
    return OwnedHandle(pi.hProcess);
}

static void verifyPipeClientPid(HANDLE serverPipe, DWORD agentPid) {
    const auto client = getNamedPipeClientProcessId(serverPipe);
    const auto success = std::get<0>(client);
    const auto lastError = std::get<2>(client);
    if (success == GetNamedPipeClientProcessId_Result::Success) {
        const auto clientPid = std::get<1>(client);
        if (clientPid != agentPid) {
            WStringBuilder errMsg;
            errMsg << L"Security check failed: pipe client pid (" << clientPid
                   << L") does not match agent pid (" << agentPid << L")";
            throwVT7PtyException(errMsg.c_str());
        }
    } else {
        throwWindowsError(L"GetNamedPipeClientProcessId failed", lastError);
    }
}

static std::unique_ptr<vt7pty_t>
createAgentSession(const vt7pty_config_t *cfg,
                   const std::wstring &params,
                   DWORD creationFlags) {
    std::unique_ptr<vt7pty_t> wp(new vt7pty_t);
    wp->agentTimeoutMs = cfg->timeoutMs;
    wp->ioEvent = createEvent();

    // Create control server pipe.
    const auto pipeName =
        L"\\\\.\\pipe\\vt7pty-control-v1-" + GenRandom().uniqueName();
    wp->controlPipe = createControlPipe(pipeName);

    DWORD agentPid = 0;
    wp->agentProcess = startAgentProcess(
        pipeName, params, creationFlags, agentPid);
    connectControlPipe(*wp.get());
    verifyPipeClientPid(wp->controlPipe.get(), agentPid);

    return std::move(wp);
}

VT7PTY_API vt7pty_t *
vt7pty_open(const vt7pty_config_t *cfg,
            vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(cfg != nullptr);
        dumpWindowsVersion();
        dumpVersionToTrace();

        // Start the primary agent session.
        const auto params =
            (WStringBuilder(128)
                << cfg->flags << L' '
                << cfg->mouseMode << L' '
                << cfg->cols << L' '
                << cfg->rows).str_moved();
        auto wp = createAgentSession(cfg, params, CREATE_NEW_CONSOLE);

        // Validate the agent before accepting its pipe names.  This prevents
        // a colocated agent from a different VT7Pty build from silently using
        // an incompatible protocol.
        auto packet = readPacket(*wp.get());
        AgentHandshake handshake;
        try {
            handshake = readAgentHandshake(packet);
        } catch (const ReadBuffer::DecodeError &) {
            throw ClientException(
                VT7PTY_ERROR_AGENT_INCOMPATIBLE,
                L"VT7Pty agent handshake is missing or malformed");
        }
        const auto handshakeStatus = classifyAgentHandshake(handshake);
        if (handshakeStatus == AgentHandshakeStatus::WrongIdentity) {
            const auto message =
                (WStringBuilder(192)
                    << L"VT7Pty agent identity mismatch: expected '"
                    << VT7PTY_AGENT_IDENTITY << L"', received '"
                    << handshake.identity << L"'").str_moved();
            throw ClientException(
                VT7PTY_ERROR_AGENT_INCOMPATIBLE, message.c_str());
        }
        if (handshakeStatus == AgentHandshakeStatus::UnsupportedVersion) {
            const auto message =
                (WStringBuilder(192)
                    << L"VT7Pty agent protocol mismatch: client="
                    << VT7PTY_PROTOCOL_VERSION << L", agent="
                    << handshake.protocolVersion).str_moved();
            throw ClientException(
                VT7PTY_ERROR_AGENT_INCOMPATIBLE, message.c_str());
        }

        // Get the CONIN/CONOUT pipe names.
        wp->coninPipeName = packet.getWString();
        wp->conoutPipeName = packet.getWString();
        if (cfg->flags & VT7PTY_FLAG_CONERR) {
            wp->conerrPipeName = packet.getWString();
        }
        packet.assertEof();

        return wp.release();
    } API_CATCH(nullptr)
}

VT7PTY_API HANDLE vt7pty_agent_process(vt7pty_t *wp) {
    ASSERT(wp != nullptr);
    return wp->agentProcess.get();
}



/*****************************************************************************
 * I/O pipes. */

static const wchar_t *cstrFromWStringOrNull(const std::wstring &str) {
    try {
        return str.c_str();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

VT7PTY_API LPCWSTR vt7pty_conin_name(vt7pty_t *wp) {
    ASSERT(wp != nullptr);
    return cstrFromWStringOrNull(wp->coninPipeName);
}

VT7PTY_API LPCWSTR vt7pty_conout_name(vt7pty_t *wp) {
    ASSERT(wp != nullptr);
    return cstrFromWStringOrNull(wp->conoutPipeName);
}

VT7PTY_API LPCWSTR vt7pty_conerr_name(vt7pty_t *wp) {
    ASSERT(wp != nullptr);
    if (wp->conerrPipeName.empty()) {
        return nullptr;
    } else {
        return cstrFromWStringOrNull(wp->conerrPipeName);
    }
}



/*****************************************************************************
 * VT7Pty agent RPC calls. */

namespace {

// Close the control pipe if something goes wrong with the pipe communication,
// which could leave the control pipe in an inconsistent state.
class RpcOperation {
public:
    RpcOperation(vt7pty_t &wp) : m_wp(wp) {
        if (m_wp.controlPipe.get() == nullptr) {
            throwVT7PtyException(L"Agent shutdown due to RPC failure");
        }
    }
    ~RpcOperation() {
        if (!m_success) {
            trace("~RpcOperation: Closing control pipe");
            m_wp.controlPipe.dispose(true);
        }
    }
    void success() { m_success = true; }
private:
    vt7pty_t &m_wp;
    bool m_success = false;
};

} // anonymous namespace



/*****************************************************************************
 * VT7Pty agent RPC call: process creation. */

// Return a std::wstring containing every character of the environment block.
// Typically, the block is non-empty, so the std::wstring returned ends with
// two NUL terminators.  (These two terminators are counted in size(), so
// calling c_str() produces a triply-terminated string.)
static std::wstring wstringFromEnvBlock(const wchar_t *env) {
    std::wstring envStr;
    if (env != NULL) {
        const wchar_t *p = env;
        while (*p != L'\0') {
            p += wcslen(p) + 1;
        }
        p++;
        envStr.assign(env, p);

        // Assuming the environment was non-empty, envStr now ends with two NUL
        // terminators.
        //
        // If the environment were empty, though, then envStr would only be
        // singly terminated, but the MSDN documentation thinks an env block is
        // always doubly-terminated, so add an extra NUL just in case it
        // matters.
        const auto envStrSz = envStr.size();
        if (envStrSz == 1) {
            ASSERT(envStr[0] == L'\0');
            envStr.push_back(L'\0');
        } else {
            ASSERT(envStrSz >= 3);
            ASSERT(envStr[envStrSz - 3] != L'\0');
            ASSERT(envStr[envStrSz - 2] == L'\0');
            ASSERT(envStr[envStrSz - 1] == L'\0');
        }
    }
    return envStr;
}

VT7PTY_API vt7pty_spawn_config_t *
vt7pty_spawn_config_new(UINT64 spawnFlags,
                        LPCWSTR appname /*OPTIONAL*/,
                        LPCWSTR cmdline /*OPTIONAL*/,
                        LPCWSTR cwd /*OPTIONAL*/,
                        LPCWSTR env /*OPTIONAL*/,
                        vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT((spawnFlags & VT7PTY_SPAWN_FLAG_MASK) == spawnFlags);
        std::unique_ptr<vt7pty_spawn_config_t> cfg(new vt7pty_spawn_config_t);
        cfg->spawnFlags = spawnFlags;
        if (appname != nullptr) { cfg->appname = appname; }
        if (cmdline != nullptr) { cfg->cmdline = cmdline; }
        if (cwd != nullptr) { cfg->cwd = cwd; }
        if (env != nullptr) { cfg->env = wstringFromEnvBlock(env); }
        return cfg.release();
    } API_CATCH(nullptr)
}

VT7PTY_API void vt7pty_spawn_config_free(vt7pty_spawn_config_t *cfg) {
    delete cfg;
}

// It's safe to truncate a handle from 64-bits to 32-bits, or to sign-extend it
// back to 64-bits.  See the MSDN article, "Interprocess Communication Between
// 32-bit and 64-bit Applications".
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203.aspx
static inline HANDLE handleFromInt64(int64_t i) {
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(i));
}

// Given a process and a handle in that process, duplicate the handle into the
// current process and close it in the originating process.
static inline OwnedHandle stealHandle(HANDLE process, HANDLE handle) {
    HANDLE result = nullptr;
    if (!DuplicateHandle(process, handle,
            GetCurrentProcess(),
            &result, 0, FALSE,
            DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
        throwWindowsError(L"DuplicateHandle of process handle");
    }
    return OwnedHandle(result);
}

VT7PTY_API BOOL
vt7pty_spawn(vt7pty_t *wp,
             const vt7pty_spawn_config_t *cfg,
             HANDLE *process_handle /*OPTIONAL*/,
             HANDLE *thread_handle /*OPTIONAL*/,
             DWORD *create_process_error /*OPTIONAL*/,
             vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr && cfg != nullptr);

        if (process_handle != nullptr) { *process_handle = nullptr; }
        if (thread_handle != nullptr) { *thread_handle = nullptr; }
        if (create_process_error != nullptr) { *create_process_error = 0; }

        std::lock_guard<std::mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);

        // Send spawn request.
        auto packet = newPacket();
        packet.putInt32(AgentMessage::StartProcess);
        packet.putInt64(cfg->spawnFlags);
        packet.putInt32(process_handle != nullptr);
        packet.putInt32(thread_handle != nullptr);
        packet.putWString(cfg->appname);
        packet.putWString(cfg->cmdline);
        packet.putWString(cfg->cwd);
        packet.putWString(cfg->env);
        writePacket(*wp, packet);

        // Receive reply.
        auto reply = readPacket(*wp);
        const auto result = static_cast<StartProcessResult>(reply.getInt32());
        if (result == StartProcessResult::CreateProcessFailed) {
            const DWORD lastError = reply.getInt32();
            reply.assertEof();
            if (create_process_error != nullptr) {
                *create_process_error = lastError;
            }
            rpc.success();
            throw ClientException(VT7PTY_ERROR_SPAWN_CREATE_PROCESS_FAILED,
                L"CreateProcess failed");
        } else if (result == StartProcessResult::ProcessCreated) {
            const HANDLE remoteProcess = handleFromInt64(reply.getInt64());
            const HANDLE remoteThread = handleFromInt64(reply.getInt64());
            reply.assertEof();
            OwnedHandle localProcess;
            OwnedHandle localThread;
            if (remoteProcess != nullptr) {
                localProcess =
                    stealHandle(wp->agentProcess.get(), remoteProcess);
            }
            if (remoteThread != nullptr) {
                localThread =
                    stealHandle(wp->agentProcess.get(), remoteThread);
            }
            if (process_handle != nullptr) {
                *process_handle = localProcess.release();
            }
            if (thread_handle != nullptr) {
                *thread_handle = localThread.release();
            }
            rpc.success();
        } else {
            throwVT7PtyException(
                L"Agent RPC error: invalid StartProcessResult");
        }
        return TRUE;
    } API_CATCH(FALSE)
}



/*****************************************************************************
 * VT7Pty agent RPC calls: everything else */

VT7PTY_API BOOL
vt7pty_set_size(vt7pty_t *wp, int cols, int rows,
                vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr && cols > 0 && rows > 0);
        std::lock_guard<std::mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);
        auto packet = newPacket();
        packet.putInt32(AgentMessage::SetSize);
        packet.putInt32(cols);
        packet.putInt32(rows);
        writePacket(*wp, packet);
        readPacket(*wp).assertEof();
        rpc.success();
        return TRUE;
    } API_CATCH(FALSE)
}

VT7PTY_API int
vt7pty_get_console_process_list(vt7pty_t *wp, int *processList, const int processCount,
                                vt7pty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr);
        ASSERT(processList != nullptr);
        std::lock_guard<std::mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);
        auto packet = newPacket();
        packet.putInt32(AgentMessage::GetConsoleProcessList);
        writePacket(*wp, packet);
        auto reply = readPacket(*wp);

        auto actualProcessCount = reply.getInt32();

        if (actualProcessCount <= processCount) {
            for (auto i = 0; i < actualProcessCount; i++) {
                processList[i] = reply.getInt32();
            }
        }

        reply.assertEof();
        rpc.success();
        return actualProcessCount;
    } API_CATCH(0)
}

VT7PTY_API void vt7pty_free(vt7pty_t *wp) {
    // At least in principle, CloseHandle can fail, so this deletion can
    // fail.  It won't throw an exception, but maybe there's an error that
    // should be propagated?
    delete wp;
}
