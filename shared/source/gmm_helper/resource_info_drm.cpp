/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

uint64_t GmmResourceInfo::getDriverProtectionBits(uint32_t overrideUsage, bool compressionDenied) {
    return 0u;
}

bool GmmResourceInfo::isResourceDenyCompressionEnabled() {
    return false;
}

bool GmmResourceInfo::isDisplayable() const {
    return false;
}
} // namespace NEO