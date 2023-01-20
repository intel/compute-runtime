/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(RayTracingHelperTests, whenGetMemoryBackedFifoSizeToPatchIsCalledCorrectValueIsReturned) {
    size_t fifoSize = RayTracingHelper::getMemoryBackedFifoSizeToPatch();
    size_t expectedSize =
        RayTracingHelper::memoryBackedFifoSizePerDss == 0
            ? 0
            : Math::log2(RayTracingHelper::memoryBackedFifoSizePerDss / KB) - 1;
    EXPECT_EQ(expectedSize, fifoSize);
}

TEST(RayTracingHelperTests, whenMemoryBackedFifoSizeIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    size_t size = RayTracingHelper::getTotalMemoryBackedFifoSize(device);
    size_t expectedSize = device.getHardwareInfo().gtSystemInfo.MaxDualSubSlicesSupported * RayTracingHelper::memoryBackedFifoSizePerDss;
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenRTStackSizeIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    uint32_t maxBvhLevel = 2;
    uint32_t extraBytesLocal = 20;
    uint32_t extraBytesGlobal = 100;
    uint32_t tiles = 2;

    size_t expectedSize = alignUp(RayTracingHelper::getStackSizePerRay(maxBvhLevel, extraBytesLocal) * RayTracingHelper::getNumRtStacks(device) + extraBytesGlobal, MemoryConstants::cacheLineSize);
    size_t size = RayTracingHelper::getRTStackSizePerTile(device, tiles, maxBvhLevel, extraBytesLocal, extraBytesGlobal);
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    uint32_t numDssRtStacks = RayTracingHelper::getNumRtStacksPerDss(device);
    uint32_t expectedValue = device.getHardwareInfo().gtSystemInfo.MaxDualSubSlicesSupported
                                 ? static_cast<uint32_t>(RayTracingHelper::getNumRtStacks(device) / device.getHardwareInfo().gtSystemInfo.MaxDualSubSlicesSupported + 0.5)
                                 : RayTracingHelper::stackDssMultiplier;
    EXPECT_EQ(expectedValue, numDssRtStacks);
}

TEST(RayTracingHelperTests, whenNumRtStacksIsQueriedThenItIsEqualToNumRtStacksPerDssMultipliedByDualSubsliceCount) {
    MockDevice device;

    uint32_t numDssRtStacksPerDss = RayTracingHelper::getNumRtStacksPerDss(device);
    uint32_t numDssRtStacks = RayTracingHelper::getNumRtStacks(device);
    uint32_t subsliceCount = device.getHardwareInfo().gtSystemInfo.MaxDualSubSlicesSupported;

    EXPECT_EQ(numDssRtStacks, numDssRtStacksPerDss * subsliceCount);
}

TEST(RayTracingHelperTests, whenNumDssIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;
    EXPECT_EQ(device.getHardwareInfo().gtSystemInfo.MaxDualSubSlicesSupported, RayTracingHelper::getNumDss(device));
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
