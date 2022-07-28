/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "igfxfmid.h"

#include <cstdint>
#include <stddef.h>

namespace NEO {
class GraphicsAllocation;
bool isL3Capable(void *ptr, size_t size);
bool isL3Capable(const GraphicsAllocation &graphicsAllocation);

template <PRODUCT_FAMILY gfxProduct>
struct L1CachePolicyHelper {

    static const char *getCachingPolicyOptions();

    static uint32_t getDefaultL1CachePolicy() {
        return 0u;
    }

    static uint32_t getL1CachePolicy() {
        if (DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get() != -1) {
            return DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get();
        }
        return L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy();
    }
};

} // namespace NEO
