/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

TEST(RayTracingHelperTests, whenMemoryBackedFifoSizeIsRequestedThenCorrectValueIsReturned) {
    MockDevice device;

    size_t size = RayTracingHelper::getTotalMemoryBackedFifoSize(device);
    size_t expectedSize = device.getHardwareInfo().gtSystemInfo.DualSubSliceCount * RayTracingHelper::memoryBackedFifoSizePerDss;
    EXPECT_EQ(expectedSize, size);
}
