/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/fixtures/execution_model_kernel_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "test.h"

#include <memory>

using namespace OCLRT;

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetDebugSurfaceBtiIsCalledThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockDebugKernel>(*device.get(), &program));

    EXPECT_EQ(0, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenCorrectValueIsReturned) {
    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*device.get(), &program));

    EXPECT_EQ(MockDebugKernel::perThreadSystemThreadSurfaceSize, kernel->getPerThreadSystemThreadSurfaceSize());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetDebugSurfaceBtiIsCalledThenInvalidIndexValueIsReturned) {
    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(*device.get(), &program));

    EXPECT_EQ(-1, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenZeroIsReturned) {
    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    MockProgram program(*device->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(*device.get(), &program));

    EXPECT_EQ(0u, kernel->getPerThreadSystemThreadSurfaceSize());
}
