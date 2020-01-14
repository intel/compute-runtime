/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/execution_model_kernel_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include <memory>

using namespace NEO;

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetDebugSurfaceBtiIsCalledThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockDebugKernel>(device->getDevice(), &program));

    EXPECT_EQ(0, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(device->getDevice(), &program));

    EXPECT_EQ(MockDebugKernel::perThreadSystemThreadSurfaceSize, kernel->getPerThreadSystemThreadSurfaceSize());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetDebugSurfaceBtiIsCalledThenInvalidIndexValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(device->getDevice(), &program));

    EXPECT_EQ(-1, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenZeroIsReturned) {
    auto device = std::make_unique<MockClDevice>(new MockDevice);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(device->getDevice(), &program));

    EXPECT_EQ(0u, kernel->getPerThreadSystemThreadSurfaceSize());
}
