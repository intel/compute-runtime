/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getL1CachePolicy(bool isDebuggerActive) {
    if (DebugManager.flags.ForceAllResourcesUncached.get()) {
        return L1CachePolicyHelper<gfxProduct>::getUncachedL1CachePolicy();
    }
    if (DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get() != -1) {
        return DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get();
    }
    return L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy(isDebuggerActive);
}

} // namespace NEO