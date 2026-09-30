/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/external_semaphore_linux.h"

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name, ImportResult &importResult) {
    if (!osInterface) {
        importResult = ImportResult::unsupported;
        return nullptr;
    }

    auto externalSemaphore = ExternalSemaphoreLinux::create(osInterface);

    importResult = externalSemaphore->importSemaphore(nullptr, fd, 0, nullptr, type, false);
    if (importResult != ImportResult::success) {
        return nullptr;
    }

    return externalSemaphore;
}

std::unique_ptr<ExternalSemaphoreLinux> ExternalSemaphoreLinux::create(OSInterface *osInterface) {
    auto externalSemaphoreLinux = std::make_unique<ExternalSemaphoreLinux>();
    externalSemaphoreLinux->osInterface = osInterface;

    return externalSemaphoreLinux;
}

ExternalSemaphore::ImportResult ExternalSemaphoreLinux::importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) {
    switch (type) {
    case ExternalSemaphore::OpaqueFd:
    case ExternalSemaphore::TimelineSemaphoreFd:
        break;
    default:
        DEBUG_BREAK_IF(true);
        return ImportResult::unsupported;
    }

    auto drm = this->osInterface->getDriverModel()->as<Drm>();

    struct SyncObjHandle args = {};
    args.fd = fd;
    args.handle = 0;

    auto ioctlHelper = drm->getIoctlHelper();

    int ret = ioctlHelper->ioctl(DrmIoctl::syncObjFdToHandle, &args);
    if (fd > 0) {
        SysCalls::close(fd);
    }
    if (ret != 0) {
        return ImportResult::invalidResource;
    }

    this->syncHandle = args.handle;
    this->type = type;

    return ImportResult::success;
}

ExternalSemaphoreLinux::~ExternalSemaphoreLinux() {
    // Safety check: only destroy if we have a valid handle and osInterface
    if (syncHandle == 0 || osInterface == nullptr) {
        return;
    }

    auto drm = osInterface->getDriverModel()->as<Drm>();
    auto ioctlHelper = drm->getIoctlHelper();

    SyncObjDestroy args = {};
    args.handle = syncHandle;

    // Ignore return value - destructor should not throw
    ioctlHelper->ioctl(DrmIoctl::syncObjDestroy, &args);

    syncHandle = 0;
}

} // namespace NEO
