/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/external_semaphore_helper_windows.h"

#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <memory>

namespace NEO {

std::unique_ptr<ExternalSemaphoreHelperWindows> ExternalSemaphoreHelperWindows::create(OSInterface *osInterface) {
    auto extSemHelperWindows = std::make_unique<ExternalSemaphoreHelperWindows>();
    extSemHelperWindows->osInterface = osInterface;

    return extSemHelperWindows;
}

bool ExternalSemaphoreHelperWindows::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, ExternalSemaphoreHelper::Type type, bool isNative, void *syncHandle) {
    switch (type) {
    case ExternalSemaphoreHelper::D3d12Fence:
    case ExternalSemaphoreHelper::OpaqueWin32:
        break;
    default:
        return false;
    }
    HANDLE syncNtHandle = nullptr;

    auto wddm = this->osInterface->getDriverModel()->as<Wddm>();

    syncNtHandle = reinterpret_cast<HANDLE>(extHandle);

    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 open = {};
    open.hNtHandle = syncNtHandle;
    open.hDevice = wddm->getDeviceHandle();
    open.Flags.NoGPUAccess = (type == ExternalSemaphoreHelper::D3d12Fence);

    auto status = wddm->getGdi()->openSyncObjectFromNtHandle2(&open);
    if (status != STATUS_SUCCESS) {
        return false;
    }

    return true;
}

bool ExternalSemaphoreHelperWindows::semaphoreWait(const CommandStreamReceiver *csr, void *syncHandle) {
    return false;
}

bool ExternalSemaphoreHelperWindows::semaphoreSignal(const CommandStreamReceiver *csr, void *syncHandle) {
    return false;
}

bool ExternalSemaphoreHelperWindows::isSupported(ExternalSemaphoreHelper::Type type) {
    switch (type) {
    case ExternalSemaphoreHelper::OpaqueWin32:
    case ExternalSemaphoreHelper::D3d12Fence:
        return true;
    default:
        return false;
    }
}

} // namespace NEO
