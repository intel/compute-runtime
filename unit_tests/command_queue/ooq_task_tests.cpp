/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

TYPED_TEST_P(OOQTaskTypedTests, doesntChangeTaskLevel) {
    auto previousTaskLevel = this->pCmdQ->taskLevel;
    auto taskLevelClosed = 0u; // for blocking commands task level will be closed
    if (TypeParam::Traits::cmdType == CL_COMMAND_WRITE_BUFFER || TypeParam::Traits::cmdType == CL_COMMAND_READ_BUFFER) {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
        buffer->forceDisallowCPUCopy = true; // no task level logic when cpu copy
        TypeParam::enqueue(this->pCmdQ, buffer.get());
        taskLevelClosed = 1u;
    } else {
        TypeParam::enqueue(this->pCmdQ, nullptr);
    }
    if (TypeParam::Traits::cmdType == CL_COMMAND_WRITE_IMAGE || TypeParam::Traits::cmdType == CL_COMMAND_READ_IMAGE) {
        taskLevelClosed = 1u;
    }
    EXPECT_EQ(previousTaskLevel + taskLevelClosed, this->pCmdQ->taskLevel);
}

TYPED_TEST_P(OOQTaskTypedTests, changesTaskCount) {
    auto &commandStreamReceiver = this->pDevice->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    if (TypeParam::Traits::cmdType == CL_COMMAND_WRITE_BUFFER || TypeParam::Traits::cmdType == CL_COMMAND_READ_BUFFER) {
        auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
        buffer->forceDisallowCPUCopy = true; // no task level logic when cpu copy
        TypeParam::enqueue(this->pCmdQ, buffer.get());
    } else {
        TypeParam::enqueue(this->pCmdQ, nullptr);
    }
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(this->pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
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
                           doesntChangeTaskLevel,
                           changesTaskCount);

// Instantiate all of these parameterized tests
INSTANTIATE_TYPED_TEST_CASE_P(OOQ, OOQTaskTypedTests, EnqueueParams);

typedef OOQTaskTypedTests<EnqueueKernelHelper<>> OOQTaskTests;

TEST_F(OOQTaskTests, enqueueKernel_changesTaskCount) {
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0]);
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
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
