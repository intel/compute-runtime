/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

uint64_t GmmResourceInfo::getDriverProtectionBits(uint32_t overrideUsage) {
    GMM_OVERRIDE_VALUES overrideValues{};
    if (GMM_RESOURCE_USAGE_UNKNOWN != overrideUsage) {
        overrideValues.Usage = overrideUsage;
    }
    return static_cast<uint64_t>(resourceInfo->GetDriverProtectionBits(overrideValues));
}

bool GmmResourceInfo::isDisplayable() const {
    return false;
}
} // namespace NEO