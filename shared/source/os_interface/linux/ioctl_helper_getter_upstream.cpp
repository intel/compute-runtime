/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <functional>
#include <memory>

namespace NEO {
std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm &drm)>> ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};

std::unique_ptr<IoctlHelper> IoctlHelper::getI915Helper(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, Drm &drm) {
    auto productSpecificIoctlHelperCreator = ioctlHelperFactory[productFamily];
    if (productSpecificIoctlHelperCreator) {
        return productSpecificIoctlHelperCreator.value()(drm);
    }
    return std::make_unique<IoctlHelperUpstream>(drm);
}

} // namespace NEO
