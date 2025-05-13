/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/external_semaphore_linux.h"

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name) {
    if (!osInterface) {
        return nullptr;
    }

    auto externalSemaphore = ExternalSemaphoreLinux::create(osInterface);

    bool result = externalSemaphore->importSemaphore(nullptr, fd, 0, nullptr, type, false);
    if (result == false) {
        return nullptr;
    }

    return externalSemaphore;
}

std::unique_ptr<ExternalSemaphoreLinux> ExternalSemaphoreLinux::create(OSInterface *osInterface) {
    auto externalSemaphoreLinux = std::make_unique<ExternalSemaphoreLinux>();
    externalSemaphoreLinux->osInterface = osInterface;
    externalSemaphoreLinux->state = SemaphoreState::Initial;

    return externalSemaphoreLinux;
}

bool ExternalSemaphoreLinux::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) {
    switch (type) {
    case ExternalSemaphore::OpaqueFd:
        break;
    default:
        DEBUG_BREAK_IF(true);
        return false;
    }

    auto drm = this->osInterface->getDriverModel()->as<Drm>();

    struct SyncObjHandle args = {};
    args.fd = fd;
    args.handle = 0;

    auto ioctlHelper = drm->getIoctlHelper();

    int ret = ioctlHelper->ioctl(DrmIoctl::syncObjFdToHandle, &args);
    if (ret != 0) {
        return false;
    }

    this->syncHandle = args.handle;

    return true;
}

bool ExternalSemaphoreLinux::enqueueWait(uint64_t *fenceValue) {
    return false;
}

bool ExternalSemaphoreLinux::enqueueSignal(uint64_t *fenceValue) {
    return false;
}

} // namespace NEO