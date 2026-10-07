/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/named_object_windows.h"

#include "shared/source/os_interface/windows/sys_calls.h"

#include <cwchar>

namespace NEO {
namespace NamedObjectWindows {

std::wstring getNamedObjectDirectoryPath(uint32_t sessionId, const wchar_t *name, const wchar_t **relativeName) {
    constexpr wchar_t globalPrefix[] = L"Global\\";
    constexpr wchar_t localPrefix[] = L"Local\\";
    constexpr size_t globalPrefixLength = (sizeof(globalPrefix) / sizeof(wchar_t)) - 1;
    constexpr size_t localPrefixLength = (sizeof(localPrefix) / sizeof(wchar_t)) - 1;

    *relativeName = name;

    if (wcsncmp(name, globalPrefix, globalPrefixLength) == 0) {
        *relativeName = name + globalPrefixLength;
        return std::wstring(L"\\BaseNamedObjects");
    }

    if (wcsncmp(name, localPrefix, localPrefixLength) == 0) {
        *relativeName = name + localPrefixLength;
    }

    // Session 0 (services) has no \Sessions\0 subtree; its base directory is \BaseNamedObjects.
    if (sessionId == 0) {
        return std::wstring(L"\\BaseNamedObjects");
    }

    wchar_t buffer[64];
    swprintf_s(buffer, L"\\Sessions\\%u\\BaseNamedObjects", sessionId);
    return std::wstring(buffer);
}

HANDLE openNamedObject(const wchar_t *name, bool forceGlobal, const OpenByAttributesFn &openFn) {
    if (name == nullptr) {
        return nullptr;
    }
    const size_t nameLength = wcsnlen(name, maxNameLengthInWideChars + 1);
    if (nameLength == 0u || nameLength > maxNameLengthInWideChars) {
        return nullptr;
    }

    DWORD sessionId = 0;
    if (!forceGlobal) {
        SysCalls::processIdToSessionId(GetCurrentProcessId(), &sessionId);
    }

    const wchar_t *relativeName = nullptr;
    std::wstring directoryPath = getNamedObjectDirectoryPath(sessionId, name, &relativeName);

    UNICODE_STRING unicodeDirectory;
    SysCalls::rtlInitUnicodeString(&unicodeDirectory, directoryPath.c_str());
    OBJECT_ATTRIBUTES directoryAttributes;
    InitializeObjectAttributes(&directoryAttributes, &unicodeDirectory, 0, nullptr, nullptr);

    HANDLE rootDirectory = nullptr;
    if (SysCalls::ntOpenDirectoryObject(&rootDirectory, 0x0002 /* DIRECTORY_TRAVERSE */, &directoryAttributes) != STATUS_SUCCESS) {
        return nullptr;
    }

    UNICODE_STRING unicodeName;
    SysCalls::rtlInitUnicodeString(&unicodeName, relativeName);
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, &unicodeName, 0, rootDirectory, nullptr);

    HANDLE handle = openFn(&objectAttributes);
    SysCalls::closeHandle(rootDirectory);
    return handle;
}

} // namespace NamedObjectWindows
} // namespace NEO
