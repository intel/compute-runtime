/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_printf_handler.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
using CommandStreamReceiverFlushTaskTestsWithMockCsrHw2 = UltCommandStreamReceiverTestWithCsrT<MockCsrHw2>;

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenFlushTaskIsCalledThenNothingIsSubmittedToTheHwAndSubmissionIsRecorded) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    if (mockCsr->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->getResidencyAllocations().clear();

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    auto cmdBuffer = cmdBufferList.peekHead();
    // two more because of preemption allocation and sipKernel in Mid Thread preemption mode
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount -= pDevice->getHardwareInfo().capabilityTable.supportsImages ? 0 : 1;
    csrSurfaceCount += mockCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += mockCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += pDevice->getProductHelper().getCommandBuffersPreallocatedPerCommandQueue() > 0 ? 0 : 1;
    csrSurfaceCount += pDevice->getRTMemoryBackedBuffer() ? 1 : 0;

    // we should have 3 heaps, tag allocation and csr command stream + cq
    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    EXPECT_EQ(0u, mockCsr->flushCalledCount);

    // we should be submitting via csr
    EXPECT_EQ(cmdBuffer->batchBuffer.commandBufferAllocation, mockCsr->commandStream.getGraphicsAllocation());
    EXPECT_EQ(cmdBuffer->batchBuffer.startOffset, 0u);
    EXPECT_FALSE(cmdBuffer->batchBuffer.lowPriority);

    // find BB END
    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(bbEndAddress, cmdBuffer->batchBufferEndLocation);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenTaskStreamWhenFlushingThenStoreTaskStartAddress) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    size_t startOffset = 0x1234;
    mockCsr->flushTask(commandStream, startOffset, &dsh, &ioh, &ssh, taskLevel, dispatchFlags, *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    auto cmdBuffer = cmdBufferList.peekHead();

    uint64_t expectedAddress = commandStream.getGpuBase() + startOffset;

    EXPECT_EQ(cmdBuffer->batchBuffer.taskStartAddress, expectedAddress);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeAndTwoRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto secondBatchBuffer = primaryBatch->next;

    auto bbEndLocation = primaryBatch->batchBufferEndLocation;
    auto secondBatchBufferAddress = (uint64_t)ptrOffset(secondBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                        secondBatchBuffer->batchBuffer.startOffset);

    auto lastbbEndPtr = secondBatchBuffer->batchBuffer.endCmdPtr;

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(secondBatchBufferAddress, batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.endCmdPtr, lastbbEndPtr);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenFlushSmallTaskThenCommandStreamAlignedToCacheLine) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto usedBefore = csr.commandStream.getUsed();

    auto &stream = csr.getCS(2 * MemoryConstants::cacheLineSize);
    stream.getSpace(MemoryConstants::cacheLineSize - sizeof(MI_BATCH_BUFFER_END) - 2);
    EXPECT_EQ(SubmissionStatus::success, csr.flushSmallTask(stream, stream.getUsed()));

    auto usedAfter = csr.commandStream.getUsed();

    auto used = usedAfter - usedBefore;
    auto expected = 2 * MemoryConstants::cacheLineSize;
    EXPECT_EQ(used, expected);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeAndThreeRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto lastBatchBuffer = primaryBatch->next->next;

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;
    auto lastBatchBufferAddress = (uint64_t)ptrOffset(lastBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                      lastBatchBuffer->batchBuffer.startOffset);

    auto lastbbEndPtr = lastBatchBuffer->batchBuffer.endCmdPtr;

    auto flushCalledCountBeforeFlush = mockCsr->flushCalledCount;

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(lastBatchBufferAddress, batchBufferStart->getBatchBufferStartAddress());
    EXPECT_EQ(flushCalledCountBeforeFlush + 1u, mockCsr->flushCalledCount);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.endCmdPtr, lastbbEndPtr);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeAndThreeRecordedCommandBuffersThatUsesAllResourceWhenFlushTaskIsCalledThenBatchBuffersAreNotCombined) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->heaplessStateInitialized = true;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto memorySize = (size_t)pDevice->getDeviceInfo().globalMemSize;
    MockGraphicsAllocation largeAllocation(nullptr, memorySize);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_EQ(nullptr, batchBufferStart);
    auto bbEnd = genCmdCast<MI_BATCH_BUFFER_END *>(bbEndLocation);
    EXPECT_NE(nullptr, bbEnd);

    EXPECT_EQ(3u, mockCsr->flushCalledCount);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenFlushTaskIsCalledTwiceThenNothingIsSubmittedToTheHwAndTwoSubmissionAreRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto initialBase = commandStream.getCpuBase();
    auto initialUsed = commandStream.getUsed();

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    // ensure command stream still used
    EXPECT_EQ(initialBase, commandStream.getCpuBase());
    auto baseAfterFirstFlushTask = commandStream.getCpuBase();
    auto usedAfterFirstFlushTask = commandStream.getUsed();

    dispatchFlags.lowPriority = true;

    mockCsr->flushTask(commandStream,
                       commandStream.getUsed(),
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto baseAfterSecondFlushTask = commandStream.getCpuBase();
    auto usedAfterSecondFlushTask = commandStream.getUsed();
    EXPECT_EQ(initialBase, commandStream.getCpuBase());

    EXPECT_EQ(baseAfterSecondFlushTask, baseAfterFirstFlushTask);
    EXPECT_EQ(baseAfterFirstFlushTask, initialBase);

    EXPECT_GT(usedAfterFirstFlushTask, initialUsed);
    EXPECT_GT(usedAfterSecondFlushTask, usedAfterFirstFlushTask);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_NE(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    EXPECT_NE(nullptr, cmdBufferList.peekTail());
    EXPECT_NE(nullptr, cmdBufferList.peekHead());

    auto cmdBuffer1 = cmdBufferList.peekHead();
    auto cmdBuffer2 = cmdBufferList.peekTail();

    EXPECT_GT(cmdBuffer2->batchBufferEndLocation, cmdBuffer1->batchBufferEndLocation);

    EXPECT_FALSE(cmdBuffer1->batchBuffer.lowPriority);
    EXPECT_TRUE(cmdBuffer2->batchBuffer.lowPriority);

    EXPECT_GT(cmdBuffer2->batchBuffer.startOffset, cmdBuffer1->batchBuffer.startOffset);
}

HWCMDTEST_TEMPLATED_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(false);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->streamProperties.stateComputeMode.isCoherencyRequired.value = 0;

    commandStream.getSpace(4);

    mockCsr->flushTask(commandStream,
                       4,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0u, mockCsr->flushCalledCount);

    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());
    auto cmdBuffer = cmdBufferList.peekHead();

    // preemption allocation + sip kernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += mockCsr->globalFenceAllocation ? 1 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    // copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
        EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(mockCsr->getOsContext().getContextId()));
    }

    mockCsr->flushBatchedSubmissions();

    EXPECT_FALSE(mockCsr->recordedCommandBuffer->batchBuffer.lowPriority);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.commandBufferAllocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(4u, mockCsr->recordedCommandBuffer->batchBuffer.startOffset);
    EXPECT_EQ(1u, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());

    EXPECT_EQ(0u, surfacesForResidency.size());

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_FALSE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrCreatedWithDedicatedDebugFlagWhenItIsCreatedThenItHasProperDispatchMode) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::adaptiveDispatch));
    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(DispatchMode::adaptiveDispatch, mockCsr->dispatchMode);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenBlockingCommandIsSendThenItIsFlushedAndNotBatched) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;
    mockCsr->flushCalledCount = 0;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(false);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.blocking = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    auto flushCalledCountBeforeFlush = mockCsr->flushCalledCount;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(flushCalledCountBeforeFlush + 1u, mockCsr->flushCalledCount);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenBufferToFlushWhenFlushTaskCalledThenUpdateFlushStamp) {
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->heaplessStateInitialized = true;

    commandStream.getSpace(1);
    EXPECT_EQ(0u, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_GT(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), cmplStamp.flushStamp);
    EXPECT_EQ(1u, mockCsr->flushCalledCount);
}

