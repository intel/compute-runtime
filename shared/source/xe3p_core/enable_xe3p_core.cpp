/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy_from_xe3p_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"

#include "hw_cmds_xe3p_core.h"

namespace NEO {

#ifdef SUPPORT_CRI
template <>
uint32_t L1CachePolicyHelper<IGFX_CRI>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_CRI>::GfxFamily;
    if (isDebuggerActive) {
        return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;
    }
    return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WS;
}
#endif

#include "enable_xe3p_core.inl"

} // namespace NEO
