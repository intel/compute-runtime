/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};

IoctlHelper *IoctlHelper::get(PRODUCT_FAMILY product) {
    auto ioctlHelper = ioctlHelperFactory[product];
    if (!ioctlHelper) {
        return IoctlHelperDefault::get();
    }
    return ioctlHelper;
}

uint32_t IoctlHelper::ioctl(Drm *drm, unsigned long request, void *arg) {
    return drm->ioctl(request, arg);
}

} // namespace NEO
