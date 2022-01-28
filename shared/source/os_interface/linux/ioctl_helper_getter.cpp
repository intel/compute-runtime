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

#include <string>

namespace NEO {

IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};

std::map<std::string, std::shared_ptr<IoctlHelper>> ioctlHelperImpls{
    {"", std::make_shared<IoctlHelperUpstream>()},
    {"2.0", std::make_shared<IoctlHelperPrelim20>()}};

IoctlHelper *IoctlHelper::get(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion) {
    auto productSpecificIoctlHelper = ioctlHelperFactory[productFamily];
    if (productSpecificIoctlHelper) {
        return productSpecificIoctlHelper->clone();
    }
    if (ioctlHelperImpls.find(prelimVersion) != ioctlHelperImpls.end()) {
        return ioctlHelperImpls[prelimVersion]->clone();
    }
    return new IoctlHelperPrelim20{}; //fallback to 2.0 if no proper implementation has been found
}

} // namespace NEO
