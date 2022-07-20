/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

HWTEST_F(CommandQueueHwTest, WhenConstructingTwoCommandQueuesThenOnlyOneDebugSurfaceIsAllocated) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));
    auto sipType = SipKernel::getSipKernelType(device->getDevice());
    SipKernel::initSipKernel(sipType, device->getDevice());

    MockCommandQueueHw<FamilyType> mockCmdQueueHw1(context, device.get(), nullptr);

    auto dbgSurface = mockCmdQueueHw1.getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();
    EXPECT_NE(dbgSurface, nullptr);

    MockCommandQueueHw<FamilyType> mockCmdQueueHw2(context, device.get(), nullptr);
    EXPECT_EQ(dbgSurface, mockCmdQueueHw1.getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation());
}

HWTEST_F(CommandQueueHwTest, givenNoTimestampPacketsWhenWaitForTimestampsThenNoWaitAndTagIsNotUpdated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableTimestampPacket.set(0);
    DebugManager.flags.EnableTimestampWaitForQueues.set(4);
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));
    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    MockCommandQueueHw<FamilyType> cmdQ(context, device.get(), nullptr);
    auto taskCount = device->getUltCommandStreamReceiver<FamilyType>().peekLatestFlushedTaskCount();

    cmdQ.waitForTimestamps({}, 101u);

    EXPECT_EQ(device->getUltCommandStreamReceiver<FamilyType>().peekLatestFlushedTaskCount(), taskCount);
}

HWTEST_F(CommandQueueHwTest, WhenDebugSurfaceIsAllocatedThenBufferIsZeroed) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));
    auto sipType = SipKernel::getSipKernelType(device->getDevice());
    SipKernel::initSipKernel(sipType, device->getDevice());

    MockCommandQueueHw<FamilyType> mockCmdQueueHw1(context, device.get(), nullptr);

    auto dbgSurface = mockCmdQueueHw1.getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();
    EXPECT_NE(dbgSurface, nullptr);
    auto mem = dbgSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    auto &stateSaveAreaHeader = SipKernel::getSipKernel(device->getDevice()).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = dbgSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();
    EXPECT_TRUE(memoryZeroed(mem, size));
}

HWTEST_F(CommandQueueHwTest, WhenConstructingCommandQueueDebugOnButIgcDoesNotReturnSSAHDoNotCopyIt) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockActiveSourceLevelDebugger(new MockOsLibrary));

    MockGraphicsAllocation sipAlloc1;
    auto mockSip1 = std::make_unique<MockSipKernel>(SipKernelType::DbgCsrLocal, &sipAlloc1);
    mockSip1->mockStateSaveAreaHeader.clear();

    MockGraphicsAllocation sipAlloc2;
    auto mockSip2 = std::make_unique<MockSipKernel>(SipKernelType::DbgCsr, &sipAlloc2);
    mockSip2->mockStateSaveAreaHeader.clear();

    auto mockBuiltIns = new MockBuiltins();
    mockBuiltIns->overrideSipKernel(std::move(mockSip1));
    mockBuiltIns->overrideSipKernel(std::move(mockSip2));

    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);

    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDeviceWithDebuggerActive>(executionEnvironment, 0u));

    MockCommandQueueHw<FamilyType> mockCmdQueueHw1(context, device.get(), nullptr);

    auto dbgSurface = mockCmdQueueHw1.getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();
    EXPECT_NE(dbgSurface, nullptr);

    auto &stateSaveAreaHeader = SipKernel::getSipKernel(device->getDevice()).getStateSaveAreaHeader();
    EXPECT_EQ(static_cast<size_t>(0), stateSaveAreaHeader.size());
}

HWTEST_F(CommandQueueHwTest, givenMultiDispatchInfoWhenAskingForAuxTranslationThenCheckMemObjectsCountAndDebugFlag) {
    DebugManagerStateRestore restore;
    MockBuffer buffer;
    auto emptyKernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    MultiDispatchInfo multiDispatchInfo;
    HardwareInfo *hwInfo = pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));

    MockCommandQueueHw<FamilyType> mockCmdQueueHw(context, pClDevice, nullptr);

    hwInfo->capabilityTable.blitterOperationsSupported = true;

    EXPECT_FALSE(mockCmdQueueHw.isBlitAuxTranslationRequired(multiDispatchInfo));

    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(emptyKernelObjsForAuxTranslation));
    EXPECT_FALSE(mockCmdQueueHw.isBlitAuxTranslationRequired(multiDispatchInfo));

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::MEM_OBJ, &buffer});
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    EXPECT_TRUE(mockCmdQueueHw.isBlitAuxTranslationRequired(multiDispatchInfo));

    hwInfo->capabilityTable.blitterOperationsSupported = false;
    EXPECT_FALSE(mockCmdQueueHw.isBlitAuxTranslationRequired(multiDispatchInfo));

    hwInfo->capabilityTable.blitterOperationsSupported = true;
    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Builtin));
    EXPECT_FALSE(mockCmdQueueHw.isBlitAuxTranslationRequired(multiDispatchInfo));
}

