/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/cache_info.h"
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

    auto &hwHelper = HwHelper::get(drm.context.hwInfo->platform.eRenderCoreFamily);

    drm.setupCacheInfo(*defaultHwInfo.get());

    auto cacheInfo = drm.getCacheInfo();
    EXPECT_NE(nullptr, cacheInfo);

    if (hwHelper.getNumCacheRegions() == 0) {
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
        const size_t maxReservationNumCacheRegions = hwHelper.getNumCacheRegions() - 1;

        EXPECT_EQ(maxReservationCacheSize, cacheInfo->getMaxReservationCacheSize());
        EXPECT_EQ(maxReservationNumCacheRegions, cacheInfo->getMaxReservationNumCacheRegions());
        EXPECT_EQ(maxReservationNumWays, cacheInfo->getMaxReservationNumWays());
    }
}

TEST(DrmCacheInfoTest, givenDebugFlagSetWhenCallingSetUpCacheInfoThenCacheInfoIsCreatedWithoutValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ClosEnabled.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.setupCacheInfo(*defaultHwInfo.get());
    EXPECT_NE(nullptr, drm.getCacheInfo());
    auto cacheInfo = drm.getCacheInfo();

    EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize());
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions());
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays());
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionSucceedsToReserveCacheRegionThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::Region1));

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionFailsToReserveCacheRegionThenReturnFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    drm.context.closIndex = 0xFFFF;
    EXPECT_FALSE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::Region1));

    EXPECT_EQ(CacheRegion::None, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWithReservedCacheRegionWhenGetCacheRegionIsCalledForReservedCacheRegionThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32);
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.reserveCacheRegion(cacheReservationSize));

    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::Region1));

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionIsCalledForReservableRegionsWithRegionSizesInverselyProportionalToNumCacheRegionsThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    CacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32);
    size_t regionSize = cacheInfo.getMaxReservationCacheSize() / cacheInfo.getMaxReservationNumCacheRegions();

    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::Region1));
    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::Region2));

    EXPECT_EQ(CacheRegion::Region2, cacheInfo.freeCacheRegion(CacheRegion::Region2));
    EXPECT_EQ(CacheRegion::Region1, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWhenSpecificNumCacheWaysIsRequestedThenReserveAppropriateCacheSize) {
    constexpr uint16_t maxNumCacheWays = 32;

    DebugManagerStateRestore restorer;
    DebugManager.flags.ClosNumCacheWays.set(maxNumCacheWays / 2);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, maxNumCacheWays);
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::Region1, maxReservationCacheSize));

    auto cacheRegion = cacheInfo.cacheRegionsReserved.begin();
    EXPECT_EQ(CacheRegion::Region1, cacheRegion->first);
    EXPECT_EQ(maxReservationCacheSize / 2, cacheRegion->second);

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}

TEST(DrmCacheInfoTest, givenCacheInfoWhenNumCacheWaysIsExceededThenDontReserveCacheRegion) {
    constexpr uint16_t maxNumCacheWays = 32;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, maxNumCacheWays);
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize();

    EXPECT_EQ(CacheRegion::Region1, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::Region1, maxReservationCacheSize));

    EXPECT_EQ(CacheRegion::None, cacheInfo.reserveCacheRegion(maxReservationCacheSize));
    EXPECT_FALSE(cacheInfo.isRegionReserved(CacheRegion::Region2, maxReservationCacheSize));
}

TEST(DrmCacheInfoTest, givenCacheInfoCreatedWhenFreeCacheRegionIsCalledForNonReservedRegionThenItFails) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmQueryMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockCacheInfo cacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32);

    cacheInfo.cacheRegionsReserved.insert({CacheRegion::Region1, MemoryConstants::kiloByte});
    EXPECT_EQ(CacheRegion::None, cacheInfo.freeCacheRegion(CacheRegion::Region1));
}
