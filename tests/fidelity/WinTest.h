// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <sddl.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace fidelity {
inline void check(bool okay, const char *operation) {
    if (!okay) throw std::runtime_error(std::string(operation) + " (Win32 " + std::to_string(GetLastError()) + ")");
}
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { reset(); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    void reset(HANDLE h = INVALID_HANDLE_VALUE) {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = h;
    }
};
template<typename T> struct LocalAllocation {
    T *value=nullptr;
    ~LocalAllocation() { if (value) LocalFree(value); }
    LocalAllocation() = default;
    LocalAllocation(const LocalAllocation &) = delete;
    LocalAllocation &operator=(const LocalAllocation &) = delete;
};
inline std::wstring quote(std::wstring_view argument) {
    std::wstring result = L"\""; size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (c == L'"') result += L'\\';
        result += c;
    }
    result.append(slashes * 2,L'\\'); return result + L'"';
}
inline std::wstring directory() {
    std::vector<wchar_t> text(32768);
    DWORD length = GetModuleFileNameW(nullptr,text.data(),static_cast<DWORD>(text.size()));
    check(length > 0 && length < text.size(),"module path");
    std::wstring path(text.data(),length);
    return path.substr(0,path.find_last_of(L'\\'));
}
inline bool complete(HANDLE file, OVERLAPPED &ov, DWORD &bytes, DWORD timeout) {
    if (WaitForSingleObject(ov.hEvent,timeout) != WAIT_OBJECT_0) {
        CancelIoEx(file,&ov);
        // OVERLAPPED/event storage must survive until cancellation completes.
        // The outer runner handles this explicit process-level failure.
        if (WaitForSingleObject(ov.hEvent,10000) != WAIT_OBJECT_0) ExitProcess(124);
        GetOverlappedResult(file,&ov,&bytes,FALSE);
        throw std::runtime_error("test-channel I/O deadline");
    }
    return GetOverlappedResult(file,&ov,&bytes,FALSE) != FALSE;
}
inline void transfer(HANDLE file, void *data, DWORD length, bool write, DWORD timeout=5000) {
    auto *cursor = static_cast<unsigned char *>(data);
    while (length != 0) {
        Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr)); check(event.value != nullptr,"I/O event");
        OVERLAPPED ov = {}; ov.hEvent = event.value;
        DWORD amount = 0;
        BOOL okay = write ? WriteFile(file,cursor,length,&amount,&ov) : ReadFile(file,cursor,length,&amount,&ov);
        if (!okay && GetLastError() == ERROR_IO_PENDING) okay = complete(file,ov,amount,timeout);
        check(okay != FALSE && amount > 0 && amount <= length,"test-channel transfer");
        cursor += amount; length -= amount;
    }
}
inline void connect(HANDLE server) {
    Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr)); check(event.value != nullptr,"connect event");
    OVERLAPPED ov = {}; ov.hEvent = event.value;
    const BOOL okay = ConnectNamedPipe(server,&ov);
    if (okay) return;
    const DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) return;
    check(error == ERROR_IO_PENDING,"connect");
    DWORD ignored=0; check(complete(server,ov,ignored,5000),"connect completion");
}
inline HANDLE privateServer(const std::wstring &name) {
    Handle token; check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value),"token");
    DWORD size=0; GetTokenInformation(token.value,TokenUser,nullptr,0,&size);
    check(size > 0,"token size");
    std::vector<unsigned char> storage(size);
    check(GetTokenInformation(token.value,TokenUser,storage.data(),size,&size),"token user");
    LocalAllocation<wchar_t> sid;
    check(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid,&sid.value),"SID");
    const std::wstring sddl=L"D:P(A;;GA;;;SY)(A;;GA;;;" + std::wstring(sid.value) + L")";
    LocalAllocation<void> descriptor;
    check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor.value,nullptr),"pipe ACL");
    SECURITY_ATTRIBUTES attributes={sizeof(attributes),descriptor.value,FALSE};
    HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&attributes);
    check(pipe != INVALID_HANDLE_VALUE,"private pipe");
    return pipe;
}
inline std::string json(std::string_view text) {
    std::string result="\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) {
            char buffer[7]; sprintf_s(buffer,"\\u%04x",static_cast<unsigned int>(c)); result += buffer;
        } else result += static_cast<char>(c);
    }
    return result + '"';
}
inline void writeFile(const std::wstring &path, std::string_view contents) {
    Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    check(file.value != INVALID_HANDLE_VALUE,"result file");
    DWORD written=0;
    check(contents.size() <= 1024 * 1024 && WriteFile(file.value,contents.data(),static_cast<DWORD>(contents.size()),&written,nullptr) &&
        written == contents.size(),"write result");
}
constexpr uint32_t packetMagic=0x31465456;
struct Key {
    uint32_t character=0, virtualKey=0, modifiers=0, repeat=0;
};
struct Packet {
    uint32_t magic=packetMagic, schema=1;
    uint32_t columns=0, rows=0, bufferColumns=0, bufferRows=0, windowLeft=0, windowTop=0;
    uint32_t cursorX=0, cursorY=0, cursorVisible=0;
    uint32_t inputMode=0, outputMode=0, inputCP=0, outputCP=0;
    uint32_t controlCount=0, keyCount=0, bufferTag=1;
    uint32_t fontWidth=0, fontHeight=0, fontError=0;
    wchar_t fontName[LF_FACESIZE]={};
    Key keys[128]={};
    CHAR_INFO cells[120*40]={};
};
inline void validate(const Packet &p) {
    if (p.magic != packetMagic || p.schema != 1 || p.columns == 0 || p.columns > 120 ||
        p.rows == 0 || p.rows > 40 || p.keyCount > 128 || p.cursorX >= p.columns || p.cursorY >= p.rows ||
        (p.bufferTag != 1 && p.bufferTag != 2) || p.bufferColumns > 32767 || p.bufferRows > 32767 ||
        p.windowLeft > p.bufferColumns || p.windowTop > p.bufferRows ||
        p.columns > p.bufferColumns-p.windowLeft || p.rows > p.bufferRows-p.windowTop)
        throw std::runtime_error("malformed fixture packet");
}
} // namespace fidelity