HWTEST_F(CommandQueueHwTest, WhenEnqueuingBlockedMapUnmapOperationThenVirtualEventIsCreated) {

    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    ASSERT_NE(nullptr, pHwQ->virtualEvent);
    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
}

class MockCommandStreamReceiverWithFailingFlushBatchedSubmission : public MockCommandStreamReceiver {
  public:
    using MockCommandStreamReceiver::MockCommandStreamReceiver;
    bool flushBatchedSubmissions() override {
        return false;
    }
};

template <typename GfxFamily>
struct MockCommandQueueHwWithOverwrittenCsr : public CommandQueueHw<GfxFamily> {
    using CommandQueueHw<GfxFamily>::CommandQueueHw;
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission *csr;
    CommandStreamReceiver &getGpgpuCommandStreamReceiver() const override { return *csr; }
};

HWTEST_F(CommandQueueHwTest, GivenCommandQueueWhenProcessDispatchForMarkerCalledThenEventAllocationIsMadeResident) {

    pDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = false;
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    auto myCmdQ = std::make_unique<MockCommandQueueHwWithOverwrittenCsr<FamilyType>>(pCmdQ->getContextPtr(), pClDevice, nullptr, false);
    myCmdQ->csr = &csr;
    csr.osContext = &pCmdQ->getGpgpuCommandStreamReceiver().getOsContext();
    std::unique_ptr<Event> event(new Event(myCmdQ.get(), CL_COMMAND_COPY_BUFFER, 0, 0));
    ASSERT_NE(nullptr, event);

    GraphicsAllocation *allocation = event->getHwTimeStampNode()->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);
    cl_event a = event.get();
    EventsRequest eventsRequest(0, nullptr, &a);
    uint32_t streamBuffer[100] = {};
    NEO::LinearStream linearStream(streamBuffer, sizeof(streamBuffer));
    CsrDependencies deps = {};
    myCmdQ->processDispatchForMarker(*myCmdQ.get(), &linearStream, eventsRequest, deps);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);
}

HWTEST_F(CommandQueueHwTest, GivenCommandQueueWhenItIsCreatedThenInitDirectSubmissionIsCalledOnAllBcsEngines) {
    MockCommandQueueHw<FamilyType> queue(pContext, pClDevice, nullptr);
    for (auto engine : queue.bcsEngines) {
        if (engine != nullptr) {
            auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(engine->commandStreamReceiver);
            EXPECT_EQ(1u, csr->initDirectSubmissionCalled);
        }
    }
}

HWTEST_F(CommandQueueHwTest, givenCommandQueueWhenAskingForCacheFlushOnBcsThenReturnTrue) {
    auto pHwQ = static_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    EXPECT_TRUE(pHwQ->isCacheFlushForBcsRequired());
}

HWTEST_F(CommandQueueHwTest, givenBlockedMapBufferCallWhenMemObjectIsPassedToCommandThenItsRefCountIsBeingIncreased) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    auto currentRefCount = buffer.getRefInternalCount();

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(currentRefCount + 1, buffer.getRefInternalCount());

    ASSERT_NE(nullptr, pHwQ->virtualEvent);
    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
    EXPECT_EQ(currentRefCount, buffer.getRefInternalCount());
}

HWTEST_F(CommandQueueHwTest, givenNoReturnEventWhenCallingEnqueueBlockedMapUnmapOperationThenVirtualEventIncrementsCommandQueueInternalRefCount) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);

    MockBuffer buffer;
    pHwQ->virtualEvent = nullptr;

    auto initialRefCountInternal = pHwQ->getRefInternalCount();

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          &buffer,
                                          size, offset, false,
                                          eventBuilder);

    ASSERT_NE(nullptr, pHwQ->virtualEvent);

    auto refCountInternal = pHwQ->getRefInternalCount();
    EXPECT_EQ(initialRefCountInternal + 1, refCountInternal);

    pHwQ->virtualEvent->decRefInternal();
    pHwQ->virtualEvent = nullptr;
}

