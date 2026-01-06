/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy_from_xe_hpg_to_xe3.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3_core/hw_cmds_xe3_core.h"

namespace NEO {
#ifdef SUPPORT_PTL
template <>
uint32_t L1CachePolicyHelper<IGFX_PTL>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_PTL>::GfxFamily;
    if (isDebuggerActive) {
        return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP;
    }
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB;
}

template struct L1CachePolicyHelper<IGFX_PTL>;
static EnableGfxProductHw<IGFX_PTL> enableGfxProductHwPTL;
#endif

#ifdef SUPPORT_NVLS
template struct L1CachePolicyHelper<IGFX_NVL_XE3G>;
static EnableGfxProductHw<IGFX_NVL_XE3G> enableGfxProductHwNVLS;
#endif
} // namespace NEO
