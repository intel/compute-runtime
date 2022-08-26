/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flush_specific_cache_helper.h"

#include "gtest/gtest.h"

#include <bitset>

using namespace NEO;

TEST(FlushSpecificCacheHelperTests, WhenSettingFlushSpecificCachesIndexThenCorrectResultIsReturned) {
    static constexpr size_t flushSpecificCacheMaskSize = 12u;

    std::bitset<flushSpecificCacheMaskSize> flushSpecificCaches = 0b000000000001;
    EXPECT_TRUE(FlushSpecificCacheHelper::isDcFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000000000010;
    EXPECT_TRUE(FlushSpecificCacheHelper::isRenderTargetCacheFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000000000100;
    EXPECT_TRUE(FlushSpecificCacheHelper::isInstructionCacheInvalidateEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000000001000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isTextureCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000000010000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isPipeControlFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000000100000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isVfCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000001000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isConstantCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000010000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isStateCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b000100000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isTlbInvalidationSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b001000000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isHdcPipelineFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b010000000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isUnTypedDataPortCacheFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));

    flushSpecificCaches = 0b100000000000;
    EXPECT_TRUE(FlushSpecificCacheHelper::isCompressionControlSurfaceCcsFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));
}

TEST(FlushSpecificCacheHelperTests, WhenSettingFlushSpecificCachesIndexesThenCorrectResultIsReturned) {
    static constexpr size_t flushSpecificCacheMaskSize = 12u;

    std::bitset<flushSpecificCacheMaskSize> flushSpecificCaches = 0b100000001011;

    EXPECT_TRUE(FlushSpecificCacheHelper::isDcFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_TRUE(FlushSpecificCacheHelper::isRenderTargetCacheFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isInstructionCacheInvalidateEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_TRUE(FlushSpecificCacheHelper::isTextureCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isPipeControlFlushEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isVfCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isConstantCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isStateCacheInvalidationEnableSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isTlbInvalidationSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isHdcPipelineFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_FALSE(FlushSpecificCacheHelper::isUnTypedDataPortCacheFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));
    EXPECT_TRUE(FlushSpecificCacheHelper::isCompressionControlSurfaceCcsFlushSet(static_cast<int>(flushSpecificCaches.to_ulong())));
}