HWTEST_F(CommandQueueHwTest, WhenAddMapUnmapToWaitlistEventsThenDependenciesAreNotAddedIntoChild) {
    auto buffer = new MockBuffer;
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    auto returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    auto event = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    const cl_event eventWaitList = event;

    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(&eventWaitList,
                                          1,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);

    ASSERT_EQ(nullptr, event->peekChildEvents());

    // Release API refcount (i.e. from workload's perspective)
    returnEvent->release();
    event->decRefInternal();
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenMapCommandWhenZeroStateCommandIsSubmittedThenTaskCountIsNotBeingWaited) {
    auto buffer = new MockBuffer;
    MockCommandQueueHw<FamilyType> mockCmdQueueHw(context, pClDevice, nullptr);

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    mockCmdQueueHw.enqueueBlockedMapUnmapOperation(nullptr,
                                                   0,
                                                   MAP,
                                                   buffer,
                                                   size, offset, false,
                                                   eventBuilder);

    EXPECT_NE(nullptr, mockCmdQueueHw.virtualEvent);
    mockCmdQueueHw.virtualEvent->setStatus(CL_COMPLETE);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), mockCmdQueueHw.latestTaskCountWaited);

    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenMapCommandWhenZeroStateCommandIsSubmittedOnNonZeroCopyBufferThenTaskCountIsBeingWaited) {
    auto buffer = new MockBuffer;
    buffer->isZeroCopy = false;
    MockCommandQueueHw<FamilyType> mockCmdQueueHw(context, pClDevice, nullptr);

    MockEventBuilder eventBuilder;
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    mockCmdQueueHw.enqueueBlockedMapUnmapOperation(nullptr,
                                                   0,
                                                   MAP,
                                                   buffer,
                                                   size, offset, false,
                                                   eventBuilder);

    EXPECT_NE(nullptr, mockCmdQueueHw.virtualEvent);
    mockCmdQueueHw.virtualEvent->setStatus(CL_COMPLETE);
    EXPECT_EQ(1u, mockCmdQueueHw.latestTaskCountWaited);

    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenEventWhenEnqueuingBlockedMapUnmapOperationThenEventIsRetained) {
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    Event *returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    auto buffer = new MockBuffer;
    pHwQ->virtualEvent = nullptr;

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);
    eventBuilder.finalizeAndRelease();

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);
    EXPECT_NE(nullptr, returnEvent->peekCommand());
    // CommandQueue has retained this event, release it
    returnEvent->release();
    pHwQ->virtualEvent = nullptr;
    delete returnEvent; // NOLINT(clang-analyzer-cplusplus.NewDelete)
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenEventWhenEnqueuingBlockedMapUnmapOperationThenChildIsUnaffected) {
    auto buffer = new MockBuffer;
    CommandQueueHw<FamilyType> *pHwQ = reinterpret_cast<CommandQueueHw<FamilyType> *>(pCmdQ);
    Event *returnEvent = new Event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);
    Event event(pHwQ, CL_COMMAND_MAP_BUFFER, 0, 0);

    pHwQ->virtualEvent = nullptr;

    pHwQ->virtualEvent = &event;
    // virtual event from regular event to stored in previousVirtualEvent
    pHwQ->virtualEvent->incRefInternal();

    MockEventBuilder eventBuilder(returnEvent);
    MemObjSizeArray size = {{1, 1, 1}};
    MemObjOffsetArray offset = {{0, 0, 0}};
    pHwQ->enqueueBlockedMapUnmapOperation(nullptr,
                                          0,
                                          MAP,
                                          buffer,
                                          size, offset, false,
                                          eventBuilder);

    EXPECT_EQ(returnEvent, pHwQ->virtualEvent);
    ASSERT_EQ(nullptr, event.peekChildEvents());

    returnEvent->release();
    buffer->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenNonEmptyQueueOnBlockingWhenMappingBufferThenWillWaitForPrecedingCommandsToComplete) {
    struct MockCmdQ : CommandQueueHw<FamilyType> {
        MockCmdQ(Context *context, ClDevice *device)
            : CommandQueueHw<FamilyType>(context, device, 0, false) {
            finishWasCalled = false;
        }
        cl_int finish() override {
            finishWasCalled = true;
            return 0;
        }

        bool finishWasCalled;
    };

    MockCmdQ cmdQ(context, pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

    auto b1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);
    auto b2 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);

    auto gatingEvent = clCreateUserEvent(context, nullptr);
    void *ptr1 = clEnqueueMapBuffer(&cmdQ, b1, CL_FALSE, CL_MAP_READ, 0, 8, 1, &gatingEvent, nullptr, nullptr);
    clEnqueueUnmapMemObject(&cmdQ, b1, ptr1, 0, nullptr, nullptr);

    ASSERT_FALSE(cmdQ.finishWasCalled);

    void *ptr2 = clEnqueueMapBuffer(&cmdQ, b2, CL_TRUE, CL_MAP_READ, 0, 8, 0, nullptr, nullptr, nullptr);

    ASSERT_TRUE(cmdQ.finishWasCalled);

    clSetUserEventStatus(gatingEvent, CL_COMPLETE);

    clEnqueueUnmapMemObject(pCmdQ, b2, ptr2, 0, nullptr, nullptr);

    clReleaseMemObject(b1);
    clReleaseMemObject(b2);

    clReleaseEvent(gatingEvent);
}

