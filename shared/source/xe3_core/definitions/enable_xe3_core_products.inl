/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
template struct L1CachePolicyHelper<nvlsProductEnumValue>;
static EnableGfxProductHw<nvlsProductEnumValue> enableGfxProductHwNVLS;
#endif
