/*
 * Copyright (C) 2022-2025 Intel Corporation
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
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct DrmCacheInfoFixture {
    static constexpr uint16_t maxNumCacheWays{32};

    void setUp() {
        executionEnvironment.reset(new MockExecutionEnvironment{});
        drm.reset(new DrmQueryMock{*executionEnvironment->rootDeviceEnvironments[0]});

        l2CacheParameters.maxSize = 1;
        l2CacheParameters.maxNumRegions = 1;
        l2CacheParameters.maxNumWays = 2;

        l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
        l3CacheParameters.maxNumRegions = 2;
        l3CacheParameters.maxNumWays = maxNumCacheWays;
    }

    void tearDown() {
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment{nullptr};
    std::unique_ptr<DrmQueryMock> drm{nullptr};
    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
};
using DrmCacheInfoTest = Test<DrmCacheInfoFixture>;

TEST_F(DrmCacheInfoTest, givenCacheRegionsExistsWhenCallingSetUpCacheInfoThenCacheInfoIsCreatedAndReturnsMaxReservationCacheLimits) {
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    drm->setupCacheInfo(*defaultHwInfo.get());

    auto cacheInfo = drm->getCacheInfo();
    EXPECT_NE(nullptr, cacheInfo);
    constexpr auto cacheLevel{CacheLevel::level3};

    if (productHelper.getNumCacheRegions() == 0) {
        EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize(cacheLevel));
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel));
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays(cacheLevel));
    } else {
        const GT_SYSTEM_INFO *gtSysInfo = &defaultHwInfo->gtSystemInfo;
        constexpr uint16_t maxNumWays = 32;
        constexpr uint16_t globalReservationLimit = 16;
        constexpr uint16_t clientReservationLimit = 8;
        constexpr uint16_t maxReservationNumWays = std::min(globalReservationLimit, clientReservationLimit);
        const size_t totalCacheSize = gtSysInfo->L3CacheSizeInKb * MemoryConstants::kiloByte;
        const size_t maxReservationCacheSize = (totalCacheSize * maxReservationNumWays) / maxNumWays;
        const size_t maxReservationNumCacheRegions = productHelper.getNumCacheRegions() - 1;

        EXPECT_EQ(maxReservationCacheSize, cacheInfo->getMaxReservationCacheSize(cacheLevel));
        EXPECT_EQ(maxReservationNumCacheRegions, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel));
        EXPECT_EQ(maxReservationNumWays, cacheInfo->getMaxReservationNumWays(cacheLevel));
    }
}

TEST_F(DrmCacheInfoTest, givenDebugFlagSetWhenCallingSetUpCacheInfoThenCacheInfoIsCreatedWithoutValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ClosEnabled.set(0);

    drm->setupCacheInfo(*defaultHwInfo.get());
    EXPECT_NE(nullptr, drm->getCacheInfo());
    auto cacheInfo = drm->getCacheInfo();

    constexpr auto cacheLevel3{CacheLevel::level3};
    EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize(cacheLevel3));
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel3));
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays(cacheLevel3));

    constexpr auto cacheLevel2{CacheLevel::level2};
    EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize(cacheLevel2));
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel2));
    EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays(cacheLevel2));
}

TEST_F(DrmCacheInfoTest, givenDebugFlagSetWhenCallingSetUpCacheInfoThenL2CacheInfoIsCreatedAccordingly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.L2ClosNumCacheWays.set(2);

    drm->setupCacheInfo(*defaultHwInfo.get());
    EXPECT_NE(nullptr, drm->getCacheInfo());
    auto cacheInfo = drm->getCacheInfo();

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    constexpr auto cacheLevel2{CacheLevel::level2};
    if (productHelper.getNumCacheRegions() > 0) {
        EXPECT_EQ(1u, cacheInfo->getMaxReservationCacheSize(cacheLevel2));
        EXPECT_EQ(1u, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel2));
        EXPECT_EQ(2u, cacheInfo->getMaxReservationNumWays(cacheLevel2));
    } else {
        EXPECT_EQ(0u, cacheInfo->getMaxReservationCacheSize(cacheLevel2));
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumCacheRegions(cacheLevel2));
        EXPECT_EQ(0u, cacheInfo->getMaxReservationNumWays(cacheLevel2));
    }
}

TEST_F(DrmCacheInfoTest, givenDebugFlagSetWhenCallingSetUpCacheInfoThenL2CacheInfoIsCreatedAndRegionReserved) {
    constexpr auto staticL2CacheReservationSize{1U};
    constexpr auto staticL2CacheNumWays{2U};

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceStaticL2ClosReservation.set(true);

    auto mockIoctlHelper{new MockIoctlHelper{*drm}};
    drm->ioctlHelper.reset(mockIoctlHelper);

    mockIoctlHelper->closAllocResult = CacheRegion::region3;
    mockIoctlHelper->closAllocWaysResult = staticL2CacheNumWays;

    drm->setupCacheInfo(*defaultHwInfo.get());
    EXPECT_NE(nullptr, drm->getCacheInfo());

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto *mockCacheInfo{static_cast<MockCacheInfo *>(drm->getCacheInfo())};
    if (productHelper.getNumCacheRegions() > 0U) {
        EXPECT_TRUE(mockCacheInfo->isRegionReserved(CacheRegion::region3, staticL2CacheReservationSize));
    } else {
        EXPECT_FALSE(mockCacheInfo->isRegionReserved(CacheRegion::region3, staticL2CacheReservationSize));
    }
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionSucceedsToReserveL3CacheRegionThenReturnTrue) {
    CacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));
    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionFailsToReserveCacheRegionThenReturnFalse) {
    CacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    drm->context.closIndex = 0xFFFF;
    EXPECT_FALSE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));
    EXPECT_EQ(CacheRegion::none, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoWithAnyLimitParameterEqualToZeroWhenReservationIsTriedThenCacheRegionNoneIsReturned) {
    MockCacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    const auto maxSizeOrig = cacheInfo.l3ReservationLimits.maxSize;
    cacheInfo.l3ReservationLimits.maxSize = 0;
    EXPECT_EQ(CacheRegion::none, cacheInfo.reserveCacheRegion(cacheLevel, cacheReservationSize));
    cacheInfo.l3ReservationLimits.maxSize = maxSizeOrig;

    const auto maxNumWaysOrig = cacheInfo.l3ReservationLimits.maxNumWays;
    cacheInfo.l3ReservationLimits.maxNumWays = 0;
    EXPECT_EQ(CacheRegion::none, cacheInfo.reserveCacheRegion(cacheLevel, cacheReservationSize));
    cacheInfo.l3ReservationLimits.maxNumWays = maxNumWaysOrig;

    const auto maxNumRegionsOrig = cacheInfo.l3ReservationLimits.maxNumRegions;
    cacheInfo.l3ReservationLimits.maxNumRegions = 0;
    EXPECT_EQ(CacheRegion::none, cacheInfo.reserveCacheRegion(cacheLevel, cacheReservationSize));
    cacheInfo.l3ReservationLimits.maxNumRegions = maxNumRegionsOrig;
}

TEST_F(DrmCacheInfoTest, givenCacheInfoWithReservedCacheRegionWhenGetCacheRegionIsCalledForReservedCacheRegionThenReturnTrue) {
    CacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t cacheReservationSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(cacheLevel, cacheReservationSize));
    EXPECT_TRUE(cacheInfo.getCacheRegion(cacheReservationSize, CacheRegion::region1));
    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionIsCalledForReservableRegionsWithRegionSizesInverselyProportionalToNumCacheRegionsThenReturnTrue) {
    CacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t regionSize = cacheInfo.getMaxReservationCacheSize(cacheLevel) / cacheInfo.getMaxReservationNumCacheRegions(cacheLevel);

    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::region1));
    EXPECT_TRUE(cacheInfo.getCacheRegion(regionSize, CacheRegion::region2));

    EXPECT_EQ(CacheRegion::region2, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region2));
    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoWhenSpecificNumCacheWaysIsRequestedThenReserveAppropriateCacheSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ClosNumCacheWays.set(maxNumCacheWays / 2);

    MockCacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(cacheLevel, maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::region1, maxReservationCacheSize));

    auto expectedRegionSize = cacheInfo.reservedCacheRegionsSize[toUnderlying(CacheRegion::region1)];
    EXPECT_EQ(maxReservationCacheSize / 2, expectedRegionSize);

    EXPECT_EQ(CacheRegion::region1, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoWhenNumCacheWaysIsExceededThenDontReserveCacheRegion) {
    MockCacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    size_t maxReservationCacheSize = cacheInfo.getMaxReservationCacheSize(cacheLevel);

    EXPECT_EQ(CacheRegion::region1, cacheInfo.reserveCacheRegion(cacheLevel, maxReservationCacheSize));
    EXPECT_TRUE(cacheInfo.isRegionReserved(CacheRegion::region1, maxReservationCacheSize));

    EXPECT_EQ(CacheRegion::none, cacheInfo.reserveCacheRegion(cacheLevel, maxReservationCacheSize));
    EXPECT_FALSE(cacheInfo.isRegionReserved(CacheRegion::region2, maxReservationCacheSize));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenFreeCacheRegionIsCalledForNonReservedRegionThenItFails) {
    MockCacheInfo cacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters);

    constexpr auto cacheLevel{CacheLevel::level3};
    cacheInfo.reservedCacheRegionsSize[toUnderlying(CacheRegion::region1)] = MemoryConstants::kiloByte;
    EXPECT_EQ(CacheRegion::none, cacheInfo.freeCacheRegion(cacheLevel, CacheRegion::region1));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenPassingRegionIndexThenCorrectCacheLevelReturned) {
    auto *cacheInfo{new MockCacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters)};
    drm->cacheInfo.reset(cacheInfo);

    EXPECT_EQ(CacheLevel::level3, cacheInfo->getLevelForRegion(CacheRegion::region1));
    EXPECT_EQ(CacheLevel::level3, cacheInfo->getLevelForRegion(CacheRegion::region2));
    EXPECT_EQ(CacheLevel::level2, cacheInfo->getLevelForRegion(CacheRegion::region3));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenAnyLevelLimitIsZeroThenNoRegionCanBeReserved) {
    constexpr auto anyNonZeroValue{42U};

    auto *cacheInfo{new MockCacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters)};
    drm->cacheInfo.reset(cacheInfo);

    l2CacheParameters.maxSize = 0U;
    EXPECT_EQ(CacheRegion::none, cacheInfo->reserveCacheRegion(CacheLevel::level2, anyNonZeroValue));
    EXPECT_FALSE(cacheInfo->isRegionReserved(CacheRegion::region3, anyNonZeroValue));

    l2CacheParameters.maxSize = anyNonZeroValue;
    l2CacheParameters.maxNumWays = 0U;
    EXPECT_EQ(CacheRegion::none, cacheInfo->reserveCacheRegion(CacheLevel::level2, anyNonZeroValue));
    EXPECT_FALSE(cacheInfo->isRegionReserved(CacheRegion::region3, anyNonZeroValue));

    l2CacheParameters.maxNumWays = anyNonZeroValue;
    l2CacheParameters.maxNumRegions = 0U;
    EXPECT_EQ(CacheRegion::none, cacheInfo->reserveCacheRegion(CacheLevel::level2, anyNonZeroValue));
    EXPECT_FALSE(cacheInfo->isRegionReserved(CacheRegion::region3, anyNonZeroValue));
}

TEST_F(DrmCacheInfoTest, givenCacheInfoCreatedWhenGetCacheRegionSucceedsToReserveL2CacheRegionThenReturnTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.L2ClosNumCacheWays.set(2);

    MockIoctlHelper ioctlHelper{*drm};
    auto *cacheInfo{new MockCacheInfo(ioctlHelper, l2CacheParameters, l3CacheParameters)};
    drm->cacheInfo.reset(cacheInfo);

    constexpr auto cacheLevel{CacheLevel::level2};
    size_t cacheReservationSize = cacheInfo->getMaxReservationCacheSize(cacheLevel);

    ioctlHelper.closAllocResult = CacheRegion::region3;
    ioctlHelper.closAllocWaysResult = l2CacheParameters.maxNumWays;

    EXPECT_TRUE(cacheInfo->getCacheRegion(cacheReservationSize, CacheRegion::region3));
    EXPECT_EQ(cacheInfo->reservedCacheRegionsSize[toUnderlying(CacheRegion::region3)], cacheReservationSize);
    EXPECT_EQ(ioctlHelper.closAllocWaysCalled, 1U);

    ioctlHelper.closFreeResult = CacheRegion::region3;
    EXPECT_EQ(cacheInfo->freeCacheRegion(cacheLevel, CacheRegion::region3), CacheRegion::region3);
    EXPECT_EQ(cacheInfo->reservedCacheRegionsSize[toUnderlying(CacheRegion::region3)], 0U);
    EXPECT_EQ(cacheInfo->freeCacheRegion(cacheLevel, CacheRegion::region3), CacheRegion::none);

    EXPECT_EQ(ioctlHelper.closAllocCalled, 1U);
    EXPECT_EQ(ioctlHelper.closAllocWaysCalled, 2U);
    EXPECT_EQ(ioctlHelper.closFreeCalled, 1U);
}