HWTEST_F(CommandQueueHwTest, GivenEventsWaitlistOnBlockingWhenMappingBufferThenWillWaitForEvents) {
    struct MockEvent : UserEvent {
        MockEvent(Context *ctx, uint32_t updateCountBeforeCompleted)
            : UserEvent(ctx),
              updateCountBeforeCompleted(updateCountBeforeCompleted) {
            this->updateTaskCount(0, 0);
            this->taskLevel = 0;
        }

        void updateExecutionStatus() override {
            ++updateCount;
            if (updateCount == updateCountBeforeCompleted) {
                transitionExecutionStatus(CL_COMPLETE);
            }
            unblockEventsBlockedByThis(executionStatus);
        }

        uint32_t updateCount = 0;
        uint32_t updateCountBeforeCompleted;
    };

    MockEvent *me = new MockEvent(context, 1024);
    auto b1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 20, nullptr, nullptr);
    cl_event meAsClEv = me;
    void *ptr1 = clEnqueueMapBuffer(pCmdQ, b1, CL_TRUE, CL_MAP_READ, 0, 8, 1, &meAsClEv, nullptr, nullptr);
    ASSERT_TRUE(me->updateStatusAndCheckCompletion());
    ASSERT_LE(me->updateCountBeforeCompleted, me->updateCount);

    clEnqueueUnmapMemObject(pCmdQ, b1, ptr1, 0, nullptr, nullptr);
    clReleaseMemObject(b1);
    me->release();
}

HWTEST_F(CommandQueueHwTest, GivenNotCompleteUserEventPassedToEnqueueWhenEventIsUnblockedThenAllSurfacesForBlockedCommandsAreMadeResident) {
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCSR);

    auto userEvent = makeReleaseable<UserEvent>(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;

    size_t offset = 0;
    size_t size = 1;

    GraphicsAllocation *constantSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockCSR->getRootDeviceIndex(), MemoryConstants::pageSize});
    mockProgram->setConstantSurface(constantSurface);

    GraphicsAllocation *printfSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockCSR->getRootDeviceIndex(), MemoryConstants::pageSize});
    GraphicsAllocation *privateSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockCSR->getRootDeviceIndex(), MemoryConstants::pageSize});

    mockKernel->setPrivateSurface(privateSurface, 10);

    cl_event blockedEvent = userEvent.get();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    userEvent->setStatus(CL_COMPLETE);

    EXPECT_TRUE(mockCSR->isMadeResident(constantSurface));
    EXPECT_TRUE(mockCSR->isMadeResident(privateSurface));

    mockKernel->setPrivateSurface(nullptr, 0);
    mockProgram->setConstantSurface(nullptr);

    mockCSR->getMemoryManager()->freeGraphicsMemory(privateSurface);
    mockCSR->getMemoryManager()->freeGraphicsMemory(printfSurface);
    mockCSR->getMemoryManager()->freeGraphicsMemory(constantSurface);
}

HWTEST_F(CommandQueueHwTest, whenReleaseQueueCalledThenFlushIsCalled) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);
    mockCmdQ->incRefInternal();
    releaseQueue(mockCmdQ, retVal);
    EXPECT_TRUE(mockCmdQ->flushCalled);
    // this call will release the queue
    mockCmdQ->decRefInternal();
}