HWCMDTEST_TEMPLATED_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    configureCSRtoNonDirtyState<FamilyType>(false);

    EXPECT_EQ(0u, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0u, mockCsr->flushCalledCount);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenFlushTaskIsCalledThenFlushedTaskCountIsNotModifed) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitWhenFlushBatchedIsCalledThenFlushedTaskCountIsNotModifed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitEnabledAndHeapAllocationDirtyWhenFlushTaskThenTaskCountIsFlushed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::immediateDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->heapStorageRequiresRecyclingTag = true;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = false;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    EXPECT_NE(itorPipeControl, cmdList.end());
    auto cmd = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_EQ(cmd->getDcFlushEnable(), mockCsr->getDcFlushSupport());
    EXPECT_TRUE(cmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(cmd->getTextureCacheInvalidationEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitEnabledAndBlockingFlagWhenFlushTaskThenTaskCountIsFlushed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->overrideDispatchPolicy(DispatchMode::immediateDispatch);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = false;
    dispatchFlags.blocking = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    EXPECT_NE(itorPipeControl, cmdList.end());
    auto cmd = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_EQ(cmd->getDcFlushEnable(), mockCsr->getDcFlushSupport());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInDefaultModeWhenFlushTaskIsCalledThenFlushedTaskCountIsModifed) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    bool heaplessStateInit = commandQueue.getHeaplessStateInitEnabled();

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    auto &csr = commandQueue.getGpgpuCommandStreamReceiver();

    flushTaskMethod<FamilyType>(csr, commandStream, 0, &dsh, &ioh, &ssh, taskLevel, dispatchFlags, *pDevice);

    EXPECT_EQ(heaplessStateInit ? 2u : 1u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, csr.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledGivenNumberOfTimesThenFlushIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PerformImplicitFlushEveryEnqueueCount.set(2);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    auto &csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(commandQueue.getGpgpuCommandStreamReceiver());
    csr.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    csr.useNewResourceImplicitFlush = false;
    csr.useGpuIdleImplicitFlush = false;
    dispatchFlags.implicitFlush = false;

    flushTaskMethod<FamilyType>(csr, commandStream, 0, &dsh, &ioh, &ssh, taskLevel, dispatchFlags, *pDevice);

    EXPECT_EQ(csr.heaplessStateInitialized ? 2u : 1u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(csr.heaplessStateInitialized ? 2u : 0u, csr.peekLatestFlushedTaskCount());
    dispatchFlags.implicitFlush = false;

    flushTaskMethod<FamilyType>(csr, commandStream, 0, &dsh, &ioh, &ssh, taskLevel, dispatchFlags, *pDevice);

    EXPECT_EQ(csr.heaplessStateInitialized ? 3u : 2u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(2u, csr.peekLatestFlushedTaskCount());

    dispatchFlags.implicitFlush = false;

    flushTaskMethod<FamilyType>(csr, commandStream, 0, &dsh, &ioh, &ssh, taskLevel, dispatchFlags, *pDevice);

    EXPECT_EQ(csr.heaplessStateInitialized ? 4u : 3u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(csr.heaplessStateInitialized ? 4u : 2u, csr.peekLatestFlushedTaskCount());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenWaitForTaskCountIsCalledWithTaskCountThatWasNotYetFlushedThenBatchedCommandBuffersAreSubmitted) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    auto cmdBuffer = cmdBufferList.peekHead();
    EXPECT_EQ(1u, cmdBuffer->taskCount);

    mockCsr->waitForCompletionWithTimeout(WaitParams{false, false, false, 1}, 1);

    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());

    EXPECT_TRUE(cmdBufferList.peekIsEmpty());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenEnqueueIsMadeThenCurrentMemoryUsedIsTracked) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->getResidencyAllocations().clear();
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    EXPECT_EQ(expectedUsed, mockCsr->peekTotalMemoryUsed());

    // after flush it goes to 0
    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenSusbsequentEnqueueIsMadeThenOnlyNewResourcesAreTrackedForMemoryUsage) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->getResidencyAllocations().clear();
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    auto additionalSize = 1234;

    MockGraphicsAllocation graphicsAllocation(nullptr, additionalSize);
    mockCsr->makeResident(graphicsAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(expectedUsed + additionalSize, mockCsr->peekTotalMemoryUsed());
    mockCsr->flushBatchedSubmissions();
}

struct MockedMemoryManager : public OsAgnosticMemoryManager {
    MockedMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}
    bool isMemoryBudgetExhausted() const override { return budgetExhausted; }
    bool budgetExhausted = false;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTotalResourceUsedExhaustsTheBudgetThenDoImplicitFlush) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto mockedMemoryManager = new MockedMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(mockedMemoryManager);
    auto mockCsr = std::make_unique<MockCsrHw2<FamilyType>>(*executionEnvironment, 0, pDevice->getDeviceBitfield());
    mockCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    if (pDevice->getPreemptionMode() == PreemptionMode::MidThread) {
        mockCsr->createPreemptionAllocation();
    }

    mockCsr->initializeTagAllocation();
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    mockedMemoryManager->budgetExhausted = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    EXPECT_EQ(expectedUsed, mockCsr->peekTotalMemoryUsed());

    auto budgetSize = (size_t)pDevice->getDeviceInfo().globalMemSize;
    MockGraphicsAllocation hugeAllocation(nullptr, budgetSize / 4);
    mockCsr->makeResident(hugeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    // expect 2 flushes, since we cannot batch those submissions
    EXPECT_EQ(2u, mockCsr->peekLatestFlushedTaskCount());
    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2,
                   givenCsrInBatchingModeWhenTwoTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    using MI_NOOP = typename FamilyType::MI_NOOP;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    MI_NOOP *pCmd = static_cast<MI_NOOP *>(commandStream.getSpace(sizeof(MI_NOOP)));
    *pCmd = FamilyType::cmdInitNoop;
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    // now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    // validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(firstCmdBuffer->pipeControlThatMayBeErasedLocation, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc);
    auto ppc2 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc2);

    // flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    // decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    // make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    if (mockCsr->getHeaplessStateInitEnabled()) {
        EXPECT_EQ(cmdList.end(), itorBatchBufferStartSecond);
    } else {
        EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
        EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
        auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
        EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);
    }

    // first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    EXPECT_NE(nullptr, noop1);
    EXPECT_EQ(nullptr, noop2);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc2);
    EXPECT_NE(nullptr, ppcAfterChange);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenDcFlushIsNotRequiredThenItIsNotSet) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()), pipeControl->getDcFlushEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWithOutOfOrderModeFisabledWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()), pipeControl->getDcFlushEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitSetAndGuardCommandBufferWithPipeControlWhenFlushTaskThenThereIsPipeControlForUpdateTaskCount) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(itorPipeControl, cmdList.end());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitSetWhenFlushTaskThenThereIsNoPipeControlForUpdateTaskCount) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(itorPipeControl, cmdList.end());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenUpdateTaskCountFromWaitSetWhenFlushTaskThenPipeControlIsFlushed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    MockContext context(pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&context, pClDevice, 0, false);
    commandQueue.taskCount = 10;

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->taskCount.store(10);
    mockCsr->latestFlushedTaskCount.store(5);

    const auto waitStatus = commandQueue.waitForAllEngines(false, nullptr);
    EXPECT_EQ(WaitStatus::ready, waitStatus);

    parseCommands<FamilyType>(mockCsr->getCS(4096u));
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(itorPipeControl, cmdList.end());
    EXPECT_EQ(mockCsr->flushCalledCount, 1u);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenGpuHangOnPrintEnqueueOutputWhenWaitingForEnginesThenGpuHangIsReported) {
    MockCommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, nullptr);
    commandQueue.waitUntilCompleteReturnValue = WaitStatus::ready;

    const auto blockedQueue{false};
    const auto cleanTemporaryAllocationsList{false};
    MockPrintfHandler printfHandler(*pDevice);

    const auto waitStatus = commandQueue.waitForAllEngines(blockedQueue, &printfHandler, cleanTemporaryAllocationsList);
    EXPECT_EQ(WaitStatus::gpuHang, waitStatus);
}

