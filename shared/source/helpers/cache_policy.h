/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class GraphicsAllocation;
bool isL3Capable(void *ptr, size_t size);
bool isL3Capable(const GraphicsAllocation &graphicsAllocation);

template <PRODUCT_FAMILY gfxProduct>
struct L1CachePolicyHelper {

    static const char *getCachingPolicyOptions(bool isDebuggerActive);

    static uint32_t getDefaultL1CachePolicy(bool isDebuggerActive);

    static uint32_t getUncachedL1CachePolicy();

    static uint32_t getL1CachePolicy(bool isDebuggerActive);
};

} // namespace NEO
