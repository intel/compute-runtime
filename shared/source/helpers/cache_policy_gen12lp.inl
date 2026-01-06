/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/cache_policy_base.inl"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
const char *L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions(bool isDebuggerActive) {
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getDefaultL1CachePolicy(bool isDebuggerActive) {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t L1CachePolicyHelper<gfxProduct>::getUncachedL1CachePolicy() {
    return 1u;
}

} // namespace NEO
