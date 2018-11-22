/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/mocks/mock_csr.h"

using namespace OCLRT;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

template <typename TypeParam>
struct OOQTaskTypedTests : public HelloWorldTest<OOQFixtureFactory> {
};

TYPED_TEST_CASE_P(OOQTaskTypedTests);

bool isBlockingCall(unsigned int cmdType) {
    if (cmdType == CL_COMMAND_WRITE_BUFFER ||
        cmdType == CL_COMMAND_READ_BUFFER ||
        cmdType == CL_COMMAND_WRITE_IMAGE ||
        cmdType == CL_COMMAND_READ_IMAGE) {
        return true;
    } else {
        return false;
    }
}

TYPED_TEST_P(OOQTaskTypedTests, givenNonBlockingCallWhenDoneOnOutOfOrderQueueThenTaskLevelDoesntChange) {
    auto &commandStreamReceiver = this->pCmdQ->getCommandStreamReceiver();
    auto tagAddress = commandStreamReceiver.getTagAddress();

    auto blockingCall = isBlockingCall(TypeParam::Traits::cmdType);
    auto taskLevelClosed = blockingCall ? 1u : 0u; // for blocking commands task level will be closed

    //for non blocking calls make sure that resources are added to defer free list instaed of being destructed in place
    if (!blockingCall) {
        *tagAddress = 0;
    }

    auto previousTaskLevel = this->pCmdQ->taskLevel;

    if (TypeParam::Traits::cmdType == CL_COMMAND_WRITE_BUFFER || TypeParam::Traits::cmdType == CL_COMMAND_READ_BUFFER) {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
        buffer->forceDisallowCPUCopy = true; // no task level logic when cpu copy
        TypeParam::enqueue(this->pCmdQ, buffer.get());
        this->pCmdQ->flush();

    } else {
        TypeParam::enqueue(this->pCmdQ, nullptr);
    }
    EXPECT_EQ(previousTaskLevel + taskLevelClosed, this->pCmdQ->taskLevel);
    *tagAddress = initialHardwareTag;
}

TYPED_TEST_P(OOQTaskTypedTests, givenTaskWhenEnqueuedOnOutOfOrderQueueThenTaskCountIsUpdated) {
    auto &commandStreamReceiver = this->pCmdQ->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();
    auto tagAddress = commandStreamReceiver.getTagAddress();
    auto blockingCall = isBlockingCall(TypeParam::Traits::cmdType);

    //for non blocking calls make sure that resources are added to defer free list instaed of being destructed in place
    if (!blockingCall) {
        *tagAddress = 0;
    }

    if (TypeParam::Traits::cmdType == CL_COMMAND_WRITE_BUFFER || TypeParam::Traits::cmdType == CL_COMMAND_READ_BUFFER) {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
        buffer->forceDisallowCPUCopy = true; // no task level logic when cpu copy
        TypeParam::enqueue(this->pCmdQ, buffer.get());
        this->pCmdQ->flush();
    } else {
        TypeParam::enqueue(this->pCmdQ, nullptr);
    }
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(this->pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
    *tagAddress = initialHardwareTag;
}

typedef ::testing::Types<
    EnqueueCopyBufferHelper<>,
    EnqueueCopyImageHelper<>,
    EnqueueFillBufferHelper<>,
    EnqueueFillImageHelper<>,
    EnqueueReadBufferHelper<>,
    EnqueueReadImageHelper<>,
    EnqueueWriteBufferHelper<>,
    EnqueueWriteImageHelper<>>
    EnqueueParams;

REGISTER_TYPED_TEST_CASE_P(OOQTaskTypedTests,
                           givenNonBlockingCallWhenDoneOnOutOfOrderQueueThenTaskLevelDoesntChange,
                           givenTaskWhenEnqueuedOnOutOfOrderQueueThenTaskCountIsUpdated);

// Instantiate all of these parameterized tests
INSTANTIATE_TYPED_TEST_CASE_P(OOQ, OOQTaskTypedTests, EnqueueParams);

typedef OOQTaskTypedTests<EnqueueKernelHelper<>> OOQTaskTests;

TEST_F(OOQTaskTests, enqueueKernel_changesTaskCount) {
    auto &commandStreamReceiver = pCmdQ->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ,
                                         pKernel);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(this->pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(OOQTaskTests, givenCommandQueueWithLowerTaskLevelThenCsrWhenItIsSubmittedThenCommandQueueObtainsTaskLevelFromCsrWithoutSendingPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskLevel = 100;
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);
}

HWTEST_F(OOQTaskTests, givenCommandQueueAtTaskLevel100WhenMultipleEnqueueAreDoneThenTaskLevelDoesntChnage) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->taskLevel = 100;

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);
    EXPECT_EQ(100u, mockCsr->peekTaskLevel());
}

