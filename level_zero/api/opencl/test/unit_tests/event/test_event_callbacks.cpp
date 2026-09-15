/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/test/common/fixtures/leo_event_callbacks_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace NEO {
namespace LEO {
namespace ult {

using EventCallbacksTests = Test<EventCallbacksFixture>;

TEST_F(EventCallbacksTests, givenNewEventWhenQueriedForCallbacksThenNoneAreRegistered) {
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);

    EXPECT_FALSE(event->peekHasCallbacks());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenNotCompletedEventWhenAddingCallbackThenItIsRegisteredWithTheHandlerAndNotExecuted) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();

    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    EXPECT_EQ(0u, tracker.count);
    EXPECT_TRUE(event->peekHasCallbacks());
    EXPECT_TRUE(mockHandler->openThreadCalled);
    EXPECT_FALSE(mockHandler->peekIsRegisterListEmpty());

    signalEvent(event);
    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenCompletedEventWhenSettingCallbackThenItIsExecutedImmediatelyAndNotRegistered) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    signalEvent(event);
    auto &tracker = createTracker();

    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(static_cast<cl_event>(event), tracker.lastEvent);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);
    EXPECT_FALSE(event->peekHasCallbacks());
    EXPECT_TRUE(mockHandler->peekIsRegisterListEmpty());
    EXPECT_FALSE(mockHandler->openThreadCalled);

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenNotCompletedEventWhenSettingQueuedOrSubmittedCallbackThenItIsExecutedImmediatelyWithItsOwnTargetStatus) {
    auto mockHandler = installMockHandler(false);

    for (auto callbackType : {CL_QUEUED, CL_SUBMITTED}) {
        auto event = new Event(CL_COMMAND_MARKER, commandQueue);
        auto &tracker = createTracker();

        EXPECT_EQ(CL_SUCCESS, clSetEventCallback(event, callbackType, &CallbackTracker::callback, &tracker));

        EXPECT_EQ(1u, tracker.count);
        EXPECT_EQ(callbackType, tracker.lastStatus);
        EXPECT_FALSE(event->peekHasCallbacks());
        EXPECT_TRUE(mockHandler->peekIsRegisterListEmpty());

        signalEvent(event);
        clReleaseEvent(event);
    }
}

TEST_F(EventCallbacksTests, givenCallbacksOfDifferentTypesWhenEventCompletesThenAllOfThemAreExecutedOnce) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &submittedTracker = createTracker();
    auto &runningTracker = createTracker();
    auto &completeTracker = createTracker();

    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_SUBMITTED, &CallbackTracker::callback, &submittedTracker));
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_RUNNING, &CallbackTracker::callback, &runningTracker));
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &completeTracker));
    ASSERT_EQ(1u, submittedTracker.count);
    ASSERT_EQ(0u, runningTracker.count + completeTracker.count);

    signalEvent(event);
    event->updateExecutionStatus();

    EXPECT_EQ(1u, submittedTracker.count);
    EXPECT_EQ(1u, runningTracker.count);
    EXPECT_EQ(1u, completeTracker.count);
    EXPECT_EQ(CL_SUBMITTED, submittedTracker.lastStatus);
    EXPECT_EQ(CL_RUNNING, runningTracker.lastStatus);
    EXPECT_EQ(CL_COMPLETE, completeTracker.lastStatus);
    EXPECT_FALSE(event->peekHasCallbacks());

    event->updateExecutionStatus();
    EXPECT_EQ(1u, completeTracker.count);

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenMultipleCallbacksOfTheSameTypeWhenEventCompletesThenAllOfThemAreExecuted) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &firstTracker = createTracker();
    auto &secondTracker = createTracker();

    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &firstTracker);
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &secondTracker);

    signalEvent(event);
    event->updateExecutionStatus();

    EXPECT_EQ(1u, firstTracker.count);
    EXPECT_EQ(1u, secondTracker.count);

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenNotCompletedEventWhenUpdatingExecutionStatusThenCallbacksAreNotExecuted) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    event->updateExecutionStatus();

    EXPECT_EQ(0u, tracker.count);
    EXPECT_EQ(CL_SUBMITTED, event->peekExecutionStatus());
    EXPECT_TRUE(event->peekHasCallbacks());

    signalEvent(event);
    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenPendingCallbackWhenEventIsReleasedByTheApiThenTheCallbackKeepsTheEventAlive) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(event));
    EXPECT_EQ(0u, tracker.count);

    signalEvent(event);

    EXPECT_EQ(nullptr, mockHandler->process());
    EXPECT_EQ(1u, tracker.count);
    EXPECT_TRUE(mockHandler->peekIsListEmpty());
}

