/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmCacheInfoTest, givenCacheRegionsExistsWhenCallingSetUpCacheInfoThenCacheInfoIsCreatedAndReturnsMaxReservationCacheLimits) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    drm.setupCacheInfo(*defaultHwInfo.get());

    auto cacheInfo = drm.getL3CacheInfo();
    EXPECT_NE(nullptr, cacheInfo);

    if (productHelper.getNumCacheRegions() == 0) {
        EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize());
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions());
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays());
    } else {
        const GT_SYSTEM_INFO *gtSysInfo = &defaultHwInfo->gtSystemInfo;
        constexpr uint16_t maxNumWays = 32;
        constexpr uint16_t globalReservationLimit = 16;
        constexpr uint16_t clientReservationLimit = 8;
        constexpr uint16_t maxReservationNumWays = std::min(globalReservationLimit, clientReservationLimit);
        const size_t totalCacheSize = gtSysInfo->L3CacheSizeInKb * MemoryConstants::kiloByte;
        const size_t maxReservationCacheSize = (totalCacheSize * maxReservationNumWays) / maxNumWays;
        const size_t maxReservationNumCacheRegions = productHelper.getNumCacheRegions() - 1;

        EXPECT_EQ(maxReservationCacheSize, cacheInfo->getMaxReservationCacheSize());
        EXPECT_EQ(maxReservationNumCacheRegions, cacheInfo->getMaxReservationNumCacheRegions());
        EXPECT_EQ(maxReservationNumWays, cacheInfo->getMaxReservationNumWays());
    }
}

TEST(DrmCacheInfoTest, givenDebugFlagSetWhenCallingSetUpCacheInfoThenCacheInfoIsCreatedWithoutValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ClosEnabled.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.setupCacheInfo(*defaultHwInfo.get());
    EXPECT_NE(nullptr, drm.getL3CacheInfo());
    auto cacheInfo = drm.getL3CacheInfo();

    EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize());
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions());
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays());
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionSucceedsToReserveCacheRegionThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));

    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(CacheRegion::region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionFailsToReserveCacheRegionThenReturnFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    drm.context.closIndex = 0xFFFF;
    EXPECT_FALSE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));

    EXPECT_EQ(CacheRegion::none, cacheInfo.freeCacheRegion(CacheRegion::region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWithReservedCacheRegionWhenGetCacheRegionIsCalledForReservedCacheRegionThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(cacheReservationSize));

    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));

    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(CacheRegion::region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionIsCalledForReservableRegionsWithRegionSizesInverselyProportionalToNumCacheRegionsThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, 32);
    size_t regionSize = cacheInfo.getMaxReservationCacheSize() / cacheInfo.getMaxReservationNumCacheRegions();

    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::region1));
    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::region2));

    EXPECT_EQ(CacheRegion::region2, cacheInfo.freeCacheRegion(CacheRegion::region2));
    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(CacheRegion::region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWhenSpecificNumCacheWaysIsRequestedThenReserveAppropriateCacheSize) {
    constexpr uint16_t maxNumCacheWays = 32;

    DebugManagerStateRestore restorer;
    debugManager.flags.ClosNumCacheWays.set(maxNumCacheWays / 2);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, maxNumCacheWays);
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::region1, maxReservationCacheSize));

    auto expectedRegionSize = cacheInfo.reservedCacheRegionsSize[toUnderlying(CacheRegion::region1)];
    EXPECT_EQ(maxReservationCacheSize / 2, expectedRegionSize);

    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(CacheRegion::region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWhenNumCacheWaysIsExceededThenDontReserveCacheRegion) {
    constexpr uint16_t maxNumCacheWays = 32;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, maxNumCacheWays);
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::region1, maxReservationCacheSize));

    EXPECT_EQ(CacheRegion::none, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_FALSE(cacheInfo.isRegionReserved(CacheRegion::region2, maxReservationCacheSize));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenFreeCacheRegionIsCalledForNonReservedRegionThenItFails) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(*drm.getIoctlHelper(), 32 * MemoryConstants::kiloByte, 2, 32);

    cacheInfo.reservedCacheRegionsSize[toUnderlying(CacheRegion::region1)] = MemoryConstants::kiloByte;
    EXPECT_EQ(CacheRegion::none, cacheInfo.freeCacheRegion(CacheRegion::region1));
}
