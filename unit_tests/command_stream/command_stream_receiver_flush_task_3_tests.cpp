/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

using namespace OCLRT;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskTests;

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledThenNothingIsSubmittedToTheHwAndSubmissionIsRecorded) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    auto cmdBuffer = cmdBufferList.peekHead();
    //two more because of preemption allocation and sipKernel in Mid Thread preemption mode
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    //we should have 3 heaps, tag allocation and csr command stream + cq
    EXPECT_EQ(5u + csrSurfaceCount, cmdBuffer->surfaces.size());

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    //we should be submitting via csr
    EXPECT_EQ(cmdBuffer->batchBuffer.commandBufferAllocation, mockCsr->commandStream.getGraphicsAllocation());
    EXPECT_EQ(cmdBuffer->batchBuffer.startOffset, 0u);
    EXPECT_FALSE(cmdBuffer->batchBuffer.requiresCoherency);
    EXPECT_FALSE(cmdBuffer->batchBuffer.low_priority);

    //find BB END
    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(bbEndAddress, cmdBuffer->batchBufferEndLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndTwoRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto secondBatchBuffer = primaryBatch->next;

    auto bbEndLocation = primaryBatch->batchBufferEndLocation;
    auto secondBatchBufferAddress = (uint64_t)ptrOffset(secondBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                        secondBatchBuffer->batchBuffer.startOffset);

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(secondBatchBufferAddress, batchBufferStart->getBatchBufferStartAddressGraphicsaddress472());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndThreeRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto lastBatchBuffer = primaryBatch->next->next;

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;
    auto lastBatchBufferAddress = (uint64_t)ptrOffset(lastBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                      lastBatchBuffer->batchBuffer.startOffset);

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(lastBatchBufferAddress, batchBufferStart->getBatchBufferStartAddressGraphicsaddress472());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndThreeRecordedCommandBuffersThatUsesAllResourceWhenFlushTaskIsCalledThenBatchBuffersAreNotCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto memorySize = (size_t)pDevice->getDeviceInfo().globalMemSize;
    MockGraphicsAllocation largeAllocation(nullptr, memorySize);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
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

    EXPECT_EQ(3, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledTwiceThenNothingIsSubmittedToTheHwAndTwoSubmissionAreRecorded) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto initialBase = commandStream.getCpuBase();
    auto initialUsed = commandStream.getUsed();

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    //ensure command stream still used
    EXPECT_EQ(initialBase, commandStream.getCpuBase());
    auto baseAfterFirstFlushTask = commandStream.getCpuBase();
    auto usedAfterFirstFlushTask = commandStream.getUsed();

    dispatchFlags.requiresCoherency = true;
    dispatchFlags.lowPriority = true;

    mockCsr->flushTask(commandStream,
                       commandStream.getUsed(),
                       dsh,
                       ioh,
                       ssh,
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

    EXPECT_FALSE(cmdBuffer1->batchBuffer.requiresCoherency);
    EXPECT_TRUE(cmdBuffer2->batchBuffer.requiresCoherency);

    EXPECT_FALSE(cmdBuffer1->batchBuffer.low_priority);
    EXPECT_TRUE(cmdBuffer2->batchBuffer.low_priority);

    EXPECT_GT(cmdBuffer2->batchBuffer.startOffset, cmdBuffer1->batchBuffer.startOffset);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();
    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.requiresCoherency = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    mockCsr->lastSentCoherencyRequest = 1;

    commandStream.getSpace(4);

    mockCsr->flushTask(commandStream,
                       4,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());
    auto cmdBuffer = cmdBufferList.peekHead();

    //preemption allocation + sip kernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    //copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(0u));
        EXPECT_EQ(1, graphicsAllocation->getResidencyTaskCount(0u));
    }

    mockCsr->flushBatchedSubmissions();

    EXPECT_FALSE(mockCsr->recordedCommandBuffer->batchBuffer.low_priority);
    EXPECT_TRUE(mockCsr->recordedCommandBuffer->batchBuffer.requiresCoherency);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.commandBufferAllocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(4u, mockCsr->recordedCommandBuffer->batchBuffer.startOffset);
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());

    EXPECT_EQ(0u, surfacesForResidency.size());

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_FALSE(graphicsAllocation->isResident(0u));
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrCreatedWithDedicatedDebugFlagWhenItIsCreatedThenItHasProperDispatchMode) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::AdaptiveDispatch));
    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment));
    EXPECT_EQ(DispatchMode::AdaptiveDispatch, mockCsr->dispatchMode);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenBlockingCommandIsSendThenItIsFlushedAndNotBatched) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenBufferToFlushWhenFlushTaskCalledThenUpdateFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    commandStream.getSpace(1);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_GT(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), cmplStamp.flushStamp);
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>();

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledThenFlushedTaskCountIsNotModifed) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInDefaultModeWhenFlushTaskIsCalledThenFlushedTaskCountIsModifed) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    auto &csr = pDevice->getCommandStreamReceiver();

    csr.flushTask(commandStream,
                  0,
                  dsh,
                  ioh,
                  ssh,
                  taskLevel,
                  dispatchFlags,
                  *pDevice);

    EXPECT_EQ(1u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(1u, csr.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenWaitForTaskCountIsCalledWithTaskCountThatWasNotYetFlushedThenBatchedCommandBuffersAreSubmitted) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    auto cmdBuffer = cmdBufferList.peekHead();
    EXPECT_EQ(1u, cmdBuffer->taskCount);

    mockCsr->waitForCompletionWithTimeout(false, 1, 1);

    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());

    EXPECT_TRUE(cmdBufferList.peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenEnqueueIsMadeThenCurrentMemoryUsedIsTracked) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    EXPECT_EQ(expectedUsed, mockCsr->peekTotalMemoryUsed());

    //after flush it goes to 0
    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenSusbsequentEnqueueIsMadeThenOnlyNewResourcesAreTrackedForMemoryUsage) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
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
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(expectedUsed + additionalSize, mockCsr->peekTotalMemoryUsed());
    mockCsr->flushBatchedSubmissions();
}

