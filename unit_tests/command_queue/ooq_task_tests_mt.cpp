/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

using namespace NEO;

struct OOQFixtureFactory : public HelloWorldFixtureFactory {
    typedef OOQueueFixture CommandQueueFixture;
};

template <typename TypeParam>
struct OOQTaskTypedTestsMt : public HelloWorldTest<OOQFixtureFactory> {
};

typedef OOQTaskTypedTestsMt<EnqueueKernelHelper<>> OOQTaskTestsMt;

TEST_F(OOQTaskTestsMt, enqueueReadBuffer_blockingAndBlockedOnUserEvent) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto alignedReadPtr = alignedMalloc(BufferDefaults::sizeInBytes, MemoryConstants::cacheLineSize);
    ASSERT_NE(nullptr, alignedReadPtr);

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto previousTaskCount = pCmdQ->taskCount;
    auto previousTaskLevel = pCmdQ->taskLevel;

    std::thread t([=]() {
        Event *ev = castToObject<Event>(userEvent);
        while (ev->peekHasChildEvents() == false) {
            // active wait for VirtualEvent (which is added after queue is blocked)
        }
        auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
        ASSERT_EQ(CL_SUCCESS, ret);
    });

    buffer->forceDisallowCPUCopy = true; // no task level incrasing when cpu copy
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
    EXPECT_EQ(previousTaskLevel, pCmdQ->taskLevel);

    t.join();

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    alignedFree(alignedReadPtr);
}

TEST_F(OOQTaskTestsMt, enqueueMarker_blockedOnUserEvent) {

    auto userEvent = clCreateUserEvent(pContext, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::thread t([=]() {
        Event *ev = castToObject<Event>(userEvent);
        while (ev->peekHasChildEvents() == false) {
            // active wait for VirtualEvent (which is added after queue is blocked)
        }
        auto ret = clSetUserEventStatus(userEvent, CL_COMPLETE);
        ASSERT_EQ(CL_SUCCESS, ret);
    });

    retVal = pCmdQ->enqueueMarkerWithWaitList(
        1,
        &userEvent,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    t.join();

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}