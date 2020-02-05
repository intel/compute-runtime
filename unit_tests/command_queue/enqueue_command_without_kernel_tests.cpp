/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/timestamp_packet.h"
#include "core/memory_manager/surface.h"
#include "core/os_interface/os_context.h"
#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/enqueue_properties.h"
#include "test.h"
#include "unit_tests/fixtures/dispatch_flags_fixture.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_timestamp_container.h"

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

HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelWhenCommandEnqueuedThenTaskCountIncreased) {

    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;
    EnqueueProperties enqueueProperties(false, false, false, true, nullptr);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);
    EXPECT_EQ(allocation->getTaskCount(mockCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId()), 1u);
}

HWTEST_F(EnqueueHandlerTest, givenNonBlitPropertyWhenEnqueueIsBlockedThenDontRegisterBlitProperties) {
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
    auto &csr = mockCmdQ->getGpgpuCommandStreamReceiver();

    auto commandStream = new LinearStream();
    csr.ensureCommandBufferAllocation(*commandStream, 1, 1);

    auto blockedCommandsDataForDependencyFlush = new KernelOperation(commandStream, *csr.getInternalAllocationStorage());

    TimestampPacketDependencies timestampPacketDependencies;
    MultiDispatchInfo multiDispatchInfo;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    const EnqueueProperties enqueuePropertiesForDependencyFlush(false, false, false, true, nullptr);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForDependencyFlush);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_MARKER, surfaces, size_t(0), multiDispatchInfo, timestampPacketDependencies,
                             blockedCommandsData, enqueuePropertiesForDependencyFlush, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr));
    EXPECT_FALSE(blockedCommandsDataForDependencyFlush->blitEnqueue);
}

HWTEST_F(EnqueueHandlerTest, givenBlitPropertyWhenEnqueueIsBlockedThenRegisterBlitProperties) {
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
    auto &csr = mockCmdQ->getGpgpuCommandStreamReceiver();

    auto commandStream = new LinearStream();
    csr.ensureCommandBufferAllocation(*commandStream, 1, 1);

    auto blockedCommandsDataForBlitEnqueue = new KernelOperation(commandStream, *csr.getInternalAllocationStorage());

    TimestampPacketDependencies timestampPacketDependencies;
    MultiDispatchInfo multiDispatchInfo;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    BlitProperties blitProperties;
    blitProperties.srcAllocation = reinterpret_cast<GraphicsAllocation *>(0x12345);
    blitProperties.dstAllocation = reinterpret_cast<GraphicsAllocation *>(0x56789);
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);
    const EnqueueProperties enqueuePropertiesForBlitEnqueue(true, false, false, false, &blitPropertiesContainer);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForBlitEnqueue);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_READ_BUFFER, surfaces, size_t(0), multiDispatchInfo, timestampPacketDependencies,
                             blockedCommandsData, enqueuePropertiesForBlitEnqueue, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr));
    EXPECT_TRUE(blockedCommandsDataForBlitEnqueue->blitEnqueue);
    EXPECT_EQ(blitProperties.srcAllocation, blockedCommandsDataForBlitEnqueue->blitPropertiesContainer.begin()->srcAllocation);
    EXPECT_EQ(blitProperties.dstAllocation, blockedCommandsDataForBlitEnqueue->blitPropertiesContainer.begin()->dstAllocation);
}

HWTEST_F(DispatchFlagsTests, whenEnqueueCommandWithoutKernelThenPassCorrectDispatchFlags) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, nullptr);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);

    EXPECT_EQ(blocking, mockCsr->passedDispatchFlags.blocking);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(device->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);
    EXPECT_EQ(mockCmdQ->flushStamp->getStampReference(), mockCsr->passedDispatchFlags.flushStampReference);
}

HWTEST_F(DispatchFlagsTests, whenEnqueueCommandWithoutKernelThenPassCorrectThrottleHint) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    mockCmdQ->throttle = QueueThrottle::HIGH;
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, nullptr);
    bool blocking = true;

    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);

    EXPECT_EQ(mockCmdQ->throttle, mockCsr->passedDispatchFlags.throttle);
}

HWTEST_F(DispatchFlagsTests, givenBlitEnqueueWhenDispatchingCommandsWithoutKernelThenDoImplicitFlush) {
    using CsrType = MockCsrHw2<FamilyType>;
    DebugManager.flags.EnableTimestampPacket.set(1);
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    mockCsr->skipBlitCalls = true;
    mockCmdQ->bcsEngine = mockCmdQ->gpgpuEngine;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), 0, 1, nullptr, retVal));

    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);

    mockCmdQ->obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, true);
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(multiDispatchInfo, timestampPacketDependencies,
                                                                            eventsRequest, mockCmdQ->getCS(0), CL_COMMAND_READ_BUFFER, false);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    EnqueueProperties enqueueProperties(true, false, false, false, &blitPropertiesContainer);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);

    EXPECT_TRUE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
}

HWTEST_F(DispatchFlagsTests, givenN1EnabledWhenDispatchingWithoutKernelTheAllowOutOfOrderExecution) {
    using CsrType = MockCsrHw2<FamilyType>;
    DebugManager.flags.EnableTimestampPacket.set(1);

    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    mockCsr->skipBlitCalls = true;
    mockCmdQ->bcsEngine = mockCmdQ->gpgpuEngine;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), 0, 1, nullptr, retVal));

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    bool blocked = false;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);

    mockCmdQ->obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, true);
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(multiDispatchInfo, timestampPacketDependencies,
                                                                            eventsRequest, mockCmdQ->getCS(0), CL_COMMAND_READ_BUFFER, false);
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);
    EnqueueProperties enqueueProperties(true, false, false, false, &blitPropertiesContainer);

    mockCsr->nTo1SubmissionModelEnabled = false;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocked, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);

    mockCsr->nTo1SubmissionModelEnabled = true;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocked, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
}

HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelAndZeroSurfacesWhenEnqueuedHandlerThenProgramPipeControl) {
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));

    mockCmdQ->commandRequireCacheFlush = true;
    MultiDispatchInfo multiDispatch;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, nullptr);

    auto requiredCmdStreamSize = alignUp(PipeControlHelper<FamilyType>::getSizeForPipeControlWithPostSyncOperation(pDevice->getHardwareInfo()),
                                         MemoryConstants::cacheLineSize);

    EXPECT_EQ(mockCmdQ->getCS(0).getUsed(), requiredCmdStreamSize);
}

HWTEST_F(EnqueueHandlerTest, givenTimestampPacketWriteEnabledAndCommandWithCacheFlushWhenEnqueueingHandlerThenObtainNewStamp) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(csr.rootDeviceIndex, pDevice->getMemoryManager());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;

    cl_event event;

    MultiDispatchInfo multiDispatch;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, &event);
    auto node1 = mockCmdQ->timestampPacketContainer->peekNodes().at(0);
    EXPECT_NE(nullptr, node1);
    clReleaseEvent(event);
}
HWTEST_F(EnqueueHandlerTest, givenTimestampPacketWriteDisabledAndCommandWithCacheFlushWhenEnqueueingHandlerThenTimeStampContainerIsNotCreated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;

    auto mockTagAllocator = new MockTagAllocator<>(pDevice->getRootDeviceIndex(), pDevice->getMemoryManager());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;

    cl_event event;

    MultiDispatchInfo multiDispatch;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, &event);
    auto container = mockCmdQ->timestampPacketContainer.get();
    EXPECT_EQ(nullptr, container);
    clReleaseEvent(event);
}
} // namespace NEO
