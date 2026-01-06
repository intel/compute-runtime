/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_CRI
template <>
uint32_t L1CachePolicyHelper<IGFX_CRI>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<IGFX_CRI>::GfxFamily;
    return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;
}

template struct L1CachePolicyHelper<IGFX_CRI>;
static EnableGfxProductHw<IGFX_CRI> enableGfxProductHwCRI;
#endif
