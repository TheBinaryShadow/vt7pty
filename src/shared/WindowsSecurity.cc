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

#include "WindowsSecurity.h"

#include <sddl.h>

#include <array>
#include <format>

#include "DebugClient.h"
#include "OwnedHandle.h"
#include "Narrow.h"
#include "Assert.h"
#include "Exception.h"

namespace {

struct LocalFreer {
    void operator()(void *ptr) {
        if (ptr != nullptr) {
            LocalFree(reinterpret_cast<HLOCAL>(ptr));
        }
    }
};

using PointerLocal = std::unique_ptr<void, LocalFreer>;

template <typename T>
SecurityItem<T> localItem(typename T::type v) {
    using P = typename T::type;
    struct Impl : SecurityItem<T>::Impl {
        P m_v;
        Impl(P v) : m_v(v) {}
        virtual ~Impl() {
            LocalFree(reinterpret_cast<HLOCAL>(m_v));
        }
    };
    return SecurityItem<T>(v, std::make_unique<Impl>(v));
}

Sid allocatedSid(PSID v) {
    struct Impl : Sid::Impl {
        PSID m_v;
        Impl(PSID v) : m_v(v) {}
        virtual ~Impl() {
            if (m_v != nullptr) {
                FreeSid(m_v);
            }
        }
    };
    return Sid(v, std::make_unique<Impl>(v));
}

} // anonymous namespace

// Returns a handle to the thread's effective security token.  If the thread
// is impersonating another user, its token is returned, and otherwise, the
// process' security token is opened.  The handle is opened with TOKEN_QUERY.
static OwnedHandle openSecurityTokenForQuery() {
    HANDLE token = nullptr;
    // It is unclear whether OpenAsSelf matters for VT7Pty, or what the
    // most appropriate value is.
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY,
                         /*OpenAsSelf=*/FALSE, &token)) {
        if (GetLastError() != ERROR_NO_TOKEN) {
            throwWindowsError(L"OpenThreadToken failed");
        }
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            throwWindowsError(L"OpenProcessToken failed");
        }
    }
    ASSERT(token != nullptr &&
        "OpenThreadToken/OpenProcessToken token is NULL");
    return OwnedHandle(token);
}

// Returns the TokenOwner of the thread's effective security token.
Sid getOwnerSid() {
    struct Impl : Sid::Impl {
        std::unique_ptr<char[]> buffer;
    };

    OwnedHandle token = openSecurityTokenForQuery();
    DWORD actual = 0;
    BOOL success;
    success = GetTokenInformation(token.get(), TokenOwner,
        nullptr, 0, &actual);
    if (success) {
        throw Exception(L"getOwnerSid: GetTokenInformation: "
            L"expected ERROR_INSUFFICIENT_BUFFER");
    } else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        throwWindowsError(L"getOwnerSid: GetTokenInformation: "
            L"expected ERROR_INSUFFICIENT_BUFFER");
    }
    auto impl = std::make_unique<Impl>();
    impl->buffer = std::make_unique<char[]>(actual);
    success = GetTokenInformation(token.get(), TokenOwner,
                                  impl->buffer.get(), actual, &actual);
    if (!success) {
        throwWindowsError(L"getOwnerSid: GetTokenInformation");
    }
    TOKEN_OWNER tmp;
    ASSERT(actual >= sizeof(tmp));
    std::copy(
        impl->buffer.get(),
        impl->buffer.get() + sizeof(tmp),
        reinterpret_cast<char*>(&tmp));
    return Sid(tmp.Owner, std::move(impl));
}

Sid wellKnownSid(
        const wchar_t *debuggingName,
        SID_IDENTIFIER_AUTHORITY authority,
        BYTE authorityCount,
        DWORD subAuthority0/*=0*/,
        DWORD subAuthority1/*=0*/) {
    PSID psid = nullptr;
    if (!AllocateAndInitializeSid(&authority, authorityCount,
            subAuthority0,
            subAuthority1,
            0, 0, 0, 0, 0, 0,
            &psid)) {
        const auto err = GetLastError();
        const auto msg =
            std::wstring(L"wellKnownSid: error getting ") +
            debuggingName + L" SID";
        throwWindowsError(msg.c_str(), err);
    }
    return allocatedSid(psid);
}

Sid builtinAdminsSid() {
    // S-1-5-32-544
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid(L"BUILTIN\\Administrators group",
            authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,    // 32
            DOMAIN_ALIAS_RID_ADMINS);       // 544
}

Sid localSystemSid() {
    // S-1-5-18
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    return wellKnownSid(L"LocalSystem account",
            authority, 1,
            SECURITY_LOCAL_SYSTEM_RID);     // 18
}

