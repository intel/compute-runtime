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

#include <memory>

namespace NEO {

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