template <typename FamilyType>
struct MockCsrHwDirectSubmission : public MockCsrHw2<FamilyType> {
    using MockCsrHw2<FamilyType>::MockCsrHw2;
    bool isDirectSubmissionEnabled() const override {
        return true;
    }
};

using CommandStreamReceiverFlushTaskTestsWithCustomCsr = UltCommandStreamReceiverTestWithCsrT<MockCsrHwDirectSubmission>;

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithCustomCsr, givenEnabledDirectSubmissionUpdateTaskCountFromWaitSetWhenFlushTaskThenPipeControlAndBBSIsFlushed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(1);

    MockContext context(pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&context, pClDevice, 0, false);
    commandQueue.taskCount = 10;

    auto mockCsr = static_cast<MockCsrHwDirectSubmission<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->taskCount.store(10);
    mockCsr->latestFlushedTaskCount.store(5);
    mockCsr->directSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(*mockCsr);

    const auto waitStatus = commandQueue.waitForAllEngines(false, nullptr);
    EXPECT_EQ(WaitStatus::ready, waitStatus);

    parseCommands<FamilyType>(mockCsr->getCS(4096u));
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto itorBBS = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(itorPipeControl, cmdList.end());
    EXPECT_NE(itorBBS, cmdList.end());
    EXPECT_EQ(mockCsr->flushCalledCount, 1u);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenDcFlushIsRequiredThenPipeControlIsNotRegistredForNooping) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.dcFlush = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenEpiloguePipeControlThenDcFlushIsEnabled) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    ASSERT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer->epiloguePipeControlLocation);
    ASSERT_NE(nullptr, pipeControl);
    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment())) {
        itorPipeControl++;
    }
    auto pipeControlCmdBuffer = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()), pipeControlCmdBuffer->getDcFlushEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenEpiloguePipeControlWhendDcFlushDisabledByDebugFlagThenDcFlushIsDisabled) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.DisableDcFlushInEpilogue.set(true);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    ASSERT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(cmdBuffer->epiloguePipeControlLocation);
    ASSERT_NE(nullptr, pipeControl);
    mockCsr->flushBatchedSubmissions();
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2,
                   givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTwoTasksArePassedWithTheSameLevelThenThereIsPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->timestampPacketWriteEnabled = false;
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    // now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    // validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    // decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTimestampPacketWriteIsEnabledThenNoopPipeControl) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->timestampPacketWriteEnabled = false;
    mockCsr->flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, &dsh, &ioh, &ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCsrInBatchingModeWhenPipeControlForNoopAddressIsNullThenPipeControlIsNotNooped) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    // now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    // validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto ppc1Location = firstCmdBuffer->pipeControlThatMayBeErasedLocation;

    firstCmdBuffer->pipeControlThatMayBeErasedLocation = nullptr;

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc1Location);
    EXPECT_NE(nullptr, ppc);

    // call flush, both pipe controls must remain untouched
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    // decode commands to confirm no pipe controls between Walkers
    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2,
                   givenCsrInBatchingModeWhenThreeTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    // now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    // validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto secondCmdBuffer = firstCmdBuffer->next;
    auto thirdCmdBuffer = firstCmdBuffer->next->next;

    EXPECT_NE(nullptr, thirdCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(firstCmdBuffer->pipeControlThatMayBeErasedLocation, thirdCmdBuffer->pipeControlThatMayBeErasedLocation);

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto ppc2 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto ppc3 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(thirdCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc2);
    EXPECT_NE(nullptr, ppc3);

    // flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    // decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    // make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
    EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);

    if (!mockCsr->getHeaplessStateInitEnabled()) {
        auto itorBatchBufferStartThird = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartSecond, cmdList.end());
        EXPECT_NE(cmdList.end(), itorBatchBufferStartThird);
        EXPECT_NE(itorBatchBufferStartThird, itorBatchBufferStartSecond);

        itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartSecond, itorBatchBufferStartThird);
        EXPECT_EQ(itorPipeControl, itorBatchBufferStartThird);
    }

    // first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    auto noop3 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc3);

    EXPECT_NE(nullptr, noop1);
    EXPECT_NE(nullptr, noop2);
    EXPECT_EQ(nullptr, noop3);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc3);
    EXPECT_NE(nullptr, ppcAfterChange);
}

