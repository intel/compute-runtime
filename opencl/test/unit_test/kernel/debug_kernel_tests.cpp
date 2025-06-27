/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <memory>

using namespace NEO;

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(toClDeviceVector(*device));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(device->getDevice(), &program));

    EXPECT_EQ(MockDebugKernel::perThreadSystemThreadSurfaceSize, kernel->getPerThreadSystemThreadSurfaceSize());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(toClDeviceVector(*device));
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(device->getDevice(), &program));

    EXPECT_EQ(0u, kernel->getPerThreadSystemThreadSurfaceSize());
}
