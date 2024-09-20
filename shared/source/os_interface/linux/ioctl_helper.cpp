/*
 * Copyright (C) 2021-2024 Intel Corporation
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
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm.h"

#include <fcntl.h>
#include <sstream>

namespace NEO {

void IoctlHelper::setExternalContext(ExternalCtx *ctx) {
    externalCtx = ctx;
}

int IoctlHelper::ioctl(DrmIoctl request, void *arg) {
    if (externalCtx) {
        return externalCtx->ioctl(externalCtx->handle, drm.getFileDescriptor(), getIoctlRequestValue(request), arg, false);
    }
    return drm.ioctl(request, arg);
}

int IoctlHelper::ioctl(int fd, DrmIoctl request, void *arg) {
    return NEO::SysCalls::ioctl(fd, getIoctlRequestValue(request), arg);
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

uint64_t *IoctlHelper::getPagingFenceAddress(uint32_t vmHandleId, OsContextLinux *osContext) {
    if (osContext) {
        return osContext->getFenceAddr(vmHandleId);
    } else {
        return drm.getFenceAddr(vmHandleId);
    }
}

} // namespace NEO
