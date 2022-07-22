/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy() {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getL1CachePolicy() {
    if (DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get() != -1) {
        return DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get();
    }
    return L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy();
}

} // namespace NEO
