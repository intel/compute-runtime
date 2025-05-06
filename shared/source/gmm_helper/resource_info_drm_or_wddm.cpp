/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

uint64_t GmmResourceInfo::getDriverProtectionBits() {
    GMM_OVERRIDE_VALUES overrideValues{};
    return static_cast<uint64_t>(resourceInfo->GetDriverProtectionBits(overrideValues));
}

bool GmmResourceInfo::isDisplayable() const {
    return false;
}
} // namespace NEO