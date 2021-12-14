/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

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
    size_t expectedSize = device.getHardwareInfo().gtSystemInfo.DualSubSliceCount * RayTracingHelper::memoryBackedFifoSizePerDss;
    EXPECT_EQ(expectedSize, size);
}

TEST(RayTracingHelperTests, whenGlobalDispatchSizeIsRequestedThenCorrectValueIsReturned) {
    MockClDevice device{new MockDevice};
    MockContext context(&device);

    uint32_t maxBvhLevel = 2;
    uint32_t extraBytesPerThread = 20;
    uint32_t extraBytesGlobal = 100;

    size_t expectedSize = alignUp(RayTracingHelper::getRtGlobalsSize(), MemoryConstants::cacheLineSize) +
                          alignUp((RayTracingHelper::hitInfoSize +
                                   RayTracingHelper::bvhStackSize * maxBvhLevel +
                                   extraBytesPerThread),
                                  MemoryConstants::cacheLineSize) *
                              context.getDevice(0)->getHardwareInfo().gtSystemInfo.DualSubSliceCount * RayTracingHelper::stackDssMultiplier +
                          extraBytesGlobal;
    EXPECT_EQ(expectedSize, RayTracingHelper::getDispatchGlobalSize(device.getDevice(), maxBvhLevel, extraBytesPerThread, extraBytesGlobal));
}

TEST(RayTracingHelperTests, whenNumRtStacksPerDssIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    uint32_t numDssRtStacks = RayTracingHelper::getNumRtStacksPerDss(device);
    uint32_t expectedValue = device.getHardwareInfo().gtSystemInfo.DualSubSliceCount
                                 ? static_cast<uint32_t>(RayTracingHelper::getNumRtStacks(device) / device.getHardwareInfo().gtSystemInfo.DualSubSliceCount + 0.5)
                                 : RayTracingHelper::stackDssMultiplier;
    EXPECT_EQ(expectedValue, numDssRtStacks);
}

TEST(RayTracingHelperTests, whenNumDssIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;
    EXPECT_EQ(device.getHardwareInfo().gtSystemInfo.DualSubSliceCount, RayTracingHelper::getNumDss(device));
}

TEST(RayTracingHelperTests, whenStackSizePerRayIsRequestedThenCorrectValueIsReturned) {

    EXPECT_EQ(RayTracingHelper::hitInfoSize, RayTracingHelper::getStackSizePerRay(0, 0));

    uint32_t maxBvhLevel = 1234;
    uint32_t extraBytesLocal = 5678;

    uint32_t expectedValue = alignUp((RayTracingHelper::hitInfoSize + RayTracingHelper::bvhStackSize * maxBvhLevel +
                                      extraBytesLocal),
                                     MemoryConstants::cacheLineSize);
    EXPECT_EQ(RayTracingHelper::getStackSizePerRay(maxBvhLevel, extraBytesLocal), expectedValue);
}

TEST(RayTracingHelperTests, whenGetMemoryBackedFifoSizeToPatchIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(2u, RayTracingHelper::getMemoryBackedFifoSizeToPatch());
}
