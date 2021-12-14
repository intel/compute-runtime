/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"

using namespace NEO;

using ResourceBarrierTest = Test<CommandEnqueueFixture>;

HWTEST_F(ResourceBarrierTest, givenNullArgsAndHWCommandQueueWhenEnqueueResourceBarrierCalledThenCorrectStatusReturned) {
    cl_resource_barrier_descriptor_intel descriptor{};
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(
        &pCmdQ->getContext(),
        CL_MEM_READ_WRITE,
        bufferSize,
        nullptr,
        retVal));
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(pCmdQ, &descriptor, 1);
    auto surface = reinterpret_cast<ResourceSurface *>(barrierCommand.surfacePtrs.begin()[0]);
    EXPECT_EQ(surface->getGraphicsAllocation(), buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex()));

    retVal = pCmdQ->enqueueResourceBarrier(
        &barrierCommand,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(ResourceBarrierTest, whenEnqueueResourceBarrierCalledThenUpdateQueueCompletionStamp) {
    cl_resource_barrier_descriptor_intel descriptor{};
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(&pCmdQ->getContext(), CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(pCmdQ, &descriptor, 1);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;
    pCmdQ->enqueueResourceBarrier(&barrierCommand, 0, nullptr, nullptr);

    bool resourceBarrierSupported = pCmdQ->isCacheFlushCommand(CL_COMMAND_RESOURCE_BARRIER);

    if (resourceBarrierSupported) {
        EXPECT_EQ(pCmdQ->taskCount, previousTaskCount + 1);
    } else {
        EXPECT_EQ(pCmdQ->taskCount, previousTaskCount);
    }
    EXPECT_EQ(pCmdQ->taskLevel, previousTaskLevel);
}

HWTEST_F(ResourceBarrierTest, whenBarierCommandCreatedWithInvalidSvmPointerThenExceptionIsThrown) {
    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.svm_allocation_pointer = nullptr;
    EXPECT_THROW(BarrierCommand barrierCommand(pCmdQ, &descriptor, 1), std::exception);
}