using CommandStreamReceiverCleanupTests = UltCommandStreamReceiverTest;
HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenTemporaryAndReusableAllocationsArePresentThenCleanupResourcesOnlyCleansThoseAboveLatestFlushTaskLevel) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = pDevice->getMemoryManager();

    auto temporaryToClean = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto temporaryToHold = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto reusableToClean = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto reusableToHold = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToClean), TEMPORARY_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToHold), TEMPORARY_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToClean), REUSABLE_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToHold), REUSABLE_ALLOCATION);

    auto osContextId = commandStreamReceiver.getOsContext().getContextId();

    temporaryToClean->updateTaskCount(1, osContextId);
    reusableToClean->updateTaskCount(1, osContextId);

    temporaryToHold->updateTaskCount(10, osContextId);
    reusableToHold->updateTaskCount(10, osContextId);

    commandStreamReceiver.latestFlushedTaskCount = 9;
    commandStreamReceiver.cleanupResources();

    EXPECT_EQ(reusableToHold, commandStreamReceiver.getAllocationsForReuse().peekHead());
    EXPECT_EQ(reusableToHold, commandStreamReceiver.getAllocationsForReuse().peekTail());

    EXPECT_EQ(temporaryToHold, commandStreamReceiver.getTemporaryAllocations().peekHead());
    EXPECT_EQ(temporaryToHold, commandStreamReceiver.getTemporaryAllocations().peekTail());

    commandStreamReceiver.latestFlushedTaskCount = 11;
    commandStreamReceiver.cleanupResources();
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getTemporaryAllocations().peekIsEmpty());
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenDispatchFlagsWithThrottleSetToLowWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    commandStream.getSpace(16u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.throttle = QueueThrottle::LOW;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::LOW);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenDispatchFlagsWithThrottleSetToMediumWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    commandStream.getSpace(16u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.throttle = QueueThrottle::MEDIUM;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::MEDIUM);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenCommandQueueWithThrottleHintWhenFlushingThenPassThrottleHintToCsr) {
    MockContext context(pClDevice);
    cl_queue_properties properties[] = {CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_LOW_KHR, 0};
    CommandQueueHw<FamilyType> commandQueue(&context, pClDevice, properties, false);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, 0, 1, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;

    uint32_t outPtr;
    commandQueue.enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 1, &outPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(QueueThrottle::LOW, mockCsr->passedDispatchFlags.throttle);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenDispatchFlagsWithThrottleSetToHighWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    commandStream.getSpace(16u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.throttle = QueueThrottle::HIGH;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::HIGH);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandStreamReceiverFlushTaskTests, givenEpilogueRequiredFlagWhenTaskIsSubmittedDirectlyThenItPointsBackToCsr) {
    configureCSRtoNonDirtyState<FamilyType>(false);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    dispatchFlags.epilogueRequired = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    auto data = commandStream.getSpace(MemoryConstants::cacheLineSize);
    memset(data, 0, MemoryConstants::cacheLineSize);
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);
    auto &commandStreamReceiverStream = commandStreamReceiver.getCS(0u);

    EXPECT_EQ(MemoryConstants::cacheLineSize * 2, commandStream.getUsed());
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiverStream.getUsed());

    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(itBBend, cmdList.end());

    auto itBatchBufferStart = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itBatchBufferStart, cmdList.end());

    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(*itBatchBufferStart);
    EXPECT_EQ(batchBufferStart->getBatchBufferStartAddress(), commandStreamReceiverStream.getGraphicsAllocation()->getGpuAddress());

    parseCommands<FamilyType>(commandStreamReceiverStream, 0);

    itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(commandStreamReceiverStream.getCpuBase(), bbEndAddress);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiverStream.getGraphicsAllocation()));
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenDispatchFlagsWithNewSliceCountWhenFlushTaskThenNewSliceCountIsSet) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    commandStream.getSpace(16u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    uint64_t newSliceCount = 1;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.sliceCount = newSliceCount;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.sliceCount, newSliceCount);
}

