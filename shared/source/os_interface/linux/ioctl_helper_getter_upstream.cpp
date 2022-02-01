/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <memory>

namespace NEO {
IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};

IoctlHelper *IoctlHelper::get(const HardwareInfo *hwInfo, const std::string &prelimVersion) {
    auto product = hwInfo->platform.eProductFamily;

    auto productSpecificIoctlHelper = ioctlHelperFactory[product];
    if (productSpecificIoctlHelper) {
        return productSpecificIoctlHelper->clone();
    }
    return new IoctlHelperUpstream{};
}

} // namespace NEO
