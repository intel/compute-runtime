/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionLightSupported() const {
    return true;
}

template <>
inline bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
inline bool ReleaseHelperHw<release>::isGlobalBindlessAllocatorEnabled() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::getFtrXe2Compression() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isStateCacheInvalidationNoCsStallRequired() const {
    if (debugManager.flags.EnableStateCacheInvalidationWa.get() != -1) {
        return false;
    }
    return true;
}

} // namespace NEO
