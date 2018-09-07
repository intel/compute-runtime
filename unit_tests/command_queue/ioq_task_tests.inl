/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

using namespace OCLRT;

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
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueKernelHelper<>::enqueueKernel(pCmdQ,
                                         pKernel);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, enqueueFillBuffer_changesTaskCount) {
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, enqueueReadBuffer_changesTaskCount) {
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
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