using BlockedCommandQueueTest = CommandQueueHwTest;

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWhenBlockedCommandIsBeingSubmittedThenQueueHeapsAreNotUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096u);

    uint32_t defaultSshUse = UnitTestHelper<FamilyType>::getDefaultSshUsage();

    EXPECT_EQ(0u, ioh.getUsed());
    EXPECT_EQ(0u, dsh.getUsed());
    EXPECT_EQ(defaultSshUse, ssh.getUsed());

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWithUsedHeapsWhenBlockedCommandIsBeingSubmittedThenQueueHeapsAreNotUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096u);

    auto spaceToUse = 4u;

    ioh.getSpace(spaceToUse);
    dsh.getSpace(spaceToUse);
    ssh.getSpace(spaceToUse);

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    uint32_t sshSpaceUse = spaceToUse + UnitTestHelper<FamilyType>::getDefaultSshUsage();

    EXPECT_EQ(spaceToUse, ioh.getUsed());
    EXPECT_EQ(spaceToUse, dsh.getUsed());
    EXPECT_EQ(sshSpaceUse, ssh.getUsed());

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenCommandQueueWhichHasSomeUnusedHeapsWhenBlockedCommandIsBeingSubmittedThenThoseHeapsAreBeingUsed) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 4096u);
    auto &dsh = pCmdQ->getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 4096u);
    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096u);

    auto iohBase = ioh.getCpuBase();
    auto dshBase = dsh.getCpuBase();
    auto sshBase = ssh.getCpuBase();

    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(iohBase, ioh.getCpuBase());
    EXPECT_EQ(dshBase, dsh.getCpuBase());
    EXPECT_EQ(sshBase, ssh.getCpuBase());

    pCmdQ->isQueueBlocked();
}

HWTEST_F(BlockedCommandQueueTest, givenEnqueueBlockedByUserEventWhenItIsEnqueuedThenKernelReferenceCountIsIncreased) {
    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    auto currentRefCount = mockKernel->getRefInternalCount();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    EXPECT_EQ(currentRefCount + 1, mockKernel->getRefInternalCount());
    userEvent.setStatus(CL_COMPLETE);
    pCmdQ->isQueueBlocked();
    EXPECT_EQ(currentRefCount, mockKernel->getRefInternalCount());
}

using CommandQueueHwRefCountTest = CommandQueueHwTest;

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWhenNewBlockedEnqueueReplacesVirtualEventThenPreviousVirtualEventDecrementsCmdQRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event blockedEvent = &userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increments cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // new virtual event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    userEvent.setStatus(CL_COMPLETE);
    // UserEvent is set to complete and event tree is unblocked, queue has only 1 refference to itself after this operation
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    // this call will release the queue
    releaseQueue(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenBlockedCmdQWithOutputEventAsVirtualEventWhenNewBlockedEnqueueReplacesVirtualEventCreatedFromOutputEventThenPreviousVirtualEventDoesntDecrementRefCount) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent userEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = &userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    // output event increments
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    // unblocking deletes 2 virtualEvents
    userEvent.setStatus(CL_COMPLETE);

    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();
    // releasing output event decrements refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());
    mockCmdQ->isQueueBlocked();

    releaseQueue(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenEveryEventIsDeletedAndCmdQIsReleasedThenCmdQIsDeleted) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    // output event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    // unblocking deletes 2 virtualEvents
    userEvent->setStatus(CL_COMPLETE);

    userEvent->release();
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();

    // releasing output event decrements refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->isQueueBlocked();

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    releaseQueue(mockCmdQ, retVal);
}

HWTEST_F(CommandQueueHwRefCountTest, givenSeriesOfBlockedEnqueuesWhenCmdQIsReleasedBeforeOutputEventThenOutputEventDeletesCmdQ) {
    cl_int retVal = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    UserEvent *userEvent = new UserEvent(context);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;

    cl_event eventOut = nullptr;
    cl_event blockedEvent = userEvent;

    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());
    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // UserEvent on waitlist doesn't increment cmdQ refCount, virtualEvent increments refCount
    EXPECT_EQ(2, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, &eventOut);

    // output event increments refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    mockCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);

    // previous virtualEvent which was outputEvent DOES NOT decrement refCount,
    // new virtual event increments refCount
    EXPECT_EQ(4, mockCmdQ->getRefInternalCount());

    userEvent->setStatus(CL_COMPLETE);

    userEvent->release();
    // releasing UserEvent doesn't change the queue refCount
    EXPECT_EQ(3, mockCmdQ->getRefInternalCount());

    releaseQueue(mockCmdQ, retVal);

    // releasing cmdQ decrements refCount
    EXPECT_EQ(1, mockCmdQ->getRefInternalCount());

    auto pEventOut = castToObject<Event>(eventOut);
    pEventOut->release();
}

