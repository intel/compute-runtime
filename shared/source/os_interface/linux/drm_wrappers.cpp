/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_wrappers.h"

#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cstddef>

namespace NEO {

unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->getIoctlRequestValue(ioctlRequest);
    }
    switch (ioctlRequest) {
    case DrmIoctl::Getparam:
        return DRM_IOCTL_I915_GETPARAM;
    case DrmIoctl::Version:
        return DRM_IOCTL_VERSION;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

int getDrmParamValue(DrmParam drmParam, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->getDrmParamValue(drmParam);
    }
    switch (drmParam) {
    case DrmParam::ParamChipsetId:
        return I915_PARAM_CHIPSET_ID;
    case DrmParam::ParamRevision:
        return I915_PARAM_REVISION;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

std::string getDrmParamString(DrmParam drmParam, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->getDrmParamString(drmParam);
    }
    switch (drmParam) {
    case DrmParam::ParamChipsetId:
        return "I915_PARAM_CHIPSET_ID";
    case DrmParam::ParamRevision:
        return "I915_PARAM_REVISION";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

std::string getIoctlString(DrmIoctl ioctlRequest, IoctlHelper *ioctlHelper) {
    if (ioctlHelper) {
        return ioctlHelper->getIoctlString(ioctlRequest);
    }
    switch (ioctlRequest) {
    case DrmIoctl::Getparam:
        return "DRM_IOCTL_I915_GETPARAM";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}
} // namespace NEO