static SecurityDescriptor finishSecurityDescriptor(
        size_t daclEntryCount,
        EXPLICIT_ACCESSW *daclEntries,
        Acl &outAcl) {
    {
        PACL aclRaw = nullptr;
        DWORD aclError =
            SetEntriesInAclW(
                             vt7pty::internal::checkedNarrow<ULONG>(daclEntryCount),
                             daclEntries,
                             nullptr, &aclRaw);
        if (aclError != ERROR_SUCCESS) {
            throw Exception(std::format(
                L"finishSecurityDescriptor: SetEntriesInAcl failed: {}",
                aclError));
        }
        outAcl = localItem<AclTag>(aclRaw);
    }

    const PSECURITY_DESCRIPTOR sdRaw =
        reinterpret_cast<PSECURITY_DESCRIPTOR>(
            LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
    if (sdRaw == nullptr) {
        throw Exception(L"finishSecurityDescriptor: LocalAlloc failed");
    }
    SecurityDescriptor sd = localItem<SecurityDescriptorTag>(sdRaw);
    if (!InitializeSecurityDescriptor(sdRaw, SECURITY_DESCRIPTOR_REVISION)) {
        throwWindowsError(
            L"finishSecurityDescriptor: InitializeSecurityDescriptor");
    }
    if (!SetSecurityDescriptorDacl(sdRaw, TRUE, outAcl.get(), FALSE)) {
        throwWindowsError(
            L"finishSecurityDescriptor: SetSecurityDescriptorDacl");
    }

    return std::move(sd);
}

// Create a security descriptor that grants full control to the local system
// account, built-in administrators, and the owner.
SecurityDescriptor
createPipeSecurityDescriptorOwnerFullControl() {

    struct Impl : SecurityDescriptor::Impl {
        Sid localSystem;
        Sid builtinAdmins;
        Sid owner;
        std::array<EXPLICIT_ACCESSW, 3> daclEntries = {};
        Acl dacl;
        SecurityDescriptor value;
    };

    auto impl = std::make_unique<Impl>();
    impl->localSystem = localSystemSid();
    impl->builtinAdmins = builtinAdminsSid();
    impl->owner = getOwnerSid();

    for (auto &ea : impl->daclEntries) {
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    }
    impl->daclEntries[0].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->localSystem.get());
    impl->daclEntries[1].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->builtinAdmins.get());
    impl->daclEntries[2].Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(impl->owner.get());

    impl->value = finishSecurityDescriptor(
        impl->daclEntries.size(),
        impl->daclEntries.data(),
        impl->dacl);

    const auto retValue = impl->value.get();
    return SecurityDescriptor(retValue, std::move(impl));
}

SecurityDescriptor getObjectSecurityDescriptor(HANDLE handle) {
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const DWORD errCode = GetSecurityInfo(handle, SE_KERNEL_OBJECT,
        OWNER_SECURITY_INFORMATION |
            GROUP_SECURITY_INFORMATION |
            DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &dacl, nullptr, &sd);
    if (errCode != ERROR_SUCCESS) {
        throwWindowsError(L"GetSecurityInfo failed");
    }
    return localItem<SecurityDescriptorTag>(sd);
}

// The (SID/SD)<->string conversion APIs are useful for testing and diagnostics
// but are too slow for ordinary request processing.

std::wstring sidToString(PSID sid) {
    wchar_t *sidString = NULL;
    BOOL success = ConvertSidToStringSidW(sid, &sidString);
    if (!success) {
        throwWindowsError(L"ConvertSidToStringSidW failed");
    }
    PointerLocal freer(sidString);
    return std::wstring(sidString);
}

Sid stringToSid(const std::wstring &str) {
    PSID psid = nullptr;
    BOOL success = ConvertStringSidToSidW(str.c_str(), &psid);
    if (!success) {
        const auto err = GetLastError();
        throwWindowsError(
            (std::wstring(L"ConvertStringSidToSidW failed on \"") +
                str + L'"').c_str(),
            err);
    }
    return localItem<SidTag>(psid);
}

SecurityDescriptor stringToSd(const std::wstring &str) {
    PSECURITY_DESCRIPTOR desc = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            str.c_str(), SDDL_REVISION_1, &desc, nullptr)) {
        const auto err = GetLastError();
        throwWindowsError(
            (std::wstring(L"ConvertStringSecurityDescriptorToSecurityDescriptorW failed on \"") +
                str + L'"').c_str(),
            err);
    }
    return localItem<SecurityDescriptorTag>(desc);
}

std::wstring sdToString(PSECURITY_DESCRIPTOR sd) {
    wchar_t *sdString = nullptr;
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(
            sd,
            SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION |
                GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION,
            &sdString,
            nullptr)) {
        throwWindowsError(
            L"ConvertSecurityDescriptorToStringSecurityDescriptor failed");
    }
    PointerLocal freer(sdString);
    return std::wstring(sdString);
}

std::tuple<GetNamedPipeClientProcessId_Result, DWORD, DWORD>
getNamedPipeClientProcessId(HANDLE serverPipe) {
    ULONG pid = 0;
    if (!GetNamedPipeClientProcessId(serverPipe, &pid)) {
        return std::make_tuple(
            GetNamedPipeClientProcessId_Result::Failure, 0, GetLastError());
    }
    return std::make_tuple(
        GetNamedPipeClientProcessId_Result::Success,
        static_cast<DWORD>(pid),
        0);
}