HWTEST_F(CommandQueueHwTest, GivenEventThatIsNotCompletedWhenFinishIsCalledAndItGetsCompletedThenItStatusIsUpdatedAfterFinishCall) {
    cl_int ret;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    struct ClbFuncTempStruct {
        static void CL_CALLBACK clbFuncT(cl_event e, cl_int execStatus, void *valueForUpdate) {
            *((cl_int *)valueForUpdate) = 1;
        }
    };
    auto value = 0u;

    auto ev = new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, CompletionStamp::notReady + 1);
    clSetEventCallback(ev, CL_COMPLETE, ClbFuncTempStruct::clbFuncT, &value);

    auto &csr = this->pCmdQ->getGpgpuCommandStreamReceiver();
    EXPECT_GT(3u, csr.peekTaskCount());
    *csr.getTagAddress() = CompletionStamp::notReady + 1;
    ret = clFinish(this->pCmdQ);
    ASSERT_EQ(CL_SUCCESS, ret);

    ev->updateExecutionStatus();
    EXPECT_EQ(1u, value);
    ev->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, GivenMultiTileQueueWhenEventNotCompletedAndFinishIsCalledThenItGetsCompletedOnAllTilesAndItStatusIsUpdatedAfterFinishCall) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    auto &csr = this->pCmdQ->getGpgpuCommandStreamReceiver();
    csr.setActivePartitions(2u);
    auto ultCsr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(&csr);
    ultCsr->postSyncWriteOffset = 32;

    auto tagAddress = csr.getTagAddress();
    *ptrOffset(tagAddress, 32) = *tagAddress;

    struct ClbFuncTempStruct {
        static void CL_CALLBACK clbFuncT(cl_event e, cl_int execStatus, void *valueForUpdate) {
            *static_cast<cl_int *>(valueForUpdate) = 1;
        }
    };
    auto value = 0u;

    auto ev = new Event(this->pCmdQ, CL_COMMAND_COPY_BUFFER, 3, CompletionStamp::notReady + 1);
    clSetEventCallback(ev, CL_COMPLETE, ClbFuncTempStruct::clbFuncT, &value);
    EXPECT_GT(3u, csr.peekTaskCount());

    *tagAddress = CompletionStamp::notReady + 1;
    tagAddress = ptrOffset(tagAddress, 32);
    *tagAddress = CompletionStamp::notReady + 1;

    cl_int ret = clFinish(this->pCmdQ);
    ASSERT_EQ(CL_SUCCESS, ret);

    ev->updateExecutionStatus();
    EXPECT_EQ(1u, value);
    ev->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenCommandQueueThatIsBlockedAndUsesCpuCopyWhenEventIsReturnedThenItIsNotReady) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockBuffer buffer;
    cl_event returnEvent = nullptr;
    auto retVal = CL_SUCCESS;
    cmdQHw->taskLevel = CompletionStamp::notReady;
    size_t offset = 0;
    size_t size = 4096u;
    TransferProperties transferProperties(&buffer, CL_COMMAND_READ_BUFFER, 0, false, &offset, &size, nullptr, false, pDevice->getRootDeviceIndex());
    EventsRequest eventsRequest(0, nullptr, &returnEvent);
    cmdQHw->cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CompletionStamp::notReady, castToObject<Event>(returnEvent)->peekTaskCount());
    clReleaseEvent(returnEvent);
}

HWTEST_F(CommandQueueHwTest, givenEventWithRecordedCommandWhenSubmitCommandIsCalledThenTaskCountMustBeUpdatedFromOtherThread) {
    std::atomic_bool go{false};

    struct mockEvent : public Event {
        using Event::Event;
        using Event::eventWithoutCommand;
        using Event::submitCommand;
        void synchronizeTaskCount() override {
            *atomicFence = true;
            Event::synchronizeTaskCount();
        }
        uint32_t synchronizeCallCount = 0u;
        std::atomic_bool *atomicFence = nullptr;
    };

    mockEvent neoEvent(this->pCmdQ, CL_COMMAND_MAP_BUFFER, CompletionStamp::notReady, CompletionStamp::notReady);
    neoEvent.atomicFence = &go;
    EXPECT_TRUE(neoEvent.eventWithoutCommand);
    neoEvent.eventWithoutCommand = false;

    EXPECT_EQ(CompletionStamp::notReady, neoEvent.peekTaskCount());

    std::thread t([&]() {
        while (!go) {
        }
        neoEvent.updateTaskCount(77u, 0);
    });

    neoEvent.submitCommand(false);

    EXPECT_EQ(77u, neoEvent.peekTaskCount());
    t.join();
}

HWTEST_F(CommandQueueHwTest, givenNonBlockedEnqueueWhenEventIsPassedThenUpdateItsFlushStamp) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    size_t offset = 0;
    size_t size = 1;

    cl_event event;
    auto retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 0, nullptr, &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto eventObj = castToObject<Event>(event);
    EXPECT_EQ(csr.flushStamp->peekStamp(), eventObj->flushStamp->peekStamp());
    EXPECT_EQ(csr.flushStamp->peekStamp(), pCmdQ->flushStamp->peekStamp());
    eventObj->release();
}

