/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/resource_surface.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

namespace NEO {

template <typename GfxFamily>
class MockCommandQueueWithCacheFlush : public MockCommandQueueHw<GfxFamily> {
    using MockCommandQueueHw<GfxFamily>::MockCommandQueueHw;

  public:
    bool isCacheFlushCommand(uint32_t commandType) const override {
        return commandRequireCacheFlush;
    }
    bool commandRequireCacheFlush = false;
};

using EnqueueResourceBarrierTestXeHpCoreAndLater = EnqueueHandlerTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, EnqueueResourceBarrierTestXeHpCoreAndLater, GivenCommandStreamWithoutKernelAndTimestampPacketEnabledWhenEnqueuedResourceBarrierWithEventThenTimestampAddedToEvent) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableTimestampPacket.set(1);
    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;

    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(
        context,
        CL_MEM_READ_WRITE,
        bufferSize,
        nullptr,
        retVal));
    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<ResourceSurface> surface(new ResourceSurface(allocation, CL_RESOURCE_BARRIER_TYPE_RELEASE, CL_MEMORY_SCOPE_DEVICE));

    MockTimestampPacketContainer timestamp1(*pDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    Event event1(mockCmdQ.get(), 0, 0, 0);
    cl_event event2;
    event1.addTimestampPacketNodes(timestamp1);

    cl_event waitlist[] = {&event1};

    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(mockCmdQ.get(), &descriptor, 1);

    retVal = mockCmdQ->enqueueResourceBarrier(
        &barrierCommand,
        1,
        waitlist,
        &event2);

    auto eventObj = castToObjectOrAbort<Event>(event2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(eventObj->getTimestampPacketNodes()->peekNodes().size(), 1u);
    eventObj->release();
}

HWCMDTEST_F(IGFX_XE_HP_CORE, EnqueueResourceBarrierTestXeHpCoreAndLater, GivenCommandStreamWithoutKernelAndTimestampPacketDisabledWhenEnqueuedResourceBarrierWithEventThenTimestampNotAddedToEvent) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableTimestampPacket.set(0);
    static_cast<UltCommandStreamReceiver<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver())->timestampPacketWriteEnabled = false;
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;
    mockCmdQ->timestampPacketContainer.reset();
    auto retVal = CL_INVALID_VALUE;
    size_t bufferSize = MemoryConstants::pageSize;
    std::unique_ptr<Buffer> buffer(Buffer::create(
        context,
        CL_MEM_READ_WRITE,
        bufferSize,
        nullptr,
        retVal));
    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<ResourceSurface> surface(new ResourceSurface(allocation, CL_RESOURCE_BARRIER_TYPE_RELEASE, CL_MEMORY_SCOPE_DEVICE));

    Event event1(mockCmdQ.get(), 0, 0, 0);
    cl_event event2;

    cl_event waitlist[] = {&event1};

    cl_resource_barrier_descriptor_intel descriptor{};
    descriptor.mem_object = buffer.get();
    descriptor.svm_allocation_pointer = nullptr;

    BarrierCommand barrierCommand(mockCmdQ.get(), &descriptor, 1);

    retVal = mockCmdQ->enqueueResourceBarrier(
        &barrierCommand,
        1,
        waitlist,
        &event2);
    auto eventObj = castToObjectOrAbort<Event>(event2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, eventObj->getTimestampPacketNodes());
    eventObj->release();
}

} // namespace NEO
