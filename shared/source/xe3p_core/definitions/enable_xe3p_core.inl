/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef SUPPORT_CRI
template <>
uint32_t L1CachePolicyHelper<NEO::criProductEnumValue>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = HwMapper<NEO::criProductEnumValue>::GfxFamily;
    return GfxFamily::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;
}

template struct L1CachePolicyHelper<NEO::criProductEnumValue>;
static EnableGfxProductHw<NEO::criProductEnumValue> enableGfxProductHwCRI;
#endif