HWTEST_F(OOQTaskTests, givenCommandQueueAtTaskLevel100WhenItIsFlushedAndFollowedByNewCommandsThenTheyHaveHigherTaskLevel) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->taskLevel = 100;

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);
    EXPECT_EQ(100u, mockCsr->peekTaskLevel());

    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(101u, mockCsr->peekTaskLevel());
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(101u, this->pCmdQ->taskLevel);
}

HWTEST_F(OOQTaskTests, givenCommandQueueAtTaskLevel100WhenItIsFlushedAndFollowedByNewCommandsAndBarrierThenCsrTaskLevelIncreases) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->taskLevel = 100;

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(101u, mockCsr->peekTaskLevel());
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(101u, this->pCmdQ->taskLevel);
    this->pCmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(102u, this->pCmdQ->taskLevel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(102u, this->pCmdQ->taskLevel);
    EXPECT_EQ(102u, mockCsr->peekTaskLevel());
}

HWTEST_F(OOQTaskTests, givenCommandQueueAtTaskLevel100WhenItIsFlushedAndFollowedByNewCommandsAndMarkerThenCsrTaskLevelIsNotIncreasing) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr->taskLevel = 100;

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(101u, mockCsr->peekTaskLevel());
    EXPECT_EQ(100u, this->pCmdQ->taskLevel);

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(101u, this->pCmdQ->taskLevel);
    this->pCmdQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(101u, this->pCmdQ->taskLevel);
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel);
    EXPECT_EQ(101u, this->pCmdQ->taskLevel);
    EXPECT_EQ(101u, mockCsr->peekTaskLevel());
}

HWTEST_F(OOQTaskTests, givenTwoEnqueueCommandSynchronizedByEventsWhenTheyAreEnqueueThenSecondHasHigherTaskLevelThenFirst) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    auto currentTaskLevel = this->pCmdQ->taskLevel;
    cl_event retEvent;
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel, EnqueueKernelTraits::workDim,
                                         EnqueueKernelTraits::globalWorkOffset,
                                         EnqueueKernelTraits::globalWorkSize,
                                         EnqueueKernelTraits::localWorkSize,
                                         0, nullptr, &retEvent);

    auto neoEvent = castToObject<Event>(retEvent);
    EXPECT_EQ(currentTaskLevel, neoEvent->taskLevel);
    cl_event retEvent2;
    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ, pKernel, EnqueueKernelTraits::workDim,
                                         EnqueueKernelTraits::globalWorkOffset,
                                         EnqueueKernelTraits::globalWorkSize,
                                         EnqueueKernelTraits::localWorkSize,
                                         1, &retEvent, &retEvent2);
    auto neoEvent2 = castToObject<Event>(retEvent2);
    EXPECT_EQ(neoEvent2->taskLevel, neoEvent->taskLevel + 1);
    clReleaseEvent(retEvent2);
    clReleaseEvent(retEvent);
}

TEST_F(OOQTaskTests, enqueueKernel_doesntChangeTaskLevel) {
    auto previousTaskLevel = this->pCmdQ->taskLevel;

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ,
                                         pKernel);
    EXPECT_EQ(previousTaskLevel, this->pCmdQ->taskLevel);
}

TEST_F(OOQTaskTests, enqueueReadBuffer_blockingAndNonBlockedOnUserEvent) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    pCmdQ->taskLevel = 1;

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    buffer->forceDisallowCPUCopy = true; // no task level incrasing when cpu copy
    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_FALSE,
                                                          0,
                                                          BufferDefaults::sizeInBytes,
                                                          alignedReadPtr,
                                                          1,
                                                          &userEvent,
                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(previousTaskCount, pCmdQ->taskCount);
    EXPECT_EQ(previousTaskLevel, pCmdQ->taskLevel);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();
    alignedFree(alignedReadPtr);
}

TEST_F(OOQTaskTests, givenOutOfOrderCommandQueueWhenBarrierIsCalledThenTaskLevelIsUpdated) {

    EnqueueKernelHelper<>::enqueueKernel(this->pCmdQ,
                                         pKernel);
    auto currentTaskLevel = this->pCmdQ->taskLevel;

    clEnqueueBarrierWithWaitList(this->pCmdQ, 0, nullptr, nullptr);
    auto newTaskLevel = this->pCmdQ->taskLevel;

    EXPECT_GT(newTaskLevel, currentTaskLevel);
}