struct MockedMemoryManager : public OsAgnosticMemoryManager {
    MockedMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, false, executionEnvironment) {}
    bool isMemoryBudgetExhausted() const override { return budgetExhausted; }
    bool budgetExhausted = false;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTotalResourceUsedExhaustsTheBudgetThenDoImplicitFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);
    ExecutionEnvironment executionEnvironment;
    auto mockedMemoryManager = new MockedMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockedMemoryManager);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(mockCsr));
    mockCsr->initializeTagAllocation();
    mockCsr->setPreemptionCsrAllocation(pDevice->getPreemptionAllocation());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockedMemoryManager->budgetExhausted = true;

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
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
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    //expect 2 flushes, since we cannot batch those submissions
    EXPECT_EQ(2u, mockCsr->peekLatestFlushedTaskCount());
    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeWhenTwoTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(firstCmdBuffer->pipeControlThatMayBeErasedLocation, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc);
    auto ppc2 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc2);

    //flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    //make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
    EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);

    //first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    EXPECT_NE(nullptr, noop1);
    EXPECT_EQ(nullptr, noop2);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc2);
    EXPECT_NE(nullptr, ppcAfterChange);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenDcFlushIsNotRequiredThenItIsNotSet) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWithOutOfOrderModeFisabledWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenDcFlushIsRequiredThenPipeControlIsNotRegistredForNooping) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.dcFlush = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTwoTasksArePassedWithTheSameLevelThenThereIsPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->timestampPacketWriteEnabled = false;

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTimestampPacketWriteIsEnabledThenNoopPipeControl) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->timestampPacketWriteEnabled = false;
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenPipeControlForNoopAddressIsNullThenPipeControlIsNotNooped) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto ppc1Location = firstCmdBuffer->pipeControlThatMayBeErasedLocation;

    firstCmdBuffer->pipeControlThatMayBeErasedLocation = nullptr;

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc1Location);
    EXPECT_NE(nullptr, ppc);

    //call flush, both pipe controls must remain untouched
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers
    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeWhenThreeTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
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

    //flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());
    auto itorBatchBufferStartThird = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartSecond, cmdList.end());

    //make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartThird);
    EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorBatchBufferStartThird, itorBatchBufferStartSecond);

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);

    itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartSecond, itorBatchBufferStartThird);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartThird);

    //first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    auto noop3 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc3);

    EXPECT_NE(nullptr, noop1);
    EXPECT_NE(nullptr, noop2);
    EXPECT_EQ(nullptr, noop3);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc3);
    EXPECT_NE(nullptr, ppcAfterChange);
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverCleanupTests;
HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenTemporaryAndReusableAllocationsArePresentThenCleanupResourcesOnlyCleansThoseAboveLatestFlushTaskLevel) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = pDevice->getMemoryManager();

    auto temporaryToClean = memoryManager->allocateGraphicsMemory(4096u);
    auto temporaryToHold = memoryManager->allocateGraphicsMemory(4096u);

    auto reusableToClean = memoryManager->allocateGraphicsMemory(4096u);
    auto reusableToHold = memoryManager->allocateGraphicsMemory(4096u);

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToClean), TEMPORARY_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToHold), TEMPORARY_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToClean), REUSABLE_ALLOCATION);
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToHold), REUSABLE_ALLOCATION);

    temporaryToClean->updateTaskCount(1, 0u);
    reusableToClean->updateTaskCount(1, 0u);

    temporaryToHold->updateTaskCount(10, 0u);
    reusableToHold->updateTaskCount(10, 0u);

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

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToLowWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::LOW;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::LOW);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToMediumWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::MEDIUM;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::MEDIUM);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToHighWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::HIGH;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::HIGH);
}
