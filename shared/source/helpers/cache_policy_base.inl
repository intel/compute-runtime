/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getL1CachePolicy(bool isDebuggerActive) {
    if (debugManager.flags.ForceAllResourcesUncached.get()) {
        return L1CachePolicyHelper<gfxProduct>::getUncachedL1CachePolicy();
    }
    if (debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get() != -1) {
        return debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.get();
    }
    return L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy(isDebuggerActive);
}

} // namespace NEO