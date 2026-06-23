/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/kernel/kernel.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

struct EnqueueKernelExecutionTypeFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
        commandQueue = std::make_unique<CommandQueue>(context.get(), device, nullptr, nullptr);
        l0Kernel = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        program = std::make_unique<Program>(context.get());
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    void setConcurrent() {
        cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;
        auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void setUsesSyncBuffer() {
        l0Kernel->getDescriptor().kernelAttributes.flags.usesSyncBuffer = true;
    }

    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<L0::ult::Mock<L0::KernelImp>> l0Kernel;
    std::unique_ptr<Program> program;
    std::unique_ptr<Kernel> kernel;
};

TEST_F(EnqueueKernelExecutionTypeFixture, givenConcurrentKernelWhenEnqueueNDRangeKernelThenReturnsCLInvalidKernel) {
    setConcurrent();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(EnqueueKernelExecutionTypeFixture, givenKernelUsingSyncBufferWhenEnqueueNDRangeKernelThenReturnsCLInvalidKernel) {
    setUsesSyncBuffer();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(EnqueueKernelExecutionTypeFixture, givenKernelUsingSyncBufferAndNotConcurrentWhenEnqueueNDCountKernelThenReturnsCLInvalidKernel) {
    setUsesSyncBuffer();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDCountKernelINTEL(commandQueue.get(), kernel.get(), 1, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