HWTEST_F(CommandQueueHwTest, givenBlockedEnqueueWhenEventIsPassedThenDontUpdateItsFlushStamp) {
    UserEvent userEvent;
    cl_event event, clUserEvent;
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.flushStamp->setStamp(5);

    size_t offset = 0;
    size_t size = 1;

    clUserEvent = &userEvent;
    auto retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 1, &clUserEvent, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(cmdQHw->isQueueBlocked());

    retVal = cmdQHw->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, nullptr, 0, nullptr, &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    FlushStamp expectedFlushStamp = 0;
    auto eventObj = castToObject<Event>(event);
    EXPECT_EQ(expectedFlushStamp, eventObj->flushStamp->peekStamp());
    EXPECT_EQ(expectedFlushStamp, pCmdQ->flushStamp->peekStamp());

    eventObj->release();
}

HWTEST_F(CommandQueueHwTest, givenBlockedInOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    auto event = new Event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, 10, 0);

    uint32_t virtualEventTaskLevel = 77;
    uint32_t virtualEventTaskCount = 80;
    auto virtualEvent = new Event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, virtualEventTaskLevel, virtualEventTaskCount);

    cl_event blockedEvent = event;

    // Put Queue in blocked state by assigning virtualEvent
    event->addChild(*virtualEvent);
    virtualEvent->incRefInternal();
    cmdQHw->virtualEvent = virtualEvent;

    *mockCSR->getTagAddress() = 0u;
    cmdQHw->taskLevel = 23;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    // new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, virtualEvent);

    EXPECT_EQ(virtualEvent->peekExecutionStatus(), CL_QUEUED);
    event->setStatus(CL_SUBMITTED);
    EXPECT_EQ(virtualEvent->peekExecutionStatus(), CL_SUBMITTED);

    EXPECT_FALSE(cmdQHw->isQueueBlocked());
    // +1 for next level after virtualEvent is unblocked
    // +1 as virtualEvent was a parent for event with actual command that is being submitted
    EXPECT_EQ(virtualEventTaskLevel + 2, cmdQHw->taskLevel);
    // command being submitted was dependant only on virtual event hence only +1
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
    *mockCSR->getTagAddress() = initialHardwareTag;
    virtualEvent->decRefInternal();
    event->decRefInternal();
}

HWTEST_F(CommandQueueHwTest, givenBlockedOutOfOrderQueueWhenUserEventIsSubmittedThenNDREventIsSubmittedAsWell) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    cl_event userEvent = clCreateUserEvent(this->pContext, nullptr);
    cl_event blockedEvent = nullptr;

    *mockCsr.getTagAddress() = 0u;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &userEvent, &blockedEvent);

    auto neoEvent = castToObject<Event>(blockedEvent);
    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_QUEUED);

    neoEvent->updateExecutionStatus();

    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_QUEUED);
    EXPECT_EQ(neoEvent->peekTaskCount(), CompletionStamp::notReady);

    clSetUserEventStatus(userEvent, 0u);

    EXPECT_EQ(neoEvent->peekExecutionStatus(), CL_SUBMITTED);
    EXPECT_EQ(neoEvent->peekTaskCount(), 1u);

    *mockCsr.getTagAddress() = initialHardwareTag;
    clReleaseEvent(blockedEvent);
    clReleaseEvent(userEvent);
}

HWTEST_F(CommandQueueHwTest, givenWalkerSplitEnqueueNDRangeWhenNoBlockedThenKernelMakeResidentCalledOnce) {
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;
    mockProgram->setAllowNonUniform(true);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    size_t offset = 0;
    size_t gws = 63;
    size_t lws = 16;

    cl_int status = pCmdQ->enqueueKernel(mockKernel, 1, &offset, &gws, &lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel->makeResidentCalls);
}

HWTEST_F(CommandQueueHwTest, givenWalkerSplitEnqueueNDRangeWhenBlockedThenKernelGetResidencyCalledOnce) {
    UserEvent userEvent(context);
    KernelInfo kernelInfo;
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    auto mockProgram = mockKernelWithInternals.mockProgram;
    mockProgram->setAllowNonUniform(true);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    size_t offset = 0;
    size_t gws = 63;
    size_t lws = 16;

    cl_event blockedEvent = &userEvent;

    cl_int status = pCmdQ->enqueueKernel(mockKernel, 1, &offset, &gws, &lws, 1, &blockedEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel->getResidencyCalls);

    userEvent.setStatus(CL_COMPLETE);
    pCmdQ->isQueueBlocked();
}

