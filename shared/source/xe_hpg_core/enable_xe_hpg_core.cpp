/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy_dg2_and_later.inl"
#include "shared/source/helpers/enable_product.inl"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {
#ifdef SUPPORT_DG2

template <>
uint32_t L1CachePolicyHelper<IGFX_DG2>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_DG2>::GfxFamily;
    if (isDebuggerActive) {
        return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP;
    }
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB;
}

template struct L1CachePolicyHelper<IGFX_DG2>;
static EnableGfxProductHw<IGFX_DG2> enableGfxProductHwDG2;

#endif

#ifdef SUPPORT_MTL
template struct L1CachePolicyHelper<IGFX_METEORLAKE>;
static EnableGfxProductHw<IGFX_METEORLAKE> enableGfxProductHwMTL;
#endif

} // namespace NEO