template <typename GfxFamily>
class UltCommandStreamReceiverForDispatchFlags : public UltCommandStreamReceiver<GfxFamily> {
    using BaseClass = UltCommandStreamReceiver<GfxFamily>;

  public:
    UltCommandStreamReceiverForDispatchFlags(ExecutionEnvironment &executionEnvironment,
                                             const DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, 0, deviceBitfield) {}
    UltCommandStreamReceiverForDispatchFlags(ExecutionEnvironment &executionEnvironment,
                                             uint32_t rootDeviceIndex,
                                             const DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        savedDispatchFlags = dispatchFlags;
        return BaseClass::flushTask(commandStream, commandStreamStart,
                                    dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    DispatchFlags savedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
};

using CommandStreamReceiverFlushTaskTestsWithUltCsrForDispatchFlags = UltCommandStreamReceiverTestWithCsrT<UltCommandStreamReceiverForDispatchFlags>;

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithUltCsrForDispatchFlags, GivenBlockedKernelWhenItIsUnblockedThenDispatchFlagsAreSetCorrectly) {
    MockContext mockContext;
    auto csr = static_cast<UltCommandStreamReceiverForDispatchFlags<FamilyType> *>(&pDevice->getUltCommandStreamReceiver<FamilyType>());
    uint32_t numGrfRequired = 666u;

    auto pCmdQ = std::make_unique<MockCommandQueue>(&mockContext, pClDevice, nullptr, false);
    auto mockProgram = std::make_unique<MockProgram>(&mockContext, false, toClDeviceVector(*pClDevice));

    auto pKernel = MockKernel::create(*pDevice, mockProgram.get(), numGrfRequired);
    auto kernelInfos = MockKernel::toKernelInfoContainer(pKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(pKernel), kernelInfos);
    auto event = std::make_unique<MockEvent<Event>>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::commandBuffer, pDevice->getDeviceBitfield()}));

    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 4096u, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 4096u, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 4096u, ssh);

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandsData->setHeaps(dsh, ioh, ssh);

    std::vector<Surface *> surfaces;
    event->setCommand(std::make_unique<CommandComputeKernel>(*pCmdQ, blockedCommandsData, surfaces, false, false, false, nullptr, pDevice->getPreemptionMode(), pKernel, 1, nullptr));
    event->submitCommand(false);

    EXPECT_EQ(numGrfRequired, csr->savedDispatchFlags.numGrfRequired);
}