HWTEST_F(CommandQueueHwTest, givenKernelSplitEnqueueReadBufferWhenBlockedThenEnqueueSurfacesMakeResidentIsCalledOnce) {
    UserEvent userEvent(context);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    csr.timestampPacketWriteEnabled = false;

    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    GraphicsAllocation *bufferAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_event blockedEvent = &userEvent;

    cl_int status = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, MemoryConstants::cacheLineSize, ptr, nullptr, 1, &blockedEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    userEvent.setStatus(CL_COMPLETE);

    std::map<GraphicsAllocation *, uint32_t>::iterator it = csr.makeResidentAllocations.begin();
    for (; it != csr.makeResidentAllocations.end(); it++) {
        uint32_t expected = 1u;
        // Buffer surface will be added three times (for each kernel from split and as a base range of enqueueReadBuffer call)
        if (it->first == bufferAllocation) {
            expected = 3u;
        }
        EXPECT_EQ(expected, it->second);
    }

    pCmdQ->isQueueBlocked();
}

HWTEST_F(CommandQueueHwTest, givenDefaultHwCommandQueueThenCacheFlushAfterWalkerIsNotNeeded) {
    EXPECT_FALSE(pCmdQ->getRequiresCacheFlushAfterWalker());
}

HWTEST_F(CommandQueueHwTest, givenSizeWhenForceStatelessIsCalledThenCorrectValueIsReturned) {

    if (is32bit) {
        GTEST_SKIP();
    }

    struct MockCommandQueueHw : public CommandQueueHw<FamilyType> {
        using CommandQueueHw<FamilyType>::forceStateless;
    };

    MockCommandQueueHw *pCmdQHw = reinterpret_cast<MockCommandQueueHw *>(pCmdQ);
    uint64_t bigSize = 4ull * MemoryConstants::gigaByte;
    EXPECT_TRUE(pCmdQHw->forceStateless(static_cast<size_t>(bigSize)));

    uint64_t smallSize = bigSize - 1;
    EXPECT_FALSE(pCmdQHw->forceStateless(static_cast<size_t>(smallSize)));
}

HWTEST_F(CommandQueueHwTest, givenFlushWhenFlushBatchedSubmissionsFailsThenErrorIsRetured) {
    MockCommandQueueHwWithOverwrittenCsr<FamilyType> cmdQueue(context, pClDevice, nullptr, false);
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission csr(*pDevice->executionEnvironment, 0, pDevice->getDeviceBitfield());
    cmdQueue.csr = &csr;
    cl_int errorCode = cmdQueue.flush();
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errorCode);
}

HWTEST_F(CommandQueueHwTest, givenFinishWhenFlushBatchedSubmissionsFailsThenErrorIsRetured) {
    MockCommandQueueHwWithOverwrittenCsr<FamilyType> cmdQueue(context, pClDevice, nullptr, false);
    MockCommandStreamReceiverWithFailingFlushBatchedSubmission csr(*pDevice->executionEnvironment, 0, pDevice->getDeviceBitfield());
    cmdQueue.csr = &csr;
    cl_int errorCode = cmdQueue.finish();
    EXPECT_EQ(CL_OUT_OF_RESOURCES, errorCode);
}

HWTEST_F(CommandQueueHwTest, givenGpuHangWhenFinishingCommandQueueHwThenWaitForEnginesIsCalledAndOutOfResourcesIsReturned) {
    MockCommandQueueHw<FamilyType> mockCmdQueueHw{context, pClDevice, nullptr};

    mockCmdQueueHw.waitForAllEnginesReturnValue = WaitStatus::GpuHang;
    mockCmdQueueHw.getUltCommandStreamReceiver().shouldFlushBatchedSubmissionsReturnSuccess = true;

    const auto finishResult = mockCmdQueueHw.finish();
    EXPECT_EQ(1, mockCmdQueueHw.waitForAllEnginesCalledCount);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, finishResult);
}

HWTEST_F(CommandQueueHwTest, givenNoGpuHangWhenFinishingCommandQueueHwThenWaitForEnginesIsCalledAndSuccessIsReturned) {
    MockCommandQueueHw<FamilyType> mockCmdQueueHw{context, pClDevice, nullptr};

    mockCmdQueueHw.waitForAllEnginesReturnValue = WaitStatus::Ready;
    mockCmdQueueHw.getUltCommandStreamReceiver().shouldFlushBatchedSubmissionsReturnSuccess = true;

    const auto finishResult = mockCmdQueueHw.finish();
    EXPECT_EQ(1, mockCmdQueueHw.waitForAllEnginesCalledCount);
    EXPECT_EQ(CL_SUCCESS, finishResult);
}
