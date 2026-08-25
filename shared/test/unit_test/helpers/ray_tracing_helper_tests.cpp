/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(RayTracingHelperTests, whenGetMemoryBackedFifoSizeToPatchIsCalledCorrectValueIsReturned) {
    size_t fifoSize = RayTracingHelper::getMemoryBackedFifoSizeToPatch();
    size_t expectedSize =
        RayTracingHelper::memoryBackedFifoSizePerDss == 0
            ? 0
            : Math::log2(RayTracingHelper::memoryBackedFifoSizePerDss / MemoryConstants::kiloByte) - 1;
    EXPECT_EQ(expectedSize, fifoSize);
}

TEST(RayTracingHelperTests, whenMemoryBackedFifoSizeIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    size_t size = RayTracingHelper::getTotalMemoryBackedFifoSize(device);
    uint32_t subSliceCount = GfxCoreHelper::getHighestEnabledDualSubSlice(device.getHardwareInfo());
    size_t expectedSize = subSliceCount * RayTracingHelper::memoryBackedFifoSizePerDss;
    EXPECT_LT(0u, size);
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenRTStackSizeIsRequestedThenCorrectValueIsReturned) {
    uint32_t maxBvhLevel = 2;
    uint32_t extraBytesLocal = 20;
    uint32_t extraBytesGlobal = 100;
    uint32_t tiles = 2;

    size_t expectedSize = alignUp(RayTracingHelper::getStackSizePerRay(maxBvhLevel, extraBytesLocal) * RayTracingHelper::getNumRtStacks(*defaultHwInfo) + extraBytesGlobal, MemoryConstants::cacheLineSize);
    size_t size = RayTracingHelper::getRTStackSizePerTile(*defaultHwInfo, tiles, maxBvhLevel, extraBytesLocal, extraBytesGlobal);
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenNumRtStacksIsQueriedThenItIsEqualToNumRtStacksPerDssMultipliedByDualSubsliceCount) {
    uint32_t numDssRtStacksPerDss = RayTracingHelper::getNumRtStacksPerDss(*defaultHwInfo);
    uint32_t numDssRtStacks = RayTracingHelper::getNumRtStacks(*defaultHwInfo);
    uint32_t subsliceCount = GfxCoreHelper::getHighestEnabledDualSubSlice(*defaultHwInfo);

    EXPECT_LT(0u, numDssRtStacks);
    EXPECT_EQ(numDssRtStacks, numDssRtStacksPerDss * subsliceCount);
}

TEST(RayTracingHelperTests, whenStackSizePerRayIsRequestedThenCorrectValueIsReturned) {

    EXPECT_EQ(RayTracingHelper::hitInfoSize, RayTracingHelper::getStackSizePerRay(0, 0));

    uint32_t maxBvhLevel = 1234;
    uint32_t extraBytesLocal = 5678;

    uint32_t expectedValue = RayTracingHelper::hitInfoSize + RayTracingHelper::bvhStackSize * maxBvhLevel + extraBytesLocal;
    EXPECT_EQ(RayTracingHelper::getStackSizePerRay(maxBvhLevel, extraBytesLocal), expectedValue);
}

TEST(RayTracingHelperTests, whenGetMemoryBackedFifoSizeToPatchIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(2u, RayTracingHelper::getMemoryBackedFifoSizeToPatch());
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssIsRequestedAndFixedValueIsTrueThenCorrectValueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.caps.numRtStacksPerDssFixedValue = true;

    uint32_t fixedSizeOfRtStacksPerDss = 2048;
    uint32_t result = RayTracingHelper::getNumRtStacksPerDss(hwInfo);
    EXPECT_EQ(fixedSizeOfRtStacksPerDss, result);
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssIsRequestedAndFixedValueIsFalseThenCorrectValueIsReturned) {
    uint32_t maxEuPerSubSlice = 16;
    uint32_t threadCount = 672;
    uint32_t euCount = 96;
    uint32_t numThreadsPerEu = threadCount / euCount;

    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = maxEuPerSubSlice;
    hwInfo.gtSystemInfo.ThreadCount = threadCount;
    hwInfo.gtSystemInfo.EUCount = euCount;
    hwInfo.gtSystemInfo.NumThreadsPerEu = numThreadsPerEu;
    hwInfo.caps.numRtStacksPerDssFixedValue = false;

    // maxEuPerSubSlice * (threadCount / euCount) * CommonConstants::maximalSimdSize = 3584u
    constexpr uint32_t expectedValue = 3584;

    EXPECT_EQ(expectedValue, RayTracingHelper::getNumRtStacksPerDss(hwInfo));
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssExceedsMaxThenReturnsMaxRtStacksPerDssSupported) {
    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 512;
    hwInfo.gtSystemInfo.ThreadCount = 2048;
    hwInfo.gtSystemInfo.EUCount = 256;
    hwInfo.gtSystemInfo.NumThreadsPerEu = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    hwInfo.caps.numRtStacksPerDssFixedValue = false;

    uint32_t maxSizeOfRtStacksPerDss = 4096;
    uint32_t result = RayTracingHelper::getNumRtStacksPerDss(hwInfo);
    EXPECT_EQ(maxSizeOfRtStacksPerDss, result);
}
