/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <memory>

namespace NEO {
IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};
static auto ioctlHelper = std::make_unique<IoctlHelperUpstream>();
IoctlHelper *IoctlHelper::get(Drm *drm) {
    return ioctlHelper.get();
}

} // namespace NEO
