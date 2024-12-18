/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <string>

namespace NEO {

std::unique_ptr<IoctlHelper> IoctlHelper::getI915Helper(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, Drm &drm) {
    if (prelimVersion == "") {
        return std::make_unique<IoctlHelperUpstream>(drm);
    }
    return std::make_unique<IoctlHelperPrelim20>(drm);
}

} // namespace NEO