class MockCommandQueueInitializeBcs : public MockCommandQueue {
  public:
    MockCommandQueueInitializeBcs() : MockCommandQueue(nullptr, nullptr, 0, false) {}
    MockCommandQueueInitializeBcs(Context &context) : MockCommandQueueInitializeBcs(&context, context.getDevice(0), nullptr, false) {}
    MockCommandQueueInitializeBcs(Context *context, ClDevice *device, const cl_queue_properties *props, bool internalUsage)
        : MockCommandQueue(context, device, props, internalUsage) {
    }
    void initializeBcsEngine(bool internalUsage) override {
        if (initializeBcsEngineCalledTimes == 0) {
            auto th = std::thread([&]() {
                isCsrLocked = reinterpret_cast<MockCommandStreamReceiver *>(&this->getGpgpuCommandStreamReceiver())->isOwnershipMutexLocked();
            });
            th.join();
        }
        initializeBcsEngineCalledTimes++;
        MockCommandQueue::initializeBcsEngine(internalUsage);
    }
    int initializeBcsEngineCalledTimes = 0;
    bool isCsrLocked = false;
};

using CommandStreamReceiverFlushTaskTestsWithMockCommandStreamReceiver = UltCommandStreamReceiverTestWithCsr<MockCommandStreamReceiver>;

