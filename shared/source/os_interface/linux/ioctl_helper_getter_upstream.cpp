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

IoctlHelper *IoctlHelper::get(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion) {
    auto productSpecificIoctlHelper = ioctlHelperFactory[productFamily];
    if (productSpecificIoctlHelper) {
        return productSpecificIoctlHelper->clone();
    }
    return new IoctlHelperUpstream{};
}

} // namespace NEO
