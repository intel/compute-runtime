/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/cache_policy_base.inl"
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
const char *L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions(bool isDebuggerActive) {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;

    static constexpr const char *writeBackCachingPolicy = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    static constexpr const char *writeByPassCachingPolicy = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    static constexpr const char *uncachedCachingPolicy = "-cl-store-cache-default=2 -cl-load-cache-default=2";

    switch (L1CachePolicyHelper<gfxProduct>::getL1CachePolicy(isDebuggerActive)) {
    case GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP:
        return writeByPassCachingPolicy;
    case GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB:
        return writeBackCachingPolicy;
    case GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC:
        return uncachedCachingPolicy;
    default:
        return nullptr;
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getUncachedL1CachePolicy() {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC;
}

} // namespace NEO