HWTEST_F(CommandStreamReceiverFlushTaskTestsWithMockCommandStreamReceiver, GivenBlockedKernelWhenInitializeBcsCalledThenCrsIsNotLocked) {
    MockContext mockContext;
    uint32_t numGrfRequired = 666u;

    auto pCmdQ = std::make_unique<MockCommandQueueInitializeBcs>(&mockContext, pClDevice, nullptr, false);
    auto mockProgram = std::make_unique<MockProgram>(&mockContext, false, toClDeviceVector(*pClDevice));

    auto pKernel = MockKernel::create(*pDevice, mockProgram.get(), numGrfRequired);
    auto kernelInfos = MockKernel::toKernelInfoContainer(pKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(pKernel), kernelInfos);
    auto event = std::make_unique<MockEvent<Event>>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::commandBuffer, pDevice->getDeviceBitfield()}));

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());

    std::vector<Surface *> surfaces;
    event->setCommand(std::make_unique<CommandComputeKernel>(*pCmdQ, blockedCommandsData, surfaces, false, false, false, nullptr, pDevice->getPreemptionMode(), pKernel, 1, nullptr));
    event->submitCommand(false);
    EXPECT_FALSE(pCmdQ->isCsrLocked);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDcFlushArgumentIsTrueWhenCallingAddPipeControlThenDcFlushIsEnabled) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);
    LinearStream commandStream(buffer.get(), 128);

    PipeControlArgs args;
    args.dcFlushEnable = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(commandStream, args);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDcFlushArgumentIsFalseWhenCallingAddPipeControlThenDcFlushIsEnabledOnlyOnGen8) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);
    LinearStream commandStream(buffer.get(), 128);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(commandStream, args);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_FALSE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenPerDssBackBufferIsAllocatedThenItIsClearedInCleanupResources) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_NE(nullptr, pDevice);
    commandStreamReceiver.createPerDssBackedBuffer(*pDevice);
    EXPECT_NE(nullptr, commandStreamReceiver.perDssBackedBuffer);
    commandStreamReceiver.cleanupResources();
    EXPECT_EQ(nullptr, commandStreamReceiver.perDssBackedBuffer);
}