// Note from OCL spec:
//    "All callbacks registered for an event object must be called.
//     All enqueued callbacks shall be called before the event object is destroyed."
TEST_F(EventCallbacksTests, givenUserEventWithPendingCallbackWhenReleasedWithoutBeingSetCompleteThenTheCallbackIsExecutedWithTheTerminatedStatus) {
    installMockHandler(false);
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto &tracker = createTracker();
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_COMPLETE, &CallbackTracker::callback, &tracker));
    ASSERT_EQ(0u, tracker.count);

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(userEvent));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(userEvent, tracker.lastEvent);
    EXPECT_EQ(Event::executionTerminatedOnDestruction, tracker.lastStatus);
}

TEST_F(EventCallbacksTests, givenPendingEventWhenTheHandlerDropsItsLastReferenceThenTheCallbackIsExecutedWithTheTerminatedStatus) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    ASSERT_EQ(CL_SUCCESS, clReleaseEvent(event));
    ASSERT_EQ(0u, tracker.count);

    // Tearing the handler down releases the events it still tracks - here that is the last reference.
    installMockHandler(false);

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(Event::executionTerminatedOnDestruction, tracker.lastStatus);
}

TEST_F(EventCallbacksTests, givenCompletedEventWithPendingCallbackWhenDestroyedThenTheCallbackKeepsItsOwnTargetStatus) {
    installMockHandler(false);
    auto userEvent = new Event(context);
    auto &tracker = createTracker();
    userEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    signalEvent(userEvent);
    ASSERT_EQ(CL_COMPLETE, userEvent->queryAndUpdateEventStatus());
    ASSERT_EQ(0u, tracker.count);
    ASSERT_TRUE(userEvent->peekHasCallbacks());

    clReleaseEvent(userEvent);

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);
}

TEST_F(EventCallbacksTests, givenEventWhenAbortingDueToGpuHangThenCallbacksAreExecutedWithTheAbortStatus) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    event->abortExecutionDueToGpuHang();

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, tracker.lastStatus);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event->peekExecutionStatus());
    EXPECT_FALSE(event->peekHasCallbacks());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenAbortedEventWhenQueryingStatusThenTheAbortStatusIsNotOverwritten) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    event->abortExecutionDueToGpuHang();
    signalEvent(event);

    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event->queryAndUpdateEventStatus());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenCompletedEventWhenQueryingStatusAgainThenTheCachedStatusIsReturned) {
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    EXPECT_EQ(CL_SUBMITTED, event->queryAndUpdateEventStatus());

    signalEvent(event);

    EXPECT_EQ(CL_COMPLETE, event->queryAndUpdateEventStatus());
    EXPECT_EQ(CL_COMPLETE, event->queryAndUpdateEventStatus());
    EXPECT_EQ(CL_COMPLETE, event->peekExecutionStatus());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenUserEventWhenAddingCallbackThenItIsNotRegisteredWithTheHandler) {
    auto mockHandler = installMockHandler(false);
    auto userEvent = new Event(context);
    auto &tracker = createTracker();

    EXPECT_TRUE(userEvent->isUserEvent());

    userEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    EXPECT_EQ(0u, tracker.count);
    EXPECT_TRUE(userEvent->peekHasCallbacks());
    EXPECT_FALSE(mockHandler->openThreadCalled);
    EXPECT_TRUE(mockHandler->peekIsRegisterListEmpty());

    clReleaseEvent(userEvent);
}

TEST_F(EventCallbacksTests, givenUserEventWithCallbackWhenSignalledThenCallbackIsExecuted) {
    installMockHandler(false);
    auto userEvent = new Event(context);
    auto &tracker = createTracker();
    userEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    EXPECT_EQ(ZE_RESULT_SUCCESS, userEvent->signal(CL_COMPLETE));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);
    EXPECT_EQ(CL_COMPLETE, userEvent->peekExecutionStatus());

    clReleaseEvent(userEvent);
}

TEST_F(EventCallbacksTests, givenUserEventWithCallbackWhenTerminatedThenCallbackReceivesTheErrorStatus) {
    installMockHandler(false);
    auto userEvent = new Event(context);
    auto &tracker = createTracker();
    userEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    EXPECT_EQ(ZE_RESULT_SUCCESS, userEvent->signal(CL_INVALID_OPERATION));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(CL_INVALID_OPERATION, tracker.lastStatus);
    EXPECT_EQ(CL_INVALID_OPERATION, userEvent->peekExecutionStatus());

    clReleaseEvent(userEvent);
}

TEST_F(EventCallbacksTests, givenUserEventWhenSetUserEventStatusIsCalledThenPendingCallbackIsExecuted) {
    installMockHandler(false);
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto &tracker = createTracker();
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_COMPLETE, &CallbackTracker::callback, &tracker));
    ASSERT_EQ(0u, tracker.count);

    EXPECT_EQ(CL_SUCCESS, clSetUserEventStatus(userEvent, CL_COMPLETE));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(userEvent, tracker.lastEvent);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);

    clReleaseEvent(userEvent);
}

TEST_F(EventCallbacksTests, givenUserEventWhenSetUserEventStatusIsCalledWithErrorThenCallbackReceivesThatError) {
    installMockHandler(false);
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto &tracker = createTracker();
    ASSERT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    EXPECT_EQ(CL_SUCCESS, clSetUserEventStatus(userEvent, CL_INVALID_VALUE));

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(CL_INVALID_VALUE, tracker.lastStatus);
    EXPECT_TRUE(context->isTerminated());

    clReleaseEvent(userEvent);
}

TEST_F(EventCallbacksTests, givenCompletedEventWhenSetEventCallbackIsCalledThenCallbackIsExecutedAndSuccessReturned) {
    installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    signalEvent(event);
    auto &tracker = createTracker();

    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(event, CL_COMPLETE, &CallbackTracker::callback, &tracker));

    EXPECT_EQ(1u, tracker.count);

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenSignalledEventWhenWaitingForAsyncCompletionThenSuccessIsReturned) {
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    signalEvent(event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, event->wait(Event::asyncCompletionWaitTimeoutNs));

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenNotSignalledEventWhenWaitingWithZeroTimeoutThenNotReadyIsReturned) {
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);

    // Must not wait out asyncCompletionWaitTimeoutNs here - the event is never signalled, so the
    // full timeout would be spent spinning in hostSynchronize. On hosts where WAITPKG is enabled
    // that spin bumps the process-global CpuIntrinsicsTests::tpauseCounter that other ULTs assert on.
    EXPECT_EQ(ZE_RESULT_NOT_READY, event->wait(0));

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenClDeviceWhenQueryingAsyncEventsHandlerThenTheOneOwnedByThePlatformIsReturned) {
    auto mockHandler = installMockHandler(false);

    EXPECT_EQ(platform, clDevice->getPlatform());
    EXPECT_EQ(mockHandler, &clDevice->getPlatform()->getAsyncEventsHandler());
}

