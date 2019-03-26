/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> IOQ;

TEST_F(IOQ, enqueueKernel_increasesTaskLevel) {
    auto previousTaskLevel = pCmdQ->taskLevel;

    EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pKernel);
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, enqueueFillBuffer_increasesTaskLevel) {
    auto previousTaskLevel = pCmdQ->taskLevel;

    EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, enqueueReadBuffer_increasesTaskLevel) {
    auto previousTaskLevel = pCmdQ->taskLevel;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    buffer->forceDisallowCPUCopy = true; // no task level incrasing when cpu copy
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, buffer.get());

    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, enqueueKernel_changesTaskCount) {
    auto &commandStreamReceiver = pCmdQ->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueKernelHelper<>::enqueueKernel(pCmdQ,
                                         pKernel);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, enqueueFillBuffer_changesTaskCount) {
    auto &commandStreamReceiver = pCmdQ->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, enqueueReadBuffer_changesTaskCount) {
    auto &commandStreamReceiver = pCmdQ->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    buffer->forceDisallowCPUCopy = true; // no task count incrasing when cpu copy
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, buffer.get());
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, enqueueReadBuffer_blockingAndNonBlockedOnUserEvent) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    buffer->forceDisallowCPUCopy = true; // no task level incrasing when cpu copy
    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          0,
                                                          BufferDefaults::sizeInBytes,
                                                          alignedReadPtr,
                                                          1,
                                                          &userEvent,
                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_LT(previousTaskCount, pCmdQ->taskCount);
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    alignedFree(alignedReadPtr);
}
