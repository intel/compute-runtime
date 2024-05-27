/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_caching_policy_options.inl"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getUncachedL1CachePolicy() {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC;
}

} // namespace NEO