HWTEST2_F(CommandStreamReceiverFlushTaskTests, whenPerDssBackBufferProgrammingEnabledThenAllocationIsCreated, IsAtMostXe3Core) {

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);

    EXPECT_EQ(1u, commandStreamReceiver.createPerDssBackedBufferCalled);
    EXPECT_NE(nullptr, commandStreamReceiver.perDssBackedBuffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenPerDssBackBufferProgrammingEnabledAndPerDssBackedBufferAlreadyPresentThenNewAllocationIsNotCreated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = pDevice->getMemoryManager();
    commandStreamReceiver.perDssBackedBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);

    EXPECT_EQ(0u, commandStreamReceiver.createPerDssBackedBufferCalled);
}

template <typename GfxFamily>
class MockCsrWithFailingFlush : public CommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiverHw<GfxFamily>::latestSentTaskCount;
    using CommandStreamReceiverHw<GfxFamily>::submissionAggregator;

    MockCsrWithFailingFlush(ExecutionEnvironment &executionEnvironment,
                            uint32_t rootDeviceIndex,
                            const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield) {
        this->dispatchMode = DispatchMode::batchedDispatch;
        this->tagAddress = &tag;
    }
    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return SubmissionStatus::failed;
    }
    TagAddressType tag = 0;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenWaitForCompletionWithTimeoutIsCalledWhenFlushBatchedSubmissionsReturnsFailureThenItIsPropagated) {
    MockCsrWithFailingFlush<FamilyType> mockCsr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(8));
    mockCsr.setupContext(osContext);
    mockCsr.latestSentTaskCount = 1;
    auto cmdBuffer = std::make_unique<CommandBuffer>(*pDevice);
    mockCsr.submissionAggregator->recordCommandBuffer(cmdBuffer.release());
    EXPECT_EQ(NEO::WaitStatus::notReady, mockCsr.waitForCompletionWithTimeout(WaitParams{false, false, false, 0}, 1));
}
