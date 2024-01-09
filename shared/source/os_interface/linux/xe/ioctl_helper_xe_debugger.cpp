/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        RETURN_ME(DRM_IOCTL_XE_EUDEBUG_CONNECT);

    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

} // namespace NEO