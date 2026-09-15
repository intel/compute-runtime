/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/test/common/fixtures/leo_event_callbacks_fixture.h"

namespace NEO {
namespace LEO {
namespace ult {

using AsyncEventsHandlerMtTests = Test<EventCallbacksFixture>;

struct MockAsyncEvent : public Event {
    using Event::Event;
    using Event::wait;

    ADDMETHOD_NOBASE(wait, ze_result_t, ZE_RESULT_NOT_READY, (uint64_t timeout));

    void complete() { this->eventStatus = CL_COMPLETE; }
};

TEST_F(AsyncEventsHandlerMtTests, givenRegisteredEventWhenItCompletesThenTheHandlerThreadExecutesTheCallback) {
    auto handler = installMockHandler(true);
    auto event = new MockAsyncEvent(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));
    ASSERT_TRUE(handler->openThreadCalled);
    ASSERT_NE(nullptr, handler->thread.get());

    event->complete();

    EXPECT_TRUE(tracker.waitForCount(1u));
    EXPECT_EQ(static_cast<cl_event>(event), tracker.lastEvent);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);

    handler->closeThread();
    EXPECT_FALSE(event->peekHasCallbacks());

    clReleaseEvent(event);
}

TEST_F(AsyncEventsHandlerMtTests, givenAlreadySignalledEventWhenCallbackIsRegisteredThenTheHandlerThreadIsNotNeeded) {
    auto handler = installMockHandler(true);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    signalEvent(event);
    auto &tracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_FALSE(handler->openThreadCalled);
    EXPECT_EQ(nullptr, handler->thread.get());

    clReleaseEvent(event);
}

TEST_F(AsyncEventsHandlerMtTests, givenMultipleEventsWithCallbacksWhenAllCompleteThenAllCallbacksAreExecuted) {
    auto handler = installMockHandler(true);
    constexpr uint32_t numEvents = 8u;

    MockAsyncEvent *events[numEvents]{};
    std::vector<CallbackTracker *> trackers;

    for (uint32_t i = 0; i < numEvents; i++) {
        events[i] = new MockAsyncEvent(CL_COMMAND_MARKER, commandQueue);
        trackers.push_back(&createTracker());
        ASSERT_EQ(CL_SUCCESS, clSetEventCallback(events[i], CL_COMPLETE, &CallbackTracker::callback, trackers[i]));
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        events[i]->complete();
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        EXPECT_TRUE(trackers[i]->waitForCount(1u)) << "callback " << i << " was not executed";
        EXPECT_EQ(CL_COMPLETE, trackers[i]->lastStatus);
    }

    handler->closeThread();

    for (uint32_t i = 0; i < numEvents; i++) {
        clReleaseEvent(events[i]);
    }
}

TEST_F(AsyncEventsHandlerMtTests, givenCallbacksOfDifferentTypesWhenEventCompletesThenTheHandlerThreadExecutesAllOfThem) {
    auto handler = installMockHandler(true);
    auto event = new MockAsyncEvent(CL_COMMAND_MARKER, commandQueue);
    auto &submittedTracker = createTracker();
    auto &runningTracker = createTracker();
    auto &completeTracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_SUBMITTED, &CallbackTracker::callback, &submittedTracker));
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_RUNNING, &CallbackTracker::callback, &runningTracker));
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &completeTracker));

    event->complete();

    EXPECT_TRUE(submittedTracker.waitForCount(1u));
    EXPECT_TRUE(runningTracker.waitForCount(1u));
    EXPECT_TRUE(completeTracker.waitForCount(1u));

    handler->closeThread();

    EXPECT_EQ(1u, submittedTracker.count);
    EXPECT_EQ(1u, runningTracker.count);
    EXPECT_EQ(1u, completeTracker.count);

    clReleaseEvent(event);
}

TEST_F(AsyncEventsHandlerMtTests, givenApiReleasedEventWithPendingCallbackWhenItCompletesThenTheHandlerRunsTheCallbackAndDestroysTheEvent) {
    auto handler = installMockHandler(true);
    auto event = new MockAsyncEvent(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));
    ASSERT_EQ(CL_SUCCESS, clReleaseEvent(event));

    event->complete();

    EXPECT_TRUE(tracker.waitForCount(1u));

    handler->closeThread();
}

TEST_F(AsyncEventsHandlerMtTests, givenNeverSignalledEventWhenClosingTheThreadThenItIsJoinedAndTheEventIsReleased) {
    auto handler = installMockHandler(true);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));
    ASSERT_NE(nullptr, handler->thread.get());

    handler->closeThread();

    EXPECT_EQ(nullptr, handler->thread.get());
    EXPECT_EQ(0u, tracker.count);
    EXPECT_TRUE(event->peekHasCallbacks());

    clReleaseEvent(event);

    // The event never completed, so its destructor is what finally drains the callback.
    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(Event::executionTerminatedOnDestruction, tracker.lastStatus);
}

struct MockEventWithHangingWait : public Event {
    using Event::Event;
    using Event::wait;

    ADDMETHOD_NOBASE(wait, ze_result_t, ZE_RESULT_ERROR_DEVICE_LOST, (uint64_t timeout));
};

TEST_F(AsyncEventsHandlerMtTests, givenHangingEventWhenTrackedByTheHandlerThreadThenCallbackIsExecutedWithTheAbortStatus) {
    auto handler = installMockHandler(true);
    auto event = new MockEventWithHangingWait(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    EXPECT_TRUE(tracker.waitForCount(1u));
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, tracker.lastStatus);
    EXPECT_LE(1u, event->waitCalled);

    handler->closeThread();
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event->peekExecutionStatus());

    clReleaseEvent(event);
}

TEST_F(AsyncEventsHandlerMtTests, givenCallbacksRegisteredFromMultipleThreadsWhenEventCompletesThenAllOfThemAreExecuted) {
    auto handler = installMockHandler(true);
    auto event = new MockAsyncEvent(CL_COMMAND_MARKER, commandQueue);

    constexpr uint32_t numThreads = 4u;
    std::vector<CallbackTracker *> trackers;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (uint32_t i = 0; i < numThreads; i++) {
        trackers.push_back(&createTracker());
    }

    for (uint32_t i = 0; i < numThreads; i++) {
        threads.emplace_back([&, i]() {
            while (!start) {
                std::this_thread::yield();
            }
            EXPECT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, trackers[i]));
        });
    }

    start = true;
    for (auto &thread : threads) {
        thread.join();
    }

    event->complete();

    for (uint32_t i = 0; i < numThreads; i++) {
        EXPECT_TRUE(trackers[i]->waitForCount(1u)) << "callback " << i << " was not executed";
    }

    handler->closeThread();
    EXPECT_FALSE(event->peekHasCallbacks());

    clReleaseEvent(event);
}

TEST_F(AsyncEventsHandlerMtTests, givenUserEventWithCallbackWhenSetUserEventStatusIsCalledFromAnotherThreadThenCallbackIsExecuted) {
    installMockHandler(true);
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto &tracker = createTracker();
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    std::thread signalThread([&]() {
        EXPECT_EQ(CL_SUCCESS, clSetUserEventStatus(userEvent, CL_COMPLETE));
    });
    signalThread.join();

    EXPECT_TRUE(tracker.waitForCount(1u));
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);

    clReleaseEvent(userEvent);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
