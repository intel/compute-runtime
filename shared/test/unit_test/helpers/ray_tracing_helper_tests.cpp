/*
 * Copyright (C) 2020-2025 Intel Corporation
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
    MockDevice device;

    uint32_t maxBvhLevel = 2;
    uint32_t extraBytesLocal = 20;
    uint32_t extraBytesGlobal = 100;
    uint32_t tiles = 2;
    auto numRtStacksPerDss = device.getProductHelper().getNumRtStacksPerDSSForAllocation(device.getHardwareInfo());
    size_t expectedSize = alignUp(RayTracingHelper::getStackSizePerRay(maxBvhLevel, extraBytesLocal) * RayTracingHelper::getNumRtStacks(device, numRtStacksPerDss) + extraBytesGlobal, MemoryConstants::cacheLineSize);
    size_t size = RayTracingHelper::getRTStackSizePerTile(device, tiles, maxBvhLevel, extraBytesLocal, extraBytesGlobal);
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenNumRtStacksIsQueriedThenItIsEqualToNumRtStacksPerDssMultipliedByDualSubsliceCount) {
    MockDevice device;

    uint32_t numDssRtStacksPerDss = device.getProductHelper().getNumRtStacksPerDSSForAllocation(device.getHardwareInfo());
    uint32_t numDssRtStacks = RayTracingHelper::getNumRtStacks(device, numDssRtStacksPerDss);
    uint32_t subsliceCount = GfxCoreHelper::getHighestEnabledDualSubSlice(device.getHardwareInfo());

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

TEST(RayTracingHelperTests, whenNumRtStacksPerDssIsRequestedAndFixedValueIsFalseThenCorrectValueIsReturned) {

    MockDevice device;
    uint32_t maxEuPerSubSlice = 16;
    uint32_t threadCount = 672;
    uint32_t euCount = 96;

    auto &hwInfo = *device.getRootDeviceEnvironmentRef().getMutableHardwareInfo();
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = maxEuPerSubSlice;
    hwInfo.gtSystemInfo.ThreadCount = threadCount;
    hwInfo.gtSystemInfo.EUCount = euCount;

    auto numRtStackPerDss = device.getProductHelper().getSyncNumRTStacksPerDss(device.getHardwareInfo());
    if (numRtStackPerDss > 0) {
        // maxEuPerSubSlice * (threadCount / euCount) * CommonConstants::maximalSimdSize = 3584u
        constexpr uint32_t expectedValue = 3584;
        EXPECT_EQ(expectedValue, numRtStackPerDss);
    }
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssExceedsMaxThenReturnsMaxRtStacksPerDssSupported) {

    MockDevice device;
    uint32_t maxSizeOfRtStacksPerDss = 4096;

    auto &hwInfo = *device.getRootDeviceEnvironmentRef().getMutableHardwareInfo();
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 512;
    hwInfo.gtSystemInfo.ThreadCount = 2048;
    hwInfo.gtSystemInfo.EUCount = 256;

    auto numRtStackPerDss = device.getProductHelper().getSyncNumRTStacksPerDss(device.getHardwareInfo());
    if (numRtStackPerDss > 0) {
        uint32_t uncappedRtStacksPerDss = hwInfo.gtSystemInfo.MaxEuPerSubSlice * (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) * CommonConstants::maximalSimdSize;
        EXPECT_GT(uncappedRtStacksPerDss, maxSizeOfRtStacksPerDss);
        EXPECT_EQ(maxSizeOfRtStacksPerDss, numRtStackPerDss);
    }
}
