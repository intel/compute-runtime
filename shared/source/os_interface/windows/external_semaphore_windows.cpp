/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/external_semaphore_windows.h"

#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <memory>

typedef VOID(NTAPI *_RtlInitUnicodeString)(PUNICODE_STRING destinationString, PCWSTR sourceString);
typedef VOID(NTAPI *_NtOpenDirectoryObject)(PHANDLE directoryHandle, ACCESS_MASK desiredAccess, POBJECT_ATTRIBUTES objectAttributes);

typedef wchar_t SharedSyncName[9 + 2 * (sizeof(uint32_t) + sizeof(uint64_t))];

struct SharedMemoryContentHeader {
    alignas(8) uint64_t lastSignaledValue;
    SharedSyncName sharedSyncName;
    uint32_t access;
    uint32_t serializedSecurityDescriptorStringSize;
};

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd) {
    if (osInterface) {
        if (osInterface->getDriverModel()->getDriverModelType() == DriverModelType::wddm) {
            auto externalSemaphore = ExternalSemaphoreWindows::create(osInterface);

            bool result = externalSemaphore->importSemaphore(handle, fd, 0, nullptr, type, false);
            if (result == false) {
                return nullptr;
            }

            return externalSemaphore;
        }
    }
    return nullptr;
}

std::unique_ptr<ExternalSemaphoreWindows> ExternalSemaphoreWindows::create(OSInterface *osInterface) {
    auto extSemWindows = std::make_unique<ExternalSemaphoreWindows>();
    extSemWindows->osInterface = osInterface;
    extSemWindows->state = SemaphoreState::Initial;

    return extSemWindows;
}

bool ExternalSemaphoreWindows::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) {
    switch (type) {
    case ExternalSemaphore::D3d12Fence:
    case ExternalSemaphore::OpaqueWin32:
        break;
    default:
        return false;
    }
    HANDLE syncNtHandle = nullptr;

    this->type = type;

    auto wddm = this->osInterface->getDriverModel()->as<Wddm>();

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

        auto pCpuAddress = MapViewOfFile(syncNtHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0);

        auto sharedMemoryContentHeader = *reinterpret_cast<SharedMemoryContentHeader *>(pCpuAddress);
        auto access = sharedMemoryContentHeader.access;
        auto pSyncName = &sharedMemoryContentHeader.sharedSyncName[0];
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
        auto status = wddm->getGdi()->openSyncObjectNtHandleFromName(&openName);
        if (status != STATUS_SUCCESS) {
            return false;
        }

        syncNtHandle = openName.hNtHandle;
    }

    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 open = {};
    open.hNtHandle = syncNtHandle;
    open.hDevice = wddm->getDeviceHandle();
    open.Flags.NoGPUAccess = (type == ExternalSemaphore::D3d12Fence);

    auto status = wddm->getGdi()->openSyncObjectFromNtHandle2(&open);
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
    wait.FenceValueArray = fenceValue;
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
    signal.FenceValueArray = fenceValue;

    auto status = wddm->getGdi()->signalSynchronizationObjectFromCpu(&signal);
    if (status != STATUS_SUCCESS) {
        return false;
    }

    this->state = SemaphoreState::Signaled;

    return true;
}

} // namespace NEO