TEST_F(EventCallbacksTests, givenRegisteredEventWhenProcessedByTheHandlerThenTheCallbackRunsAndTheEventIsReleased) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);
    ASSERT_FALSE(mockHandler->peekIsRegisterListEmpty());

    EXPECT_EQ(event, mockHandler->process());
    EXPECT_FALSE(mockHandler->peekIsListEmpty());
    EXPECT_EQ(0u, tracker.count);

    signalEvent(event);

    EXPECT_EQ(nullptr, mockHandler->process());
    EXPECT_TRUE(mockHandler->peekIsListEmpty());
    EXPECT_EQ(1u, tracker.count);

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenMultiplePendingEventsWhenProcessedByTheHandlerThenTheFirstOneIsTheSleepCandidate) {
    auto mockHandler = installMockHandler(false);
    auto firstEvent = new Event(CL_COMMAND_MARKER, commandQueue);
    auto secondEvent = new Event(CL_COMMAND_MARKER, commandQueue);
    firstEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());
    secondEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());

    EXPECT_EQ(firstEvent, mockHandler->process());

    signalEvent(firstEvent);

    EXPECT_EQ(secondEvent, mockHandler->process());

    signalEvent(secondEvent);
    clReleaseEvent(firstEvent);
    clReleaseEvent(secondEvent);
}

struct MockEventWithHangingWait : public Event {
    using Event::Event;
    using Event::wait;

    ADDMETHOD_NOBASE(wait, ze_result_t, ZE_RESULT_ERROR_DEVICE_LOST, (uint64_t timeout));
};

TEST_F(EventCallbacksTests, givenHangingEventWhenProcessedByTheHandlerThenCallbacksAreAbortedWithTheHangStatus) {
    auto mockHandler = installMockHandler(false);
    auto event = new MockEventWithHangingWait(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    auto sleepCandidate = mockHandler->process();
    ASSERT_EQ(event, sleepCandidate);

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, sleepCandidate->wait(Event::asyncCompletionWaitTimeoutNs));
    sleepCandidate->abortExecutionDueToGpuHang();

    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, tracker.lastStatus);
    EXPECT_EQ(nullptr, mockHandler->process());
    EXPECT_TRUE(mockHandler->peekIsListEmpty());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenEventRegisteredTwiceBeforeTransferWhenTransferringThenItIsTrackedOnce) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &firstTracker = createTracker();
    auto &secondTracker = createTracker();

    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &firstTracker);
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &secondTracker);

    // One reference from construction plus one per registration.
    ASSERT_EQ(3, event->getRefInternalCount());

    EXPECT_EQ(event, mockHandler->process());

    EXPECT_EQ(1u, mockHandler->list.size());
    EXPECT_EQ(2, event->getRefInternalCount());

    signalEvent(event);

    EXPECT_EQ(nullptr, mockHandler->process());
    EXPECT_EQ(1u, firstTracker.count);
    EXPECT_EQ(1u, secondTracker.count);
    EXPECT_TRUE(mockHandler->peekIsListEmpty());
    EXPECT_EQ(1, event->getRefInternalCount());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenEventAlreadyOnTheListWhenRegisteredAgainThenTheDuplicateIsDiscarded) {
    auto mockHandler = installMockHandler(false);
    auto event = new Event(CL_COMMAND_MARKER, commandQueue);
    auto &firstTracker = createTracker();
    auto &secondTracker = createTracker();

    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &firstTracker);
    EXPECT_EQ(event, mockHandler->process());
    ASSERT_EQ(1u, mockHandler->list.size());
    ASSERT_EQ(2, event->getRefInternalCount());

    // Registered again while it is already being tracked.
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &secondTracker);
    ASSERT_EQ(3, event->getRefInternalCount());

    EXPECT_EQ(event, mockHandler->process());

    EXPECT_EQ(1u, mockHandler->list.size());
    EXPECT_EQ(2, event->getRefInternalCount());

    signalEvent(event);

    EXPECT_EQ(nullptr, mockHandler->process());
    EXPECT_EQ(1u, firstTracker.count);
    EXPECT_EQ(1u, secondTracker.count);
    EXPECT_EQ(1, event->getRefInternalCount());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenDistinctEventsWhenTransferringTheRegisterListThenAllOfThemAreTracked) {
    auto mockHandler = installMockHandler(false);
    auto firstEvent = new Event(CL_COMMAND_MARKER, commandQueue);
    auto secondEvent = new Event(CL_COMMAND_MARKER, commandQueue);

    firstEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());
    secondEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());

    EXPECT_EQ(firstEvent, mockHandler->process());

    EXPECT_EQ(2u, mockHandler->list.size());
    EXPECT_EQ(2, firstEvent->getRefInternalCount());
    EXPECT_EQ(2, secondEvent->getRefInternalCount());

    signalEvent(firstEvent);
    signalEvent(secondEvent);
    mockHandler->process();

    clReleaseEvent(firstEvent);
    clReleaseEvent(secondEvent);
}

// The ULT harness points Thread::createFunc at a MockThread factory, so the real openThread() and
// closeThread() can be driven here without an OS thread ever running asyncProcess().
TEST_F(EventCallbacksTests, givenHandlerWhenOpeningAndClosingTheThreadThenItIsCreatedOnceAndClearedOnce) {
    auto mockHandler = installMockHandler(true);
    ASSERT_EQ(nullptr, mockHandler->thread.get());
    ASSERT_FALSE(mockHandler->allowAsyncProcess);

    mockHandler->openThread();
    auto firstThread = mockHandler->thread.get();
    EXPECT_NE(nullptr, firstThread);
    EXPECT_TRUE(mockHandler->allowAsyncProcess);

    // Second call is a no-op, the existing thread is kept.
    mockHandler->openThread();
    EXPECT_EQ(firstThread, mockHandler->thread.get());

    mockHandler->closeThread();
    EXPECT_EQ(nullptr, mockHandler->thread.get());
    EXPECT_FALSE(mockHandler->allowAsyncProcess);

    // Closing again must be harmless - this is also what ~AsyncEventsHandler() does.
    mockHandler->closeThread();
    EXPECT_EQ(nullptr, mockHandler->thread.get());
}

struct MockEventWithControlledWait : public Event {
    using Event::Event;
    using Event::wait;

    ADDMETHOD_NOBASE(wait, ze_result_t, ZE_RESULT_NOT_READY, (uint64_t timeout));
};

// Runs AsyncEventsHandler::asyncProcess() on the test thread instead of a worker. transferRegisterList()
// is called once per loop iteration while asyncMtx is held, so counting it stops the loop deterministically.
struct MockLoopControlledAsyncEventsHandler : public MockAsyncEventsHandler {
    using MockAsyncEventsHandler::MockAsyncEventsHandler;

    void transferRegisterList() override {
        MockAsyncEventsHandler::transferRegisterList();
        if (++transferRegisterListCalled >= stopAfterTransferCount) {
            allowAsyncProcess = false;
        }
    }

    // Executes the loop body once, then leaves through the !allowAsyncProcess path that drains the list.
    // At least one event must be registered first, otherwise the loop blocks on asyncCond with no one to notify.
    void runOneIteration() {
        transferRegisterListCalled = 0u;
        allowAsyncProcess = true;
        asyncProcess(this);
    }

    uint32_t transferRegisterListCalled = 0u;
    uint32_t stopAfterTransferCount = 2u;
};

