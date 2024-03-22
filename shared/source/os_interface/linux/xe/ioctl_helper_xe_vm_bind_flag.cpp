/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "drm/xe_drm.h"

namespace NEO {

uint64_t IoctlHelperXe::getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLock) {
    uint64_t ret = 0;
    xeLog(" -> IoctlHelperXe::%s %d %d %d %d\n", __FUNCTION__, bindCapture, bindImmediate, bindMakeResident, bindLock);
    if (bindCapture) {
        ret |= XE_NEO_BIND_CAPTURE_FLAG;
    }
    if (bindImmediate) {
        ret |= DRM_XE_VM_BIND_FLAG_IMMEDIATE;
    }
    if (bindMakeResident) {
        ret |= XE_NEO_BIND_MAKERESIDENT_FLAG;
    }
    return ret;
}

} // namespace NEO
