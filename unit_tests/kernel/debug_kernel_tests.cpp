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

class MockDebugKernel : public MockKernel {
  public:
    MockDebugKernel(Program *program, KernelInfo &kernelInfo, const Device &device) : MockKernel(program, kernelInfo, device) {
        if (!kernelInfo.patchInfo.pAllocateSystemThreadSurface) {
            SPatchAllocateSystemThreadSurface *patchToken = new SPatchAllocateSystemThreadSurface;

            patchToken->BTI = 0;
            patchToken->Offset = 0;
            patchToken->PerThreadSystemThreadSurfaceSize = MockDebugKernel::perThreadSystemThreadSurfaceSize;
            patchToken->Size = sizeof(SPatchAllocateSystemThreadSurface);
            patchToken->Token = iOpenCL::PATCH_TOKEN_ALLOCATE_SIP_SURFACE;

            kernelInfo.patchInfo.pAllocateSystemThreadSurface = patchToken;

            systemThreadSurfaceAllocated = true;
        }
    }

    ~MockDebugKernel() override {
        if (systemThreadSurfaceAllocated) {
            delete kernelInfo.patchInfo.pAllocateSystemThreadSurface;
        }
    }
    static const uint32_t perThreadSystemThreadSurfaceSize;
    bool systemThreadSurfaceAllocated = false;
};

const uint32_t MockDebugKernel::perThreadSystemThreadSurfaceSize = 0x100;

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetDebugSurfaceBtiIsCalledThenCorrectValueIsReturned) {
    std::unique_ptr<MockDevice> device(new MockDevice(*platformDevices[0]));
    MockProgram program;
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockDebugKernel>(*device.get(), &program));

    EXPECT_EQ(0, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelCompiledForDebuggingWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenCorrectValueIsReturned) {
    std::unique_ptr<MockDevice> device(new MockDevice(*platformDevices[0]));
    MockProgram program;
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*device.get(), &program));

    EXPECT_EQ(MockDebugKernel::perThreadSystemThreadSurfaceSize, kernel->getPerThreadSystemThreadSurfaceSize());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetDebugSurfaceBtiIsCalledThenInvalidIndexValueIsReturned) {
    std::unique_ptr<MockDevice> device(new MockDevice(*platformDevices[0]));
    MockProgram program;
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(*device.get(), &program));

    EXPECT_EQ(-1, kernel->getDebugSurfaceBti());
}

TEST(DebugKernelTest, givenKernelWithoutDebugFlagWhenGetPerThreadSystemThreadSurfaceSizeIsCalledThenZeroIsReturned) {
    std::unique_ptr<MockDevice> device(new MockDevice(*platformDevices[0]));
    MockProgram program;
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(*device.get(), &program));

    EXPECT_EQ(0u, kernel->getPerThreadSystemThreadSurfaceSize());
}