TEST_F(EventCallbacksTests, givenSleepCandidateWhoseWaitSucceedsWhenAsyncLoopRunsThenExecutionIsNotAborted) {
    auto mockHandler = installMockHandler<MockLoopControlledAsyncEventsHandler>(false);
    auto event = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    event->waitResult = ZE_RESULT_SUCCESS;
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    mockHandler->runOneIteration();

    EXPECT_EQ(1u, event->waitCalled);
    EXPECT_EQ(0u, tracker.count);
    EXPECT_EQ(CL_SUBMITTED, event->peekExecutionStatus());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenSleepCandidateWhoseWaitTimesOutWhenAsyncLoopRunsThenExecutionIsNotAborted) {
    auto mockHandler = installMockHandler<MockLoopControlledAsyncEventsHandler>(false);
    auto event = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    event->waitResult = ZE_RESULT_NOT_READY;
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    mockHandler->runOneIteration();

    EXPECT_EQ(1u, event->waitCalled);
    EXPECT_EQ(0u, tracker.count);
    EXPECT_EQ(CL_SUBMITTED, event->peekExecutionStatus());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenSleepCandidateWhoseWaitFailsWhenAsyncLoopRunsThenExecutionIsAborted) {
    auto mockHandler = installMockHandler<MockLoopControlledAsyncEventsHandler>(false);
    auto event = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    event->waitResult = ZE_RESULT_ERROR_DEVICE_LOST;
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    mockHandler->runOneIteration();

    EXPECT_EQ(1u, event->waitCalled);
    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, tracker.lastStatus);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event->peekExecutionStatus());
    EXPECT_TRUE(mockHandler->peekIsListEmpty());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenAlreadyCompletedEventWhenAsyncLoopRunsThenCallbackIsExecutedWithoutPickingASleepCandidate) {
    auto mockHandler = installMockHandler<MockLoopControlledAsyncEventsHandler>(false);
    auto event = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    auto &tracker = createTracker();
    event->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);
    signalEvent(event);

    mockHandler->runOneIteration();

    EXPECT_EQ(0u, event->waitCalled);
    EXPECT_EQ(1u, tracker.count);
    EXPECT_EQ(CL_COMPLETE, tracker.lastStatus);
    EXPECT_TRUE(mockHandler->peekIsListEmpty());

    clReleaseEvent(event);
}

TEST_F(EventCallbacksTests, givenStillPendingEventsWhenTheAsyncLoopStopsThenTheirHandlerReferencesAreReleased) {
    auto mockHandler = installMockHandler<MockLoopControlledAsyncEventsHandler>(false);
    auto firstEvent = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    auto secondEvent = new MockEventWithControlledWait(CL_COMMAND_MARKER, commandQueue);
    firstEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());
    secondEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &createTracker());

    // One reference from construction plus the one taken by registerEvent().
    ASSERT_EQ(2, firstEvent->getRefInternalCount());
    ASSERT_EQ(2, secondEvent->getRefInternalCount());

    mockHandler->runOneIteration();

    EXPECT_TRUE(mockHandler->peekIsListEmpty());
    EXPECT_EQ(1, firstEvent->getRefInternalCount());
    EXPECT_EQ(1, secondEvent->getRefInternalCount());
    EXPECT_TRUE(firstEvent->peekHasCallbacks());
    EXPECT_TRUE(secondEvent->peekHasCallbacks());

    clReleaseEvent(firstEvent);
    clReleaseEvent(secondEvent);
}

struct WhiteBoxEventWithHandle : public Event {
    using Event::Event;
    using Event::eventHandle;
};

TEST_F(EventCallbacksTests, givenFailingHostSignalWhenSignallingUserEventThenStatusIsUnchangedAndCallbacksAreNotExecuted) {
    installMockHandler(false);
    auto userEvent = new WhiteBoxEventWithHandle(context);
    auto &tracker = createTracker();
    userEvent->addCallback(&CallbackTracker::callback, CL_COMPLETE, &tracker);

    L0::ult::Mock<L0::ult::Event> failingL0Event{};
    failingL0Event.hostSignalResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    {
        VariableBackup<ze_event_handle_t> handleBackup{&userEvent->eventHandle, failingL0Event.toHandle()};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, userEvent->signal(CL_COMPLETE));
    }

    EXPECT_EQ(0u, tracker.count);
    EXPECT_EQ(CL_SUBMITTED, userEvent->peekExecutionStatus());
    EXPECT_TRUE(userEvent->peekHasCallbacks());

    clReleaseEvent(userEvent);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
