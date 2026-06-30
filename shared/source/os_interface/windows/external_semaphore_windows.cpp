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
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <memory>

typedef VOID(NTAPI *_RtlInitUnicodeString)(PUNICODE_STRING destinationString, PCWSTR sourceString);
typedef VOID(NTAPI *_NtOpenDirectoryObject)(PHANDLE directoryHandle, ACCESS_MASK desiredAccess, POBJECT_ATTRIBUTES objectAttributes);

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name) {
    if (osInterface) {
        auto externalSemaphore = ExternalSemaphoreWindows::create(osInterface);

        bool result = externalSemaphore->importSemaphore(handle, fd, 0, name, type, false);
        if (result == false) {
            return nullptr;
        }

        return externalSemaphore;
    }
    return nullptr;
}

std::unique_ptr<ExternalSemaphoreWindows> ExternalSemaphoreWindows::create(OSInterface *osInterface) {
    auto extSemWindows = std::make_unique<ExternalSemaphoreWindows>();
    extSemWindows->osInterface = osInterface;
    extSemWindows->state = SemaphoreState::Initial;

    return extSemWindows;
}

std::wstring ExternalSemaphoreWindows::getNamedObjectDirectoryPath(uint32_t sessionId, const wchar_t *name, const wchar_t **relativeName) {
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

void *ExternalSemaphoreWindows::openSyncObjectByName(Gdi *gdi, const wchar_t *name, uint32_t desiredAccess, bool forceGlobal) {
    DWORD sessionId = 0;
    if (!forceGlobal) {
        SysCalls::processIdToSessionId(GetCurrentProcessId(), &sessionId);
    }

    const wchar_t *relativeName = name;
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

    D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME openName = {};
    openName.dwDesiredAccess = desiredAccess;
    openName.pObjAttrib = &objectAttributes;
    auto status = gdi->openSyncObjectNtHandleFromName(&openName);
    SysCalls::closeHandle(rootDirectory);
    if (status != STATUS_SUCCESS) {
        return nullptr;
    }

    return openName.hNtHandle;
}

bool ExternalSemaphoreWindows::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) {
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
        return false;
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
            return false;
        }

        syncNtHandle = openName.hNtHandle;
    }

    if (name != nullptr && isNameable) {
        std::wstring wideName;
        size_t length = strlen(name) + 1;
        wideName.resize(length);
        mbstowcs(&wideName[0], name, length);
        bool forceGlobal = (type == ExternalSemaphore::TimelineSemaphoreWin32);

        syncNtHandle = openSyncObjectByName(gdi, wideName.c_str(), D3DDDI_SYNC_OBJECT_ALL_ACCESS, forceGlobal);
        if (syncNtHandle == nullptr) {
            return false;
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
        return false;
    }

    this->syncHandle = open.hSyncObject;

    return true;
}

bool ExternalSemaphoreWindows::enqueueWait(uint64_t *fenceValue) {
    auto wddm = this->osInterface->getDriverModel()->as<Wddm>();

    D3DDDI_WAITFORSYNCHRONIZATIONOBJECTFROMCPU_FLAGS waitFlags = {};
    waitFlags.WaitAny = false;

    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait = {};
    wait.hDevice = wddm->getDeviceHandle();
    wait.ObjectCount = 1;
    wait.ObjectHandleArray = &this->syncHandle;

    uint64_t lastSignaledValue = 0;
    if (this->type == ExternalSemaphore::OpaqueWin32) {
        lastSignaledValue = *this->pLastSignaledValue;
        wait.FenceValueArray = &lastSignaledValue;
    } else {
        wait.FenceValueArray = fenceValue;
    }

    wait.hAsyncEvent = nullptr;
    wait.Flags = waitFlags;

    auto status = wddm->getGdi()->waitForSynchronizationObjectFromCpu(&wait);
    if (status != STATUS_SUCCESS) {
        return false;
    }

    this->state = SemaphoreState::Signaled;

    return true;
}

bool ExternalSemaphoreWindows::enqueueSignal(uint64_t *fenceValue) {
    auto wddm = this->osInterface->getDriverModel()->as<Wddm>();

    D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU signal = {};
    signal.hDevice = wddm->getDeviceHandle();
    signal.ObjectCount = 1;
    signal.ObjectHandleArray = &this->syncHandle;

    if (this->type == ExternalSemaphore::TimelineSemaphoreWin32) {
        signal.Flags.AllowFenceRewind = true;
    }

    uint64_t lastSignaledValue = 0;
    if (this->type == ExternalSemaphore::OpaqueWin32) {
        lastSignaledValue = *this->pLastSignaledValue + 2;
        signal.FenceValueArray = &lastSignaledValue;
    } else {
        signal.FenceValueArray = fenceValue;
    }

    auto status = wddm->getGdi()->signalSynchronizationObjectFromCpu(&signal);
    if (status != STATUS_SUCCESS) {
        return false;
    }

    if (this->type == ExternalSemaphore::OpaqueWin32) {
        *this->pLastSignaledValue += 2;
    }
    this->state = SemaphoreState::Signaled;

    return true;
}

} // namespace NEO
