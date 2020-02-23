/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> IOQ;

TEST_F(IOQ, WhenEnqueueingKernelThenTaskLevelIsIncremented) {
    auto previousTaskLevel = pCmdQ->taskLevel;

    EnqueueKernelHelper<>::enqueueKernel(
        pCmdQ,
        pKernel);
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, WhenFillingBufferThenTaskLevelIsIncremented) {
    auto previousTaskLevel = pCmdQ->taskLevel;

    EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, WhenReadingBufferThenTaskLevelIsIncremented) {
    auto previousTaskLevel = pCmdQ->taskLevel;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    buffer->forceDisallowCPUCopy = true; // task level is not increased if doing cpu copy
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, buffer.get());

    EXPECT_LT(previousTaskLevel, pCmdQ->taskLevel);
}

TEST_F(IOQ, WhenEnqueueingKernelThenTaskCountIsIncremented) {
    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueKernelHelper<>::enqueueKernel(pCmdQ,
                                         pKernel);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, WhenFillingBufferThenTaskCountIsIncremented) {
    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();

    EnqueueFillBufferHelper<>::enqueue(pCmdQ);
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, WhenReadingBufferThenTaskCountIsIncremented) {
    auto &commandStreamReceiver = pCmdQ->getGpgpuCommandStreamReceiver();
    auto previousTaskCount = commandStreamReceiver.peekTaskCount();
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    buffer->forceDisallowCPUCopy = true; // task level is not increased if doing cpu copy
    EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ, buffer.get());
    EXPECT_LT(previousTaskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_LE(pCmdQ->taskCount, commandStreamReceiver.peekTaskCount());
}

TEST_F(IOQ, GivenUserEventWhenReadingBufferThenTaskCountAndTaskLevelAreIncremented) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    buffer->forceDisallowCPUCopy = true; // task level is not increased if doing cpu copy
    retVal = EnqueueReadBufferHelper<>::enqueueReadBuffer(pCmdQ,
                                                          buffer.get(),
                                                          CL_TRUE,
                                                          0,
                                                          BufferDefaults::sizeInBytes,
                                                          alignedReadPtr,
                                                          nullptr,
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
