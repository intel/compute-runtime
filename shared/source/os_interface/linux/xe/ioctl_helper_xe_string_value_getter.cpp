/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "drm/xe_drm.h"

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

namespace NEO {
unsigned int IoctlHelperXe::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    xeLog(" -> IoctlHelperXe::%s 0x%x\n", __FUNCTION__, ioctlRequest);
    switch (ioctlRequest) {
    case DrmIoctl::GemClose:
        RETURN_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::GemVmCreate:
        RETURN_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::GemVmDestroy:
        RETURN_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::GemMmapOffset:
        RETURN_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::GemCreate:
        RETURN_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::GemExecbuffer2:
        RETURN_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::GemVmBind:
        RETURN_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::Query:
        RETURN_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::GemContextCreateExt:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::GemContextDestroy:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::GemWaitUserFence:
        RETURN_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::PrimeFdToHandle:
        RETURN_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::PrimeHandleToFd:
        RETURN_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

std::string IoctlHelperXe::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::GemClose:
        STRINGIFY_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::GemVmCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::GemVmDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::GemMmapOffset:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::GemCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::GemExecbuffer2:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::GemVmBind:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::Query:
        STRINGIFY_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::GemContextCreateExt:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::GemContextDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::GemWaitUserFence:
        STRINGIFY_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::PrimeFdToHandle:
        STRINGIFY_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::PrimeHandleToFd:
        STRINGIFY_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    default:
        return "???";
    }
}
} // namespace NEO
