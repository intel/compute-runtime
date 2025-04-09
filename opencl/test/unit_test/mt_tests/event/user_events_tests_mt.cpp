/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/event/async_events_handler.h"
#include "opencl/test/unit_test/event/event_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/scenario_test_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include <memory>

typedef HelloWorldTest<HelloWorldFixtureFactory> EventTests;

TEST_F(MockEventTests, GivenEventCreatedFromUserEventsThatIsNotSignaledThenDoNotFlushToCsr) {
    uEvent = makeReleaseable<UserEvent>();
    cl_event retEvent = nullptr;
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    // call NDR
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    *csr.getTagAddress() = (unsigned int)-1;
    auto taskLevelBeforeWaitForEvents = csr.peekTaskLevel();

    int counter = 0;
    int deadline = 20000;

    std::atomic<bool> threadStarted(false);
    std::atomic<bool> waitForEventsCompleted(false);

    std::thread t([&]() {
        threadStarted = true;
        // call WaitForEvents
        clWaitForEvents(1, &retEvent);
        waitForEventsCompleted = true;
    });
    // wait for the thread to start
    while (!threadStarted)
        ;
    // now wait a while.
    while (!waitForEventsCompleted && counter++ < deadline)
        ;

    ASSERT_EQ(waitForEventsCompleted, false) << "WaitForEvents returned while user event is not signaled!";

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(taskLevelBeforeWaitForEvents, csr.peekTaskLevel());

    // set event to CL_COMPLETE
    uEvent->setStatus(CL_COMPLETE);
    t.join();

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, givenUserEventBlockingEnqueueWithBlockingFlagWhenUserEventIsCompletedAfterBlockedPathIsChosenThenBlockingFlagDoesNotCauseStall) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create());
    std::unique_ptr<char[]> dst(new char[srcBuffer->getSize()]);

    for (int32_t i = 0; i < 4; i++) {
        UserEvent uEvent;
        cl_event eventWaitList[] = {&uEvent};
        int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

        std::thread t([&]() {
            uEvent.setStatus(CL_COMPLETE);
        });

        auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(), CL_TRUE, 0, srcBuffer->getSize(), dst.get(), nullptr, sizeOfWaitList, eventWaitList, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        t.join();
    }
}

TEST_F(EventTests, givenUserEventBlockingEnqueueWithBlockingFlagWhenUserEventIsCompletedAfterUpdateFromCompletionStampThenBlockingFlagDoesNotCauseStall) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create());
    std::unique_ptr<char[]> dst(new char[srcBuffer->getSize()]);

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    std::thread t([&]() {
        while (true) {
            pCmdQ->takeOwnership();

            if (pCmdQ->taskLevel == CompletionStamp::notReady) {
                pCmdQ->releaseOwnership();
                break;
            }
            pCmdQ->releaseOwnership();
        }
        uEvent.setStatus(CL_COMPLETE);
    });

    auto retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(), CL_TRUE, 0, srcBuffer->getSize(), dst.get(), nullptr, sizeOfWaitList, eventWaitList, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    t.join();
}

HWTEST_F(EventTests, givenOneThreadUpdatingUserEventAnotherWaitingOnFinishWhenFinishIsCalledThenItWaitsForCorrectTaskCount) {
    USE_REAL_FILE_SYSTEM();
    MockCommandQueueHw<FamilyType> mockCmdQueue(context, pClDevice, nullptr);
    std::unique_ptr<Buffer> srcBuffer(BufferHelper<>::create());
    std::unique_ptr<char[]> dst(new char[srcBuffer->getSize()]);
    for (uint32_t i = 0; i < 4; i++) {

        UserEvent uEvent;
        cl_event eventWaitList[] = {&uEvent};
        int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
        cl_event returnedEvent = nullptr;

        std::atomic_bool go{false};
        std::atomic_bool updateEvent{true};

        std::thread t([&]() {
            while (!go)
                ;

            uEvent.setStatus(CL_COMPLETE);
        });

        auto retVal = mockCmdQueue.enqueueReadBuffer(srcBuffer.get(), CL_FALSE, 0, srcBuffer->getSize(), dst.get(), nullptr, sizeOfWaitList, eventWaitList, &returnedEvent);
        EXPECT_EQ(CL_SUCCESS, retVal);

        std::thread t2([&]() {
            while (updateEvent) {
                castToObject<Event>(returnedEvent)->updateExecutionStatus();
            }
        });

        go = true;

        clFinish(&mockCmdQueue);
        EXPECT_EQ(mockCmdQueue.latestTaskCountWaited, i + 1);

        t.join();
        updateEvent = false;
        t2.join();
        clReleaseEvent(returnedEvent);
    }
}

void CL_CALLBACK callback(cl_event event, cl_int status, void *data) {
    CallbackData *callbackData = (CallbackData *)data;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    clEnqueueNDRangeKernel(callbackData->queue, callbackData->kernel, 1, offset, gws, nullptr, 0, nullptr, nullptr);
    clFinish(callbackData->queue);

    callbackData->callbackCalled = true;

    if (callbackData->signalCallbackDoneEvent) {
        cl_event callbackEvent = callbackData->signalCallbackDoneEvent;
        clSetUserEventStatus(callbackEvent, CL_COMPLETE);
        // No need to reatin and release this synchronization event
        // clReleaseEvent(callbackEvent);
    }
}

TEST_F(ScenarioTest, givenAsyncHandlerEnabledAndUserEventBlockingEnqueueAndOutputEventWithCallbackWhenUserEventIsSetCompleteThenCallbackIsExecuted) {
    debugManager.flags.EnableAsyncEventsHandler.set(true);

    cl_command_queue clCommandQ = nullptr;
    cl_queue_properties properties = 0;
    cl_kernel clKernel = kernelInternals->mockMultiDeviceKernel;
    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};
    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;
    UserEvent *userEvent = new UserEvent(context);
    cl_event eventBlocking = userEvent;
    cl_event eventOut = nullptr;

    clCommandQ = clCreateCommandQueue(context, devices[0], properties, &retVal);
    retVal = clEnqueueNDRangeKernel(clCommandQ, clKernel, 1, offset, gws, nullptr, 1, &eventBlocking, &eventOut);

    EXPECT_EQ(success, retVal);
    ASSERT_NE(nullptr, eventOut);

    CallbackData data;
    data.kernel = clKernel;
    data.queue = clCommandQ;
    data.callbackCalled = false;
    data.signalCallbackDoneEvent = new UserEvent(context);
    cl_event callbackEvent = data.signalCallbackDoneEvent;

    clSetEventCallback(eventOut, CL_COMPLETE, callback, &data);
    EXPECT_FALSE(data.callbackCalled);

    clSetUserEventStatus(eventBlocking, CL_COMPLETE);
    userEvent->release();

    clWaitForEvents(1, &eventOut);
    clWaitForEvents(1, &callbackEvent);

    EXPECT_TRUE(data.callbackCalled);

    data.signalCallbackDoneEvent->release();

    clReleaseEvent(eventOut);
    clReleaseCommandQueue(clCommandQ);

    context->getAsyncEventsHandler().closeThread();
}
