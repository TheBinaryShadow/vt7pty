// Copyright (c) 2016 Ryan Prichard
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

#include "WindowsVersion.h"

#include <windows.h>
#include <stdint.h>

#include <memory>
#include <format>
#include <string>
#include <tuple>

#include "DebugClient.h"
#include "OsModule.h"
#include "StringUtil.h"
#include "Assert.h"
#include "Exception.h"

namespace {

using Version = std::tuple<DWORD, DWORD>;

using RtlGetVersion_t = LONG (WINAPI *)(OSVERSIONINFOW *);

OSVERSIONINFOEXW getWindowsVersionInfo() {
    // RtlGetVersion reports the running system rather than a manifest-based
    // compatibility version. It is present throughout VT7Pty's Windows 7+
    // platform range.
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        throwWindowsError(L"GetModuleHandleW(ntdll.dll) failed");
    }
    const auto rtlGetVersion = reinterpret_cast<RtlGetVersion_t>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion == nullptr) {
        throwWindowsError(L"RtlGetVersion is missing from ntdll.dll");
    }

    OSVERSIONINFOEXW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    const LONG status = rtlGetVersion(
        reinterpret_cast<OSVERSIONINFOW *>(&info));
    if (status < 0) {
        throw Exception(L"RtlGetVersion failed");
    }
    return info;
}

Version getWindowsVersion() {
    const auto info = getWindowsVersionInfo();
    return Version(info.dwMajorVersion, info.dwMinorVersion);
}

struct ModuleNotFound : Exception {
    ModuleNotFound() : Exception(L"ModuleNotFound") {}
};

// Throws Exception on error.
std::wstring getSystemDirectory() {
    wchar_t systemDirectory[MAX_PATH];
    const UINT size = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (size == 0) {
        throwWindowsError(L"GetSystemDirectory failed");
    } else if (size >= MAX_PATH) {
        throw Exception(
            L"GetSystemDirectory: path is longer than MAX_PATH");
    }
    return systemDirectory;
}

template <typename Function>
Function requireProc(
        OsModule &module, const char *name, const wchar_t *wideName) {
    const auto function = reinterpret_cast<Function>(module.proc(name));
    if (function == nullptr) {
        throw Exception(std::wstring(wideName) + L" is missing");
    }
    return function;
}

// Throws Exception on error.
VS_FIXEDFILEINFO getFixedFileInfo(const std::wstring &path) {
    // version.dll is not a conventional KnownDll, so if we link to it, there's
    // a danger of accidentally loading a malicious DLL.  In a more typical
    // application, perhaps we'd guard against this security issue by
    // controlling which directories this code runs in (e.g. *not* the
    // "Downloads" directory), but that's harder for the VT7Pty library.
    OsModule versionDll(
        (getSystemDirectory() + L"\\version.dll").c_str(),
        OsModule::LoadErrorBehavior::Throw);
    const auto pGetFileVersionInfoSizeW =
        requireProc<decltype(&GetFileVersionInfoSizeW)>(
            versionDll, "GetFileVersionInfoSizeW", L"GetFileVersionInfoSizeW");
    const auto pGetFileVersionInfoW =
        requireProc<decltype(&GetFileVersionInfoW)>(
            versionDll, "GetFileVersionInfoW", L"GetFileVersionInfoW");
    const auto pVerQueryValueW = requireProc<decltype(&VerQueryValueW)>(
        versionDll, "VerQueryValueW", L"VerQueryValueW");
    DWORD size = pGetFileVersionInfoSizeW(path.c_str(), nullptr);
    if (!size) {
        // Different Windows releases use either of these errors for a module
        // without file-version data.
        if (GetLastError() == ERROR_FILE_NOT_FOUND ||
                GetLastError() == ERROR_RESOURCE_DATA_NOT_FOUND) {
            throw ModuleNotFound();
        } else {
            throwWindowsError(
                (L"GetFileVersionInfoSizeW failed on " + path).c_str());
        }
    }
    auto versionBuffer = std::make_unique<char[]>(size);
    if (!pGetFileVersionInfoW(path.c_str(), 0, size, versionBuffer.get())) {
        throwWindowsError((L"GetFileVersionInfoW failed on " + path).c_str());
    }
    VS_FIXEDFILEINFO *versionInfo = nullptr;
    UINT versionInfoSize = 0;
    if (!pVerQueryValueW(
                versionBuffer.get(), L"\\",
                reinterpret_cast<void**>(&versionInfo), &versionInfoSize) ||
            versionInfo == nullptr ||
            versionInfoSize != sizeof(VS_FIXEDFILEINFO) ||
            versionInfo->dwSignature != 0xFEEF04BD) {
        throw Exception((L"VerQueryValueW failed on " + path).c_str());
    }
    return *versionInfo;
}

uint64_t productVersionFromInfo(const VS_FIXEDFILEINFO &info) {
    return (static_cast<uint64_t>(info.dwProductVersionMS) << 32) |
           (static_cast<uint64_t>(info.dwProductVersionLS));
}

uint64_t fileVersionFromInfo(const VS_FIXEDFILEINFO &info) {
    return (static_cast<uint64_t>(info.dwFileVersionMS) << 32) |
           (static_cast<uint64_t>(info.dwFileVersionLS));
}

std::string versionToString(uint64_t version) {
    return std::format("{}.{}.{}.{}",
        static_cast<uint16_t>(version >> 48),
        static_cast<uint16_t>(version >> 32),
        static_cast<uint16_t>(version >> 16),
        static_cast<uint16_t>(version));
}

} // anonymous namespace

// Returns true for Windows 8 (or Windows Server 2012) or newer.
bool isWindows8OrGreater() {
    return getWindowsVersion() >= Version(6, 2);
}

void dumpWindowsVersion() {
    if (!isTracingEnabled()) {
        return;
    }
    const auto info = getWindowsVersionInfo();
    auto version = std::format("{}.{}.{} SP{}.{} ",
        info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber,
        info.wServicePackMajor, info.wServicePackMinor);
    switch (info.wProductType) {
        case VER_NT_WORKSTATION:        version += "Client"; break;
        case VER_NT_DOMAIN_CONTROLLER:  version += "DomainController"; break;
        case VER_NT_SERVER:             version += "Server"; break;
        default:
            version += std::format("product={}", info.wProductType); break;
    }
    version += " X64";
    const auto dllVersion = [](const wchar_t *dllPath) -> std::string {
        try {
            const auto info = getFixedFileInfo(dllPath);
            return std::format("{}:F:{}/P:{}", utf8FromWide(dllPath),
                versionToString(fileVersionFromInfo(info)),
                versionToString(productVersionFromInfo(info)));
        } catch (const ModuleNotFound&) {
            return utf8FromWide(dllPath) + ":none";
        } catch (const Exception &e) {
            trace("Error getting %s version: %s",
                utf8FromWide(dllPath).c_str(), utf8FromWide(e.what()).c_str());
            return utf8FromWide(dllPath) + ":error";
        }
    };
    version += ' ' + dllVersion(L"kernel32.dll");
    // ConEmu provides a DLL that hooks many Windows APIs, especially console
    // APIs.  Its existence and version number could be useful in debugging.
    version += ' ' + dllVersion(L"ConEmuHk64.dll");
    trace("Windows version: %s", version.c_str());
}
