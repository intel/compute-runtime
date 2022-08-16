/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

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
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(&pCmdQ->getContext(), CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(pCmdQ, &descriptor, 1);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    const auto enqueueResult = pCmdQ->enqueueResourceBarrier(&barrierCommand, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    bool resourceBarrierSupported = pCmdQ->isCacheFlushCommand(CL_COMMAND_RESOURCE_BARRIER);

    if (resourceBarrierSupported) {
        EXPECT_EQ(pCmdQ->taskCount, previousTaskCount + 1);
    } else {
        EXPECT_EQ(pCmdQ->taskCount, previousTaskCount);
    }
    EXPECT_EQ(pCmdQ->taskLevel, previousTaskLevel);
}

HWTEST_F(ResourceBarrierTest, GivenGpuHangAndBlockingCallsWhenEnqueueResourceBarrierIsCalledThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;

    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(&mockCommandQueueHw.getContext(), CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(&mockCommandQueueHw, &descriptor, 1);

    const auto enqueueResult = mockCommandQueueHw.enqueueResourceBarrier(&barrierCommand, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(ResourceBarrierTest, whenBarierCommandCreatedWithInvalidSvmPointerThenExceptionIsThrown) {
    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.svm_allocation_pointer = nullptr;
    EXPECT_THROW(BarrierCommand barrierCommand(pCmdQ, &descriptor, 1), std::exception);
}
