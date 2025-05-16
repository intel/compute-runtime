/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

uint64_t GmmResourceInfo::getDriverProtectionBits(bool overrideUsage, uint32_t usage) {
    return 0u;
}

bool GmmResourceInfo::isDisplayable() const {
    return false;
}
} // namespace NEO