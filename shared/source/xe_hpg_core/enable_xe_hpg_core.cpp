/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_from_xe_hpg_to_xe3.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {
#ifdef SUPPORT_DG2

template <>
uint32_t L1CachePolicyHelper<IGFX_DG2>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_DG2>::GfxFamily;
    if (isDebuggerActive) {
        return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP;
    }
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB;
}

template struct L1CachePolicyHelper<IGFX_DG2>;
static EnableGfxProductHw<IGFX_DG2> enableGfxProductHwDG2;

#endif

#ifdef SUPPORT_MTL
template struct L1CachePolicyHelper<IGFX_METEORLAKE>;
static EnableGfxProductHw<IGFX_METEORLAKE> enableGfxProductHwMTL;
#endif

#ifdef SUPPORT_ARL
template struct L1CachePolicyHelper<IGFX_ARROWLAKE>;
static EnableGfxProductHw<IGFX_ARROWLAKE> enableGfxProductHwARL;
#endif

} // namespace NEO
