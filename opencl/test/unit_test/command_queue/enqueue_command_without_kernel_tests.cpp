/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/enqueue_properties.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

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

    CsrDependencies csrDeps;
    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr);
    EXPECT_EQ(allocation->getTaskCount(mockCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId()), 1u);
}

template <bool enabled>
struct EnqueueHandlerTimestampTest : public EnqueueHandlerTest {
    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(enabled);
        EnqueueHandlerTest::SetUp();
    }

    void TearDown() override {
        EnqueueHandlerTest::TearDown();
    }

    DebugManagerStateRestore restorer;
};

using EnqueueHandlerTimestampEnabledTest = EnqueueHandlerTimestampTest<true>;

HWTEST_F(EnqueueHandlerTimestampEnabledTest, givenProflingAndTimeStampPacketsEnabledWhenEnqueueCommandWithoutKernelThenSubmitTimeStampIsSet) {
    cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, properties));

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    eventBuilder.create<MockEvent<Event>>(mockCmdQ.get(), CL_COMMAND_USER, CompletionStamp::notReady, CompletionStamp::notReady);
    auto ev = static_cast<MockEvent<UserEvent> *>(eventBuilder.getEvent());
    ev->setProfilingEnabled(true);
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;

    CsrDependencies csrDeps;
    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);

    EXPECT_EQ(ev->submitTimeStamp.CPUTimeinNS, 0u);
    EXPECT_EQ(ev->submitTimeStamp.GPUTimeStamp, 0u);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr);

    EXPECT_NE(ev->submitTimeStamp.CPUTimeinNS, 0u);
    EXPECT_EQ(ev->submitTimeStamp.GPUTimeStamp, 0u);

    DebugManagerStateRestore dbgState;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);
    ev->queueTimeStamp.GPUTimeStamp = 1000;
    ev->calculateSubmitTimestampData();

    EXPECT_NE(ev->submitTimeStamp.GPUTimeStamp, 0u);

    delete ev;
}

using EnqueueHandlerTimestampDisabledTest = EnqueueHandlerTimestampTest<false>;

HWTEST_F(EnqueueHandlerTimestampDisabledTest, givenProflingEnabledTimeStampPacketsDisabledWhenEnqueueCommandWithoutKernelThenSubmitTimeStampIsSet) {
    cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, properties));

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    eventBuilder.create<MockEvent<Event>>(mockCmdQ.get(), CL_COMMAND_USER, CompletionStamp::notReady, CompletionStamp::notReady);
    auto ev = static_cast<MockEvent<UserEvent> *>(eventBuilder.getEvent());
    ev->setProfilingEnabled(true);
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;

    CsrDependencies csrDeps;
    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);

    EXPECT_EQ(ev->submitTimeStamp.CPUTimeinNS, 0u);
    EXPECT_EQ(ev->submitTimeStamp.GPUTimeStamp, 0u);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr);

    EXPECT_NE(ev->submitTimeStamp.CPUTimeinNS, 0u);
    EXPECT_EQ(ev->submitTimeStamp.GPUTimeStamp, 0u);

    DebugManagerStateRestore dbgState;
    DebugManager.flags.EnableDeviceBasedTimestamps.set(true);
    ev->queueTimeStamp.GPUTimeStamp = 1000;
    ev->calculateSubmitTimestampData();

    EXPECT_NE(ev->submitTimeStamp.GPUTimeStamp, 0u);

    delete ev;
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

    const EnqueueProperties enqueuePropertiesForDependencyFlush(false, false, false, true, false, nullptr);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForDependencyFlush);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_MARKER, surfaces, size_t(0), multiDispatchInfo, timestampPacketDependencies,
                             blockedCommandsData, enqueuePropertiesForDependencyFlush, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr), nullptr);
    EXPECT_FALSE(blockedCommandsDataForDependencyFlush->blitEnqueue);
}

HWTEST_F(EnqueueHandlerTest, givenBlitPropertyWhenEnqueueIsBlockedThenRegisterBlitProperties) {
    HardwareInfo *hwInfo = pClDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.blitterOperationsSupported = true;
    REQUIRE_BLITTER_OR_SKIP(hwInfo);

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
    const EnqueueProperties enqueuePropertiesForBlitEnqueue(true, false, false, false, false, &blitPropertiesContainer);

    auto blockedCommandsData = std::unique_ptr<KernelOperation>(blockedCommandsDataForBlitEnqueue);
    Surface *surfaces[] = {nullptr};
    mockCmdQ->enqueueBlocked(CL_COMMAND_READ_BUFFER, surfaces, size_t(0), multiDispatchInfo, timestampPacketDependencies,
                             blockedCommandsData, enqueuePropertiesForBlitEnqueue, eventsRequest,
                             eventBuilder, std::unique_ptr<PrintfHandler>(nullptr), mockCmdQ->getBcsForAuxTranslation());
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
    CsrDependencies csrDeps;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr);

    EXPECT_EQ(blocking, mockCsr->passedDispatchFlags.blocking);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(L3CachingSettings::NotApplicable, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_EQ(GrfConfig::NotApplicable, mockCsr->passedDispatchFlags.numGrfRequired);
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
    CsrDependencies csrDeps;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);
    bool blocking = true;

    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr);

    EXPECT_EQ(mockCmdQ->throttle, mockCsr->passedDispatchFlags.throttle);
}

