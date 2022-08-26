/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bit_helpers.h"

#include <cstdint>

namespace NEO {

namespace FlushSpecificCacheHelper {

static constexpr bool isDcFlushEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 0);
}

static constexpr bool isRenderTargetCacheFlushEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 1);
}

static constexpr bool isInstructionCacheInvalidateEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 2);
}

static constexpr bool isTextureCacheInvalidationEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 3);
}

static constexpr bool isPipeControlFlushEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 4);
}

static constexpr bool isVfCacheInvalidationEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 5);
}

static constexpr bool isConstantCacheInvalidationEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 6);
}

static constexpr bool isStateCacheInvalidationEnableSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 7);
}

static constexpr bool isTlbInvalidationSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 8);
}

static constexpr bool isHdcPipelineFlushSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 9);
}

static constexpr bool isUnTypedDataPortCacheFlushSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 10);
}

static constexpr bool isCompressionControlSurfaceCcsFlushSet(const int32_t flushSpecificCache) {
    return isBitSet(flushSpecificCache, 11);
}

} // namespace FlushSpecificCacheHelper

} // namespace NEO