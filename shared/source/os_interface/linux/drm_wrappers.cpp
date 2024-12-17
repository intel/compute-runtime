/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_wrappers.h"

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "drm.h"

#include <cstddef>

namespace NEO {

unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->getIoctlRequestValue(ioctlRequest);
    }
    switch (ioctlRequest) {
    case DrmIoctl::version:
        return DRM_IOCTL_VERSION;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

bool checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->checkIfIoctlReinvokeRequired(error, ioctlRequest);
    }
    return (error == EINTR || error == EAGAIN || error == EBUSY || error == -EBUSY);
}
} // namespace NEO
