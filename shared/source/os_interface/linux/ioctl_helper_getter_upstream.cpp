/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <memory>

namespace NEO {

std::unique_ptr<IoctlHelper> IoctlHelper::getI915Helper(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion, Drm &drm) {
    return std::make_unique<IoctlHelperUpstream>(drm);
}

} // namespace NEO
