/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
const char *L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions() {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;

    static constexpr const char *writeBackCachingPolicy = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    static constexpr const char *writeByPassCachingPolicy = "-cl-store-cache-default=2 -cl-load-cache-default=4";

    switch (L1CachePolicyHelper<gfxProduct>::getL1CachePolicy()) {
    case GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP:
        return writeByPassCachingPolicy;
    case GfxFamily::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB:
        return writeBackCachingPolicy;
    default:
        return nullptr;
    }
}

} // namespace NEO
