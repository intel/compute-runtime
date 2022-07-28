/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
const char *L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions() {
    return nullptr;
}

} // namespace NEO
