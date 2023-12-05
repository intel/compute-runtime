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
    case DrmIoctl::gemClose:
        RETURN_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::gemVmCreate:
        RETURN_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::gemVmDestroy:
        RETURN_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::gemMmapOffset:
        RETURN_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::gemCreate:
        RETURN_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::gemExecbuffer2:
        RETURN_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::gemVmBind:
        RETURN_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::query:
        RETURN_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::gemContextCreateExt:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::gemContextDestroy:
        RETURN_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::gemWaitUserFence:
        RETURN_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::primeFdToHandle:
        RETURN_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::primeHandleToFd:
        RETURN_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    case DrmIoctl::debuggerOpen:
        return getIoctlRequestValueDebugger(ioctlRequest);
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

std::string IoctlHelperXe::getIoctlString(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        STRINGIFY_ME(DRM_IOCTL_GEM_CLOSE);
    case DrmIoctl::gemVmCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_CREATE);
    case DrmIoctl::gemVmDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_DESTROY);
    case DrmIoctl::gemMmapOffset:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_MMAP_OFFSET);
    case DrmIoctl::gemCreate:
        STRINGIFY_ME(DRM_IOCTL_XE_GEM_CREATE);
    case DrmIoctl::gemExecbuffer2:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC);
    case DrmIoctl::gemVmBind:
        STRINGIFY_ME(DRM_IOCTL_XE_VM_BIND);
    case DrmIoctl::query:
        STRINGIFY_ME(DRM_IOCTL_XE_DEVICE_QUERY);
    case DrmIoctl::gemContextCreateExt:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_CREATE);
    case DrmIoctl::gemContextDestroy:
        STRINGIFY_ME(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY);
    case DrmIoctl::gemWaitUserFence:
        STRINGIFY_ME(DRM_IOCTL_XE_WAIT_USER_FENCE);
    case DrmIoctl::primeFdToHandle:
        STRINGIFY_ME(DRM_IOCTL_PRIME_FD_TO_HANDLE);
    case DrmIoctl::primeHandleToFd:
        STRINGIFY_ME(DRM_IOCTL_PRIME_HANDLE_TO_FD);
    case DrmIoctl::debuggerOpen:
        STRINGIFY_ME(DRM_IOCTL_XE_EUDEBUG_CONNECT);
    default:
        return "???";
    }
}
} // namespace NEO
