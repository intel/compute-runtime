/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class GraphicsAllocation;
class ProductHelper;

bool isL3Capable(const void *ptr, size_t size);
bool isL3Capable(const GraphicsAllocation &graphicsAllocation);

template <PRODUCT_FAMILY gfxProduct>
struct L1CachePolicyHelper {

    static const char *getCachingPolicyOptions(bool isDebuggerActive);

    static uint32_t getDefaultL1CachePolicy(bool isDebuggerActive);

    static uint32_t getUncachedL1CachePolicy();

    static uint32_t getL1CachePolicy(bool isDebuggerActive);
};

struct L1CachePolicy {
    L1CachePolicy() = default;
    L1CachePolicy(const ProductHelper &helper) {
        init(helper);
    }
    void init(const ProductHelper &helper);
    uint32_t getL1CacheValue(bool isDebuggerActive) {
        return isDebuggerActive ? defaultDebuggerActive : defaultDebuggerInactive;
    }

  protected:
    uint32_t defaultDebuggerActive = 0;
    uint32_t defaultDebuggerInactive = 0;
};

} // namespace NEO
