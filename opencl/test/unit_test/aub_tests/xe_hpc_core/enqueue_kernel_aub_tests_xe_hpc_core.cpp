/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_fixture.h"

using namespace NEO;

extern const HardwareInfo *defaultHwInfo;

using AUBSimpleKernelStatelessTest = Test<KernelAUBFixture<SimpleKernelStatelessFixture>>;

HWTEST_F(AUBSimpleKernelStatelessTest, givenPrefetchEnabledWhenEnqueuedKernelThenDataIsCorrect) {
    USE_REAL_FILE_SYSTEM();
    DebugManagerStateRestore restore;
    debugManager.flags.EnableMemoryPrefetch.set(1);

    constexpr size_t bufferSize = MemoryConstants::pageSize;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    uint8_t bufferData[bufferSize] = {};
    uint8_t bufferExpected[bufferSize];
    memset(bufferExpected, 0xCD, bufferSize);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_USE_HOST_PTR | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL,
                                                         bufferSize, bufferData, retVal));
    ASSERT_NE(nullptr, buffer);

    kernel->setArg(0, buffer.get());

    retVal = this->pCmdQ->enqueueKernel(kernel.get(), workDim, globalWorkOffset, globalWorkSize,
                                        localWorkSize, 0, nullptr, nullptr);

    this->pCmdQ->flush();
    expectMemory<FamilyType>(addrToPtr(ptrOffset(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), buffer->getOffset())),
                             bufferExpected, bufferSize);
}
