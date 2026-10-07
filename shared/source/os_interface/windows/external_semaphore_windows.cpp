/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/external_semaphore_windows.h"

#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/named_object_windows.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <memory>

typedef VOID(NTAPI *_RtlInitUnicodeString)(PUNICODE_STRING destinationString, PCWSTR sourceString);
typedef VOID(NTAPI *_NtOpenDirectoryObject)(PHANDLE directoryHandle, ACCESS_MASK desiredAccess, POBJECT_ATTRIBUTES objectAttributes);

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name, ImportResult &importResult) {
    if (osInterface == nullptr) {
        importResult = ImportResult::unsupported;
        return nullptr;
    }

    auto externalSemaphore = ExternalSemaphoreWindows::create(osInterface);

    importResult = externalSemaphore->importSemaphore(handle, fd, 0, name, type, false);
    if (importResult != ImportResult::success) {
        return nullptr;
    }

    return externalSemaphore;
}

std::unique_ptr<ExternalSemaphoreWindows> ExternalSemaphoreWindows::create(OSInterface *osInterface) {
    auto extSemWindows = std::make_unique<ExternalSemaphoreWindows>();
    extSemWindows->osInterface = osInterface;

    return extSemWindows;
}

std::wstring ExternalSemaphoreWindows::convertUtf8NameToWide(const char *name) {
    const int lengthWithTerminator = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0);
    if ((lengthWithTerminator <= 1) ||
        (static_cast<size_t>(lengthWithTerminator - 1) > NamedObjectWindows::maxNameLengthInWideChars)) {
        return {};
    }

    std::wstring wideName(static_cast<size_t>(lengthWithTerminator), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wideName.data(), lengthWithTerminator) != lengthWithTerminator) {
        return {};
    }

    wideName.pop_back();
    return wideName;
}

void *ExternalSemaphoreWindows::openSyncObjectByName(Gdi *gdi, const char *name, uint32_t desiredAccess, bool forceGlobal) {
    const std::wstring wideName = convertUtf8NameToWide(name);
    if (wideName.empty()) {
        return nullptr;
    }

    return NamedObjectWindows::openNamedObject(wideName.c_str(), forceGlobal, [gdi, desiredAccess](OBJECT_ATTRIBUTES *objectAttributes) -> HANDLE {
        D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME openName = {};
        openName.dwDesiredAccess = desiredAccess;
        openName.pObjAttrib = objectAttributes;
        if (gdi->openSyncObjectNtHandleFromName(&openName) != STATUS_SUCCESS) {
            return nullptr;
        }

        return openName.hNtHandle;
    });
}

ExternalSemaphore::ImportResult ExternalSemaphoreWindows::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) {
    bool isD3dFence = false;
    bool isNameable = false;
    switch (type) {
    case ExternalSemaphore::D3d12Fence:
    case ExternalSemaphore::D3d11Fence:
        isD3dFence = true;
        [[fallthrough]];
    case ExternalSemaphore::TimelineSemaphoreWin32:
        isNameable = true;
        [[fallthrough]];
    case ExternalSemaphore::OpaqueWin32:
        break;
    default:
        return ImportResult::unsupported;
    }
    HANDLE syncNtHandle = nullptr;

    this->type = type;

    auto wddm = this->osInterface->getDriverModel()->as<Wddm>();
    Gdi *gdi = wddm->getGdi();

    syncNtHandle = reinterpret_cast<HANDLE>(extHandle);

    if (type == ExternalSemaphore::OpaqueWin32) {
        auto moduleHandle = GetModuleHandleA("ntdll.dll");
        _RtlInitUnicodeString rtlInitUnicodeString = (_RtlInitUnicodeString)GetProcAddress(moduleHandle, "RtlInitUnicodeString");
        _NtOpenDirectoryObject ntOpenDirectoryObject = (_NtOpenDirectoryObject)GetProcAddress(moduleHandle, "NtOpenDirectoryObject");

        HANDLE rootDirectory;
        OBJECT_ATTRIBUTES objectAttributesRootDirectory;
        UNICODE_STRING unicodeNameRootDirectory;
        PUNICODE_STRING pUnicodeNameRootDirectory = NULL;

        wchar_t baseName[] = L"\\BaseNamedObjects";
        rtlInitUnicodeString(&unicodeNameRootDirectory, baseName);
        pUnicodeNameRootDirectory = &unicodeNameRootDirectory;
        InitializeObjectAttributes(&objectAttributesRootDirectory, pUnicodeNameRootDirectory, 0, nullptr, nullptr);
        ntOpenDirectoryObject(&rootDirectory, 0x0004 /* DIRECTORY_CREATE_OBJECT */, &objectAttributesRootDirectory);

        this->pCpuAddress = MapViewOfFile(syncNtHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);

        auto sharedMemoryContentHeader = this->getSharedMemoryContentHeader();
        auto access = sharedMemoryContentHeader->access;
        auto pSyncName = &sharedMemoryContentHeader->sharedSyncName[0];
        this->pLastSignaledValue = &sharedMemoryContentHeader->lastSignaledValue;
        syncNtHandle = nullptr;

        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING unicodeName;

        PUNICODE_STRING pUnicodeName = NULL;
        const wchar_t *pName = reinterpret_cast<wchar_t *>(pSyncName);

        if (pName) {
            rtlInitUnicodeString(&unicodeName, pName);
            pUnicodeName = &unicodeName;
        }

        InitializeObjectAttributes(&objectAttributes, pUnicodeName, 0, rootDirectory, nullptr);

        D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME openName = {};
        openName.dwDesiredAccess = access;
        openName.pObjAttrib = &objectAttributes;
        auto status = gdi->openSyncObjectNtHandleFromName(&openName);
        if (status != STATUS_SUCCESS) {
            return ImportResult::invalidResource;
        }

        syncNtHandle = openName.hNtHandle;
    }

    if (name != nullptr && isNameable) {
        bool forceGlobal = (type == ExternalSemaphore::TimelineSemaphoreWin32);

        syncNtHandle = openSyncObjectByName(gdi, name, D3DDDI_SYNC_OBJECT_ALL_ACCESS, forceGlobal);
        if (syncNtHandle == nullptr) {
            return ImportResult::invalidResource;
        }
    }

    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 open = {};
    open.hNtHandle = syncNtHandle;
    open.hDevice = wddm->getDeviceHandle();
    open.Flags.NoGPUAccess = isD3dFence;

    auto status = gdi->openSyncObjectFromNtHandle2(&open);
    if (syncNtHandle != reinterpret_cast<HANDLE>(extHandle)) {
        SysCalls::closeHandle(syncNtHandle);
    }
    if (status != STATUS_SUCCESS) {
        return ImportResult::invalidResource;
    }

    this->syncHandle = open.hSyncObject;

    return ImportResult::success;
}

uint64_t ExternalSemaphoreWindows::acquireWaitFenceValue(uint64_t fenceValue) {
    if (this->type == ExternalSemaphore::OpaqueWin32) {
        return *this->pLastSignaledValue;
    }

    return fenceValue;
}

uint64_t ExternalSemaphoreWindows::acquireSignalFenceValue(uint64_t fenceValue) {
    if (this->type == ExternalSemaphore::OpaqueWin32) {
        return *this->pLastSignaledValue += 2;
    }

    return fenceValue;
}

} // namespace NEO
