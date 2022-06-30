/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <functional>
#include <string>

namespace NEO {

std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm &drm)>> ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};

std::unique_ptr<IoctlHelper> IoctlHelper::get(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, const std::string &drmVersion, Drm &drm) {
    auto productSpecificIoctlHelperCreator = ioctlHelperFactory[productFamily];
    if (productSpecificIoctlHelperCreator) {
        return productSpecificIoctlHelperCreator.value()(drm);
    }
    if (prelimVersion == "") {
        return std::make_unique<IoctlHelperUpstream>(drm);
    }
    return std::make_unique<IoctlHelperPrelim20>(drm);
}

} // namespace NEO
