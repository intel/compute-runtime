/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <>
void ReleaseHelperHw<release>::adjustRTDispatchGlobals(RTDispatchGlobals &rtDispatchGlobals, uint32_t rtStacksPerDss, uint32_t maxBvhLevels) const {

    constexpr uint32_t maxNumDSSRTStacks = 2048u;
    constexpr uint32_t maxSyncNumDSSRTStacks = 4096u;

    rtDispatchGlobals.numDSSRTStacks = std::min(rtStacksPerDss, maxNumDSSRTStacks);
    rtDispatchGlobals.syncNumDSSRTStacks = std::min(rtStacksPerDss, maxSyncNumDSSRTStacks);
    rtDispatchGlobals.flags = RayTracingHelper::depthTestLessEqualFlag;

    if (maxBvhLevels >= 8) {
        maxBvhLevels = 0;
    }
    rtDispatchGlobals.maxBVHLevels = maxBvhLevels;
}

template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return false;
}

} // namespace NEO
