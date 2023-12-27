/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"

#include "drm/drm.h"

#include <fcntl.h>
#include <sstream>

namespace NEO {

int IoctlHelper::ioctl(DrmIoctl request, void *arg) {
    return drm.ioctl(request, arg);
}

void IoctlHelper::setupIpVersion() {
    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    hwInfo.ipVersion.value = compilerProductHelper.getHwIpVersion(hwInfo);
}

uint32_t IoctlHelper::getFlagsForPrimeHandleToFd() const {
    return DRM_CLOEXEC | DRM_RDWR;
}

unsigned int IoctlHelper::getIoctlRequestValueBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        return DRM_IOCTL_GEM_CLOSE;
    case DrmIoctl::primeFdToHandle:
        return DRM_IOCTL_PRIME_FD_TO_HANDLE;
    case DrmIoctl::primeHandleToFd:
        return DRM_IOCTL_PRIME_HANDLE_TO_FD;
    default:
        UNRECOVERABLE_IF(true);
        return 0u;
    }
}

std::string IoctlHelper::getIoctlStringBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        return "DRM_IOCTL_GEM_CLOSE";
    case DrmIoctl::primeFdToHandle:
        return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
    case DrmIoctl::primeHandleToFd:
        return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

bool IoctlHelper::checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const {
    return (error == EINTR || error == EAGAIN || error == EBUSY || error == -EBUSY);
}
} // namespace NEO