HWTEST_F(DispatchFlagsBlitTests, givenBlitEnqueueWhenDispatchingCommandsWithoutKernelThenDoImplicitFlush) {
    using CsrType = MockCsrHw2<FamilyType>;
    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(1);
    DebugManager.flags.EnableTimestampPacket.set(1);

    SetUpImpl<CsrType>();
    REQUIRE_FULL_BLITTER_OR_SKIP(&device->getHardwareInfo());

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    mockCsr->skipBlitCalls = true;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), 0, 1, nullptr, retVal));
    auto &bcsCsr = *mockCmdQ->bcsEngines[0]->commandStreamReceiver;

    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);
    CsrDependencies csrDeps;

    mockCmdQ->obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, true, bcsCsr);

    timestampPacketDependencies.cacheFlushNodes.add(mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(bcsCsr, multiDispatchInfo, timestampPacketDependencies,
                                                                            eventsRequest, &mockCmdQ->getCS(0), CL_COMMAND_READ_BUFFER, false);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    EnqueueProperties enqueueProperties(true, false, false, false, false, &blitPropertiesContainer);
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, &bcsCsr);

    EXPECT_TRUE(mockCsr->passedDispatchFlags.implicitFlush);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.guardCommandBufferWithPipeControl);
    EXPECT_EQ(L3CachingSettings::NotApplicable, mockCsr->passedDispatchFlags.l3CacheSettings);
    EXPECT_EQ(GrfConfig::NotApplicable, mockCsr->passedDispatchFlags.numGrfRequired);
}

HWTEST_F(DispatchFlagsBlitTests, givenN1EnabledWhenDispatchingWithoutKernelThenAllowOutOfOrderExecution) {
    using CsrType = MockCsrHw2<FamilyType>;
    DebugManager.flags.EnableTimestampPacket.set(1);
    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(1);

    SetUpImpl<CsrType>();
    REQUIRE_FULL_BLITTER_OR_SKIP(&device->getHardwareInfo());

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());
    mockCsr->skipBlitCalls = true;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), 0, 1, nullptr, retVal));
    auto &bcsCsr = *mockCmdQ->bcsEngines[0]->commandStreamReceiver;

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    bool blocked = false;
    BuiltinOpParams builtinOpParams;
    builtinOpParams.srcMemObj = buffer.get();
    builtinOpParams.dstPtr = reinterpret_cast<void *>(0x1234);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setBuiltinOpParams(builtinOpParams);

    mockCmdQ->obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, true, bcsCsr);
    timestampPacketDependencies.cacheFlushNodes.add(mockCmdQ->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    BlitProperties blitProperties = mockCmdQ->processDispatchForBlitEnqueue(bcsCsr, multiDispatchInfo, timestampPacketDependencies,
                                                                            eventsRequest, &mockCmdQ->getCS(0), CL_COMMAND_READ_BUFFER, false);
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    CsrDependencies csrDeps;
    EnqueueProperties enqueueProperties(true, false, false, false, false, &blitPropertiesContainer);

    mockCsr->nTo1SubmissionModelEnabled = false;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, &mockCmdQ->getCS(0), 0, blocked, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, &bcsCsr);
    EXPECT_FALSE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);

    mockCsr->nTo1SubmissionModelEnabled = true;
    mockCmdQ->enqueueCommandWithoutKernel(nullptr, 0, &mockCmdQ->getCS(0), 0, blocked, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, &bcsCsr);
    EXPECT_TRUE(mockCsr->passedDispatchFlags.outOfOrderExecutionAllowed);
}

HWTEST_F(DispatchFlagsTests, givenMockKernelWhenSettingAdditionalKernelExecInfoThenCorrectValueIsSet) {
    using CsrType = MockCsrHw2<FamilyType>;
    SetUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, false, nullptr);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()}));
    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(device.get(), pKernel);

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, *device.get()));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 4096u, dsh);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 4096u, ioh);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 4096u, ssh);
    blockedCommandsData->setHeaps(dsh, ioh, ssh);
    std::vector<Surface *> v;

    pKernel->setAdditionalKernelExecInfo(123u);
    std::unique_ptr<CommandComputeKernel> cmd(new CommandComputeKernel(*mockCmdQ.get(), blockedCommandsData, v, false, false, false, std::move(printfHandler), PreemptionMode::Disabled, pKernel, 1));
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, 123u);

    pKernel->setAdditionalKernelExecInfo(123u);
    mockCsr->setMediaVFEStateDirty(true);
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, 123u);
}

HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelAndZeroSurfacesWhenEnqueuedHandlerThenProgramPipeControl) {
    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pClDevice, 0));

    mockCmdQ->commandRequireCacheFlush = true;
    MultiDispatchInfo multiDispatch;
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    auto requiredCmdStreamSize = alignUp(MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(
                                             pDevice->getHardwareInfo()),
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
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

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
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, multiDispatch, 0, nullptr, &event);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    auto container = mockCmdQ->timestampPacketContainer.get();
    EXPECT_EQ(nullptr, container);
    clReleaseEvent(event);
}
} // namespace NEO
