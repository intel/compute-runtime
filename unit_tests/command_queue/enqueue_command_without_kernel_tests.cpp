/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
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

    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    TimestampPacketContainer previousTimestampPacketNodes;
    EnqueueProperties enqueueProperties(false, false, false, true, nullptr);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, &previousTimestampPacketNodes, eventsRequest, eventBuilder, 0);
    EXPECT_EQ(allocation->getTaskCount(mockCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId()), 1u);
}

HWTEST_F(EnqueueHandlerTest, givenNonBlitPropertyWhenEnqueueIsBlockedThenDontRegisterBlitProperties) {
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));
    auto &csr = mockCmdQ->getGpgpuCommandStreamReceiver();

    auto commandStream = new LinearStream();
    csr.ensureCommandBufferAllocation(*commandStream, 1, 1);

    auto blockedCommandsDataForDependencyFlush = new KernelOperation(commandStream, *csr.getInternalAllocationStorage());

    TimestampPacketContainer previousTimestampPacketNodes;
    MultiDispatchInfo multiDispatchInfo;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    const EnqueueProperties enqueuePropertiesForDependencyFlush(false, false, false, true, nullptr);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForDependencyFlush);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_MARKER, surfaces, size_t(0), multiDispatchInfo, &previousTimestampPacketNodes,
                             blockedCommandsData, enqueuePropertiesForDependencyFlush, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr));
    EXPECT_FALSE(blockedCommandsDataForDependencyFlush->blitEnqueue);
}

HWTEST_F(EnqueueHandlerTest, givenBlitPropertyWhenEnqueueIsBlockedThenRegisterBlitProperties) {
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));
    auto &csr = mockCmdQ->getGpgpuCommandStreamReceiver();

    auto commandStream = new LinearStream();
    csr.ensureCommandBufferAllocation(*commandStream, 1, 1);

    auto blockedCommandsDataForBlitEnqueue = new KernelOperation(commandStream, *csr.getInternalAllocationStorage());

    TimestampPacketContainer previousTimestampPacketNodes;
    MultiDispatchInfo multiDispatchInfo;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    BlitProperties blitProperties;
    blitProperties.srcAllocation = reinterpret_cast<GraphicsAllocation *>(0x12345);
    blitProperties.dstAllocation = reinterpret_cast<GraphicsAllocation *>(0x56789);
    const EnqueueProperties enqueuePropertiesForBlitEnqueue(true, false, false, false, &blitProperties);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForBlitEnqueue);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_READ_BUFFER, surfaces, size_t(0), multiDispatchInfo, &previousTimestampPacketNodes,
                             blockedCommandsData, enqueuePropertiesForBlitEnqueue, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr));
    EXPECT_TRUE(blockedCommandsDataForBlitEnqueue->blitEnqueue);
    EXPECT_EQ(blitProperties.srcAllocation, blockedCommandsDataForBlitEnqueue->blitProperties.srcAllocation);
    EXPECT_EQ(blitProperties.dstAllocation, blockedCommandsDataForBlitEnqueue->blitProperties.dstAllocation);
}

struct DispatchFlagsTests : public ::testing::Test {
    template <typename CsrType>
    void SetUpImpl() {
        auto executionEnvironment = new MockExecutionEnvironmentWithCsr<CsrType>(**platformDevices, 1u);
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0));
        context = std::make_unique<MockContext>(device.get());
    }

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restore;
};

HWTEST_F(DispatchFlagsTests, whenEnqueueCommandWithoutKernelThenPassCorrectDispatchFlags) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    auto blocking = true;
    TimestampPacketContainer previousTimestampPacketNodes;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, nullptr);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, &previousTimestampPacketNodes, eventsRequest, eventBuilder, 0);

    EXPECT_EQ(blocking, mockCsr->passedDispatchFlags.blocking);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(mockCmdQ->isMultiEngineQueue(), mockCsr->passedDispatchFlags.multiEngineQueue);
    EXPECT_EQ(device->getPreemptionMode(), mockCsr->passedDispatchFlags.preemptionMode);
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
    TimestampPacketContainer previousTimestampPacketNodes;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);

    mockCmdQ->obtainNewTimestampPacketNodes(1, previousTimestampPacketNodes, true);
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(multiDispatchInfo, previousTimestampPacketNodes, eventsRequest,
                                                                            mockCmdQ->getCS(0), 0, false);

    EnqueueProperties enqueueProperties(true, false, false, false, &blitProperties);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocking, enqueueProperties, &previousTimestampPacketNodes, eventsRequest, eventBuilder, 0);

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

    TimestampPacketContainer previousTimestampPacketNodes;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    bool blocked = false;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);

    mockCmdQ->obtainNewTimestampPacketNodes(1, previousTimestampPacketNodes, true);
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(multiDispatchInfo, previousTimestampPacketNodes, eventsRequest,
                                                                            mockCmdQ->getCS(0), 0, false);
    EnqueueProperties enqueueProperties(true, false, false, false, &blitProperties);
    enqueueProperties.blitProperties = &blitProperties;

    mockCsr->nTo1SubmissionModelEnabled = false;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocked, enqueueProperties, &previousTimestampPacketNodes, eventsRequest, eventBuilder, 0);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);

    mockCsr->nTo1SubmissionModelEnabled = true;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, mockCmdQ->getCS(0), 0, blocked, enqueueProperties, &previousTimestampPacketNodes, eventsRequest, eventBuilder, 0);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
}

HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelAndZeroSurfacesWhenEnqueuedHandlerThenProgramPipeControl) {
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pDevice, 0));

    mockCmdQ->commandRequireCacheFlush = true;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, nullptr, 0, nullptr, nullptr);

    auto requiredCmdStreamSize = alignUp(PipeControlHelper<FamilyType>::getSizeForPipeControlWithPostSyncOperation(pDevice->getHardwareInfo()),
                                         MemoryConstants::cacheLineSize);

    EXPECT_EQ(mockCmdQ->getCS(0).getUsed(), requiredCmdStreamSize);
}
HWTEST_F(EnqueueHandlerTest, givenTimestampPacketWriteEnabledAndCommandWithCacheFlushWhenEnqueueingHandlerThenObtainNewStamp) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = true;

    auto mockTagAllocator = new MockTagAllocator<>(pDevice->getMemoryManager());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;

    cl_event event;

    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, nullptr, 0, nullptr, &event);
    auto node1 = mockCmdQ->timestampPacketContainer->peekNodes().at(0);
    EXPECT_NE(nullptr, node1);
    clReleaseEvent(event);
}
HWTEST_F(EnqueueHandlerTest, givenTimestampPacketWriteDisabledAndCommandWithCacheFlushWhenEnqueueingHandlerThenTimeStampContainerIsNotCreated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.timestampPacketWriteEnabled = false;

    auto mockTagAllocator = new MockTagAllocator<>(pDevice->getMemoryManager());
    csr.timestampPacketAllocator.reset(mockTagAllocator);
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pDevice, 0));
    mockCmdQ->commandRequireCacheFlush = true;

    cl_event event;

    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, nullptr, 0, nullptr, &event);
    auto container = mockCmdQ->timestampPacketContainer.get();
    EXPECT_EQ(nullptr, container);
    clReleaseEvent(event);
}
} // namespace NEO
