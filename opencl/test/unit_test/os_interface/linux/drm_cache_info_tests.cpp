/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info_impl.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenCallingGetCacheRegionThenReturnZero) {
    CacheInfoImpl cacheInfo;

    EXPECT_FALSE(cacheInfo.getCacheRegion(1024, CacheRegion::Default));
}
