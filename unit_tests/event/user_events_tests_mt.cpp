/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "event_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "runtime/memory_manager/memory_manager.h"

#include <memory>

typedef HelloWorldTest<HelloWorldFixtureFactory> EventTests;

TEST_F(EventTests, eventCreatedFromUserEventsThatIsNotSignaledDoesntFlushToCSR) {
    UserEvent uEvent;
    cl_event retEvent = nullptr;
    cl_event eventWaitList[] = {&uEvent};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);

    auto &csr = pCmdQ->getCommandStreamReceiver();
    *csr.getTagAddress() = (unsigned int)-1;
    auto taskLevelBeforeWaitForEvents = csr.peekTaskLevel();

    int counter = 0;
    int Deadline = 20000;

    std::atomic<bool> ThreadStarted(false);
    std::atomic<bool> WaitForEventsCompleted(false);

    std::thread t([&]() {
        ThreadStarted = true;
        //call WaitForEvents
        clWaitForEvents(1, &retEvent);
        WaitForEventsCompleted = true;
    });
    //wait for the thread to start
    while (!ThreadStarted)
        ;
    //now wait a while.
    while (!WaitForEventsCompleted && counter++ < Deadline)
        ;

    ASSERT_EQ(WaitForEventsCompleted, false) << "WaitForEvents returned while user event is not signaled!";

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(taskLevelBeforeWaitForEvents, csr.peekTaskLevel());

    //set event to CL_COMPLETE
    uEvent.setStatus(CL_COMPLETE);
    t.join();

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
    pCmdQ->releaseVirtualEvent();
}

TEST_F(EventTests, givenUserEventBlockingEnqueueWithBlockingFlagWhenUserEventIsCompletedAfterBlockedPathIsChosenThenBlockingFlagDoesNotCauseStall) {

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create());
    std::unique_ptr<char[]> dst(new char[srcBuffer->getSize()]);

    for (int32_t i = 0; i < 20; i++) {
        UserEvent uEvent;
        cl_event eventWaitList[] = {&uEvent};
        int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

        std::thread t([&]() {
            uEvent.setStatus(CL_COMPLETE);
        });

        auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(), CL_TRUE, 0, srcBuffer->getSize(), dst.get(), sizeOfWaitList, eventWaitList, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        t.join();
    }
    pCmdQ->releaseVirtualEvent();
}

TEST_F(EventTests, givenUserEventBlockingEnqueueWithBlockingFlagWhenUserEventIsCompletedAfterUpdateFromCompletionStampThenBlockingFlagDoesNotCauseStall) {

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create());
    std::unique_ptr<char[]> dst(new char[srcBuffer->getSize()]);

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    std::thread t([&]() {
        while (true) {
            pCmdQ->takeOwnership(true);

            if (pCmdQ->taskLevel == Event::eventNotReady) {
                pCmdQ->releaseOwnership();
                break;
            }
            pCmdQ->releaseOwnership();
        }
        uEvent.setStatus(CL_COMPLETE);
    });

    auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(), CL_TRUE, 0, srcBuffer->getSize(), dst.get(), sizeOfWaitList, eventWaitList, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    t.join();
    pCmdQ->releaseVirtualEvent();
}
