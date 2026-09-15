/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {

uint64_t GmmResourceInfo::getDriverProtectionBits(bool compressionDenied) {
    return static_cast<uint64_t>(resourceInfo->GetDriverProtectionBits({}));
}

bool GmmResourceInfo::isResourceDenyCompressionEnabled() {
    return false;
}

bool GmmResourceInfo::isDisplayable() const {
    return false;
}
} // namespace NEO
