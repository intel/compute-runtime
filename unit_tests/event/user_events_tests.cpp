/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/os_context.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/mocks/mock_event.h"

#include "event_fixture.h"

TEST(UserEvent, testInitialStatusOfUserEventCmdQueue) {
    UserEvent uEvent;
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_QUEUE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_command_queue), retSize);
    auto cmdQueue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(0xdeadbeaf));
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_QUEUE, retSize, &cmdQueue, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(nullptr, cmdQueue);
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_TYPE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_event_info), retSize);
    auto cmd_type = CL_COMMAND_SVM_UNMAP;
    clGetEventInfo(&uEvent, CL_EVENT_COMMAND_TYPE, retSize, &cmd_type, 0);
    EXPECT_EQ(CL_COMMAND_USER, cmd_type);
}

TEST(UserEvent, testInitialStatusOfUserEventCmdType) {
    UserEvent uEvent;
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_TYPE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_event_info), retSize);
    auto cmd_type = CL_COMMAND_SVM_UNMAP;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_TYPE, retSize, &cmd_type, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(CL_COMMAND_USER, cmd_type);
}

TEST(UserEvent, testInitialStatusOfUserEventContext) {
    MockContext mc;
    cl_context dummyContext = &mc;
    UserEvent uEvent(&mc);
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_CONTEXT, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_context), retSize);
    cl_context context;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_CONTEXT, retSize, &context, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    ASSERT_EQ(context, dummyContext);
}

TEST(UserEvent, testInitialStatusOfUserEventContextIsNull) {
    UserEvent uEvent;
    cl_context context;
    auto RetValue = clGetEventInfo(&uEvent, CL_EVENT_CONTEXT, sizeof(cl_context), &context, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    ASSERT_EQ(context, nullptr);
}

TEST(UserEvent, testInitialStatusOfUserEventExecStatus) {
    UserEvent uEvent;
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_int), retSize);
    auto cmd_status = CL_COMPLETE;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, retSize, &cmd_status, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(CL_SUBMITTED, cmd_status);
}

TEST(UserEvent, givenUserEventWhenItIsQueriedForExecutionStatusThenClQueueIsReturned) {
    UserEvent uEvent;
    EXPECT_EQ(CL_QUEUED, uEvent.peekExecutionStatus());
}

TEST(UserEvent, givenUserEventWhenItIsCreatedThenItIsInInitialState) {
    UserEvent uEvent;
    EXPECT_TRUE(uEvent.isInitialEventStatus());
}

TEST(UserEvent, givenUserEventWhenItIsCreatedAndSetThenItIsNotInInitialState) {
    UserEvent uEvent;
    uEvent.setStatus(CL_COMPLETE);
    EXPECT_FALSE(uEvent.isInitialEventStatus());
}

TEST(UserEvent, testInitialStatusOfUserEventRefCount) {
    UserEvent uEvent;
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_REFERENCE_COUNT, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_uint), retSize);
    auto RefCount = 100;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_REFERENCE_COUNT, retSize, &RefCount, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(1, RefCount);
}

TEST(UserEvent, testUserEventSetUserEventStatus) {
    UserEvent uEvent;
    uEvent.setStatus(CL_COMPLETE);
    size_t retSize;
    cl_int RetValue;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(sizeof(cl_int), retSize);
    auto cmd_status = CL_COMPLETE;
    RetValue = clGetEventInfo(&uEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, retSize, &cmd_status, 0);
    ASSERT_EQ(CL_SUCCESS, RetValue);
    EXPECT_EQ(CL_COMPLETE, cmd_status);
}

TEST(UserEvent, initialUserEventDependantCountIsZero) {
    UserEvent uEvent;
    EXPECT_EQ(nullptr, uEvent.peekCommand());
}

TEST(UserEvent, initialUserEventStateIsNotReadyForSubmission) {
    UserEvent uEvent;
    EXPECT_FALSE(uEvent.isReadyForSubmission());
}

TEST(UserEvent, GIVENUserEventWHENgetTaskLevelTHENSuccess) {
    MyUserEvent uEvent;
    EXPECT_EQ(0U, uEvent.getTaskLevel());
    EXPECT_FALSE(uEvent.wait(false, false));
}

TEST(UserEvent, userEventAfterSetingStatusIsReadyForSubmission) {
    UserEvent uEvent;
    uEvent.setStatus(0);
    EXPECT_TRUE(uEvent.isReadyForSubmission());
}

TEST(UserEvent, givenUserEventWhenStatusIsCompletedThenReturnZeroTaskLevel) {
    UserEvent uEvent;

    uEvent.setStatus(CL_QUEUED);
    EXPECT_EQ(Event::eventNotReady, uEvent.getTaskLevel());

    uEvent.setStatus(CL_SUBMITTED);
    EXPECT_EQ(Event::eventNotReady, uEvent.getTaskLevel());

    uEvent.setStatus(CL_RUNNING);
    EXPECT_EQ(Event::eventNotReady, uEvent.getTaskLevel());

    uEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, uEvent.getTaskLevel());
}

typedef HelloWorldTest<HelloWorldFixtureFactory> EventTests;

TEST_F(MockEventTests, blockedUserEventPassedToEnqueueNdRangeWithoutReturnEventIsNotSubmittedToCSR) {
    uEvent = make_releaseable<UserEvent>();

    cl_event userEvent = uEvent.get();
    cl_event *eventWaitList = &userEvent;

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskCount = csr.peekTaskCount();

    //call NDR
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, 1);

    auto taskCountAfter = csr.peekTaskCount();

    //queue should be in blocked state at this moment, task level should be inherited from user event
    EXPECT_EQ(NEO::Event::eventNotReady, pCmdQ->taskLevel);

    //queue should be in blocked state at this moment, task count should be inherited from user event
    EXPECT_EQ(NEO::Event::eventNotReady, pCmdQ->taskCount);

    //queue should be in blocked state
    EXPECT_EQ(pCmdQ->isQueueBlocked(), true);

    //and virtual event should be created
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);

    //check if kernel was in fact not submitted
    EXPECT_EQ(taskCountAfter, taskCount);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, blockedUserEventPassedToEnqueueNdRangeWithReturnEventIsNotSubmittedToCSR) {
    uEvent = make_releaseable<UserEvent>();

    cl_event userEvent = uEvent.get();
    cl_event retEvent = nullptr;

    cl_event *eventWaitList = &userEvent;
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskCount = csr.peekTaskCount();

    //call NDR
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, 1, &retEvent);

    auto taskCountAfter = csr.peekTaskCount();

    //queue should be in blocked state at this moment, task level should be inherited from user event
    EXPECT_EQ(NEO::Event::eventNotReady, pCmdQ->taskLevel);

    //queue should be in blocked state at this moment, task count should be inherited from user event
    EXPECT_EQ(NEO::Event::eventNotReady, pCmdQ->taskCount);

    //queue should be in blocked state
    EXPECT_EQ(pCmdQ->isQueueBlocked(), true);

    //and virtual event should be created
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);

    //that matches the retEvent
    EXPECT_EQ(retEvent, pCmdQ->virtualEvent);

    //check if kernel was in fact not submitted
    EXPECT_EQ(taskCountAfter, taskCount);

    //and if normal event inherited status from user event
    Event *returnEvent = castToObject<Event>(retEvent);
    EXPECT_EQ(returnEvent->taskLevel, NEO::Event::eventNotReady);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, userEventInjectsCountOnReturnEventAndCreatesConnection) {
    uEvent = make_releaseable<UserEvent>();

    cl_event userEvent = uEvent.get();
    cl_event retEvent = nullptr;

    cl_event *eventWaitList = &userEvent;

    //call NDR
    callOneWorkItemNDRKernel(eventWaitList, 1, &retEvent);

    //check if dependency count is increased
    Event *returnEvent = castToObject<Event>(retEvent);

    EXPECT_EQ(1U, returnEvent->peekNumEventsBlockingThis());

    //check if user event knows his childs
    EXPECT_TRUE(uEvent->peekHasChildEvents());

    //make sure that proper event is set as child
    Event *childEvent = pCmdQ->virtualEvent;
    EXPECT_EQ(childEvent, uEvent->peekChildEvents()->ref);

    auto retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, givenNormalEventThatHasParentUserEventWhenUserEventIsUnblockedThenChildEventIsCompleteIfGpuCompletedProcessing) {
    UserEvent uEvent;
    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    uEvent.addChild(event);

    EXPECT_FALSE(event.updateStatusAndCheckCompletion());
    EXPECT_EQ(CL_QUEUED, event.peekExecutionStatus());

    uEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(CL_COMPLETE, event.peekExecutionStatus());
}

TEST_F(MockEventTests, twoUserEventInjectsCountOnReturnEventAndCreatesConnection) {
    uEvent = make_releaseable<UserEvent>();
    auto uEvent2 = make_releaseable<UserEvent>();
    cl_event retEvent = nullptr;

    cl_event eventWaitList[] = {uEvent.get(), uEvent2.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);

    //check if dependency count is increased
    Event *returnEvent = castToObject<Event>(retEvent);

    ASSERT_EQ(2U, returnEvent->peekNumEventsBlockingThis());

    //check if user event knows his childs
    EXPECT_TRUE(uEvent->peekHasChildEvents());

    //check if user event knows his childs
    EXPECT_TRUE(uEvent2->peekHasChildEvents());

    //make sure that proper event is set as child
    Event *childEvent = pCmdQ->virtualEvent;
    EXPECT_EQ(childEvent, uEvent->peekChildEvents()->ref);
    EXPECT_FALSE(childEvent->isReadyForSubmission());

    //make sure that proper event is set as child
    EXPECT_EQ(childEvent, uEvent2->peekChildEvents()->ref);

    //signal one user event, child event after this operation isn't be ready for submission
    uEvent->setStatus(0);
    //check if user event knows his childs
    EXPECT_FALSE(uEvent->peekHasChildEvents());
    EXPECT_EQ(1U, returnEvent->peekNumEventsBlockingThis());
    EXPECT_FALSE(returnEvent->isReadyForSubmission());

    auto retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uEvent2->setStatus(-1);
}

TEST_F(MockEventTests, twoUserEventInjectsCountOnNDR1whichIsPropagatedToNDR2viaVirtualEvent) {
    uEvent = make_releaseable<UserEvent>(context);
    auto uEvent2 = make_releaseable<UserEvent>(context);

    cl_event eventWaitList[] = {uEvent.get(), uEvent2.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR, no return Event
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //check if dependency count is increased
    Event *returnEvent1 = castToObject<Event>(pCmdQ->virtualEvent);

    ASSERT_EQ(2U, returnEvent1->peekNumEventsBlockingThis());

    //check if user event knows his childs
    EXPECT_TRUE(uEvent->peekHasChildEvents());

    //check if user event knows his childs
    EXPECT_TRUE(uEvent2->peekHasChildEvents());

    //make sure that proper event is set as child
    Event *childEvent = pCmdQ->virtualEvent;
    EXPECT_EQ(childEvent, uEvent->peekChildEvents()->ref);

    //make sure that proper event is set as child
    EXPECT_EQ(childEvent, uEvent2->peekChildEvents()->ref);

    //call NDR, no events, Virtual Event mustn't leak and will be bind to previous Virtual Event
    retVal = callOneWorkItemNDRKernel();
    EXPECT_EQ(CL_SUCCESS, retVal);

    //queue must be in blocked state
    EXPECT_EQ(pCmdQ->isQueueBlocked(), true);

    //check if virtual event2 is a child of virtual event 1
    VirtualEvent *returnEvent2 = castToObject<VirtualEvent>(pCmdQ->virtualEvent);
    ASSERT_TRUE(returnEvent1->peekHasChildEvents());

    EXPECT_EQ(returnEvent2, returnEvent1->peekChildEvents()->ref);

    //now signal both parents and see if all childs are notified
    uEvent->setStatus(CL_COMPLETE);
    uEvent2->setStatus(CL_COMPLETE);

    //queue shoud be in unblocked state
    EXPECT_EQ(pCmdQ->isQueueBlocked(), false);

    //finish returns immidieatly
    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, givenQueueThatIsBlockedByUserEventWhenIsQueueBlockedIsCalledThenVirtualEventOnlyQueriesForExecutionStatus) {
    struct mockEvent : public Event {
        using Event::Event;
        void updateExecutionStatus() override {
            updateExecutionStatusCalled = true;
        }
        bool updateExecutionStatusCalled = false;
    };
    mockEvent mockedVirtualEvent(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, 0);
    pCmdQ->virtualEvent = &mockedVirtualEvent;

    EXPECT_TRUE(pCmdQ->isQueueBlocked());
    EXPECT_FALSE(mockedVirtualEvent.updateExecutionStatusCalled);
    pCmdQ->virtualEvent = nullptr;
}

TEST_F(MockEventTests, finishDoesntBlockAfterUserEventSignaling) {
    uEvent = make_releaseable<UserEvent>(context);
    auto uEvent2 = make_releaseable<UserEvent>(context);

    cl_event eventWaitList[] = {uEvent.get(), uEvent2.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR, no return Event
    auto retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uEvent->setStatus(0);
    uEvent2->setStatus(0);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, userEventStatusPropagatedToNormalEvent) {
    uEvent = make_releaseable<UserEvent>();
    cl_event retEvent = nullptr;
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);

    //set user event status
    uEvent->setStatus(CL_COMPLETE);

    //wait for returend event, this should return with success.
    auto retVal = clWaitForEvents(1, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EventTests, userEventObtainsProperTaskLevelAfterSignaling) {
    UserEvent uEvent(context);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto taskLevel = csr.peekTaskLevel();

    csr.taskCount = 3;

    uEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(taskLevel, uEvent.taskLevel);

    csr.taskLevel = 2;
    csr.taskCount = 5;
    uEvent.setStatus(CL_COMPLETE);
    //even though csr taskLevel has changed, user event taskLevel should remain constant
    EXPECT_EQ(0u, uEvent.taskLevel);
}

TEST_F(MockEventTests, normalEventsBasingOnUserEventHasProperTaskLevel) {
    uEvent = make_releaseable<UserEvent>(context);
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskLevel = csr.peekTaskLevel();

    cl_event retEvent = nullptr;
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //check if dependency count is increased
    Event *returnEvent = castToObject<Event>(retEvent);
    EXPECT_EQ(Event::eventNotReady, returnEvent->taskLevel);
    EXPECT_EQ(Event::eventNotReady, returnEvent->peekTaskCount());

    //now set user event for complete status, this triggers update of childs.
    uEvent->setStatus(CL_COMPLETE);

    //child event should have the same taskLevel as parentEvent, as parent event is top of the tree and doesn't have any commands.
    EXPECT_EQ(returnEvent->taskLevel, taskLevel);
    EXPECT_EQ(csr.peekTaskCount(), returnEvent->peekTaskCount());

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, waitForEventThatWaitsOnSignaledUserEvent) {
    uEvent = make_releaseable<UserEvent>(context);

    cl_event retEvent = nullptr;
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //check if dependency count is increased
    Event *returnEvent = castToObject<Event>(retEvent);
    EXPECT_EQ(Event::eventNotReady, returnEvent->taskLevel);

    //now set user event for complete status, this triggers update of childs.
    uEvent->setStatus(CL_COMPLETE);

    retVal = clWaitForEvents(1, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, waitForAbortedUserEventReturnsFailure) {
    UserEvent uEvent(context);
    cl_event eventWaitList[] = {&uEvent};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //negative values indicate abortion
    uEvent.setStatus(-1);

    retVal = clWaitForEvents(sizeOfWaitList, eventWaitList);
    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);
}

TEST_F(MockEventTests, enqueueWithAbortedUserEventDoesntFlushToCSR) {
    uEvent = make_releaseable<UserEvent>(context);

    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent = nullptr;

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskCount = csr.peekTaskCount();

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //negative values indicate abortion
    uEvent->setStatus(-1);

    auto taskCountAfter = csr.peekTaskCount();

    EXPECT_EQ(taskCount, taskCountAfter);

    Event *pChildEvent = (Event *)retEvent;
    EXPECT_EQ(Event::eventNotReady, pChildEvent->getTaskLevel());

    cl_int eventStatus = 0;
    retVal = clGetEventInfo(retEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(-1, eventStatus);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, childEventDestructorDoesntProcessBlockedCommandsWhenParentEventWasAborted) {
    uEvent = make_releaseable<UserEvent>(context);

    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent = nullptr;

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskCount = csr.peekTaskCount();

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //call second NDR to create Virtual Event
    retVal = callOneWorkItemNDRKernel(&retEvent, 1, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //negative values indicate abortion
    uEvent->setStatus(-1);

    auto taskCountAfter = csr.peekTaskCount();

    EXPECT_EQ(taskCount, taskCountAfter);

    Event *pChildEvent = (Event *)retEvent;
    EXPECT_EQ(Event::eventNotReady, pChildEvent->taskLevel);

    cl_int eventStatus = 0;
    retVal = clGetEventInfo(retEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(-1, eventStatus);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    taskCountAfter = csr.peekTaskCount();
    EXPECT_EQ(taskCount, taskCountAfter);
}

TEST_F(MockEventTests, waitForEventDependingOnAbortedUserEventReturnsFailure) {
    uEvent = make_releaseable<UserEvent>(context);

    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent = nullptr;

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //negative values indicate abortion
    uEvent->setStatus(-1);

    eventWaitList[0] = retEvent;

    retVal = clWaitForEvents(sizeOfWaitList, eventWaitList);
    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, waitForEventDependingOnAbortedUserEventReturnsFailureTwoInputEvents) {
    uEvent = make_releaseable<UserEvent>(context);
    auto uEvent2 = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get(), uEvent2.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent = nullptr;

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //negative values indicate abortion
    uEvent->setStatus(-1);

    eventWaitList[0] = retEvent;

    retVal = clWaitForEvents(sizeOfWaitList, eventWaitList);
    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    uEvent2->setStatus(-1);
    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, finishReturnsSuccessAfterQueueIsAborted) {
    uEvent = make_releaseable<UserEvent>(context);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto taskLevel = csr.peekTaskLevel();

    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //negative values indicate abortion
    uEvent->setStatus(-1);

    //make sure we didn't asked CSR for task level for this event, as it is aborted
    EXPECT_NE(taskLevel, uEvent->taskLevel);

    retVal = clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, userEventRecordsDependantPacket) {
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList);

    //virtual event should register for this command packet
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    EXPECT_NE(nullptr, pCmdQ->virtualEvent->peekCommand());
    EXPECT_FALSE(pCmdQ->virtualEvent->peekIsCmdSubmitted());
}

TEST_F(MockEventTests, userEventDependantCommandPacketContainsValidCommandStream) {
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList);

    //virtual event should register for this command packet
    ASSERT_NE(nullptr, pCmdQ->virtualEvent);
    auto cmd = static_cast<CommandComputeKernel *>(pCmdQ->virtualEvent->peekCommand());
    EXPECT_NE(0u, cmd->getCommandStream()->getUsed());
}

TEST_F(MockEventTests, unblockingEventSendsBlockedPackets) {
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList);
    EXPECT_EQ(CL_SUCCESS, retVal);

    //task level untouched as queue blocked by user event
    EXPECT_EQ(csr.peekTaskLevel(), 0u);

    //virtual event have stored command packet
    Event *childEvent = pCmdQ->virtualEvent;
    EXPECT_NE(nullptr, childEvent);
    EXPECT_NE(nullptr, childEvent->peekCommand());
    EXPECT_FALSE(childEvent->isReadyForSubmission());

    EXPECT_NE(nullptr, childEvent->peekCommand());

    //signal the input user event
    uEvent->setStatus(0);

    EXPECT_EQ(csr.peekTaskLevel(), 1u);
}

TEST_F(MockEventTests, virtualEventObtainedFromReturnedEventCannotBeReleasedByIsQueueBlocked) {
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent;

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uEvent->setStatus(0);
    //call finish multiple times
    retVal |= clFinish(pCmdQ);
    retVal |= clFinish(pCmdQ);
    retVal |= clFinish(pCmdQ);

    EXPECT_EQ(CL_SUCCESS, retVal);

    //Virtual Event is gone, but retEvent still lives.
    EXPECT_EQ(nullptr, pCmdQ->virtualEvent);
    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, givenBlockedQueueThenCommandStreamDoesNotChangeWhileEnqueueAndAfterSignaling) {
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent;

    auto &cs = pCmdQ->getCS(1024);
    auto used = cs.getSpace(0);

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto used2 = cs.getSpace(0);

    EXPECT_EQ(used2, used);

    uEvent->setStatus(CL_COMPLETE);

    auto used3 = cs.getSpace(0);

    //call finish multiple times
    retVal |= clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(used3, used);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, givenUserEventThatHasCallbackAndBlockQueueWhenQueueIsQueriedForBlockedThenCallBackIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    struct EV : UserEvent {
        EV(Context *ctx) : UserEvent(ctx) {
        }
        void updateExecutionStatus() override {
            updated++;
        }

        int updated = 0;
    };

    auto event1 = MockEventBuilder::createAndFinalize<EV>(&pCmdQ->getContext());

    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data) {
            bool *called = (bool *)data;
            *called = true;
        }
    };

    cl_event eventWaitList[] = {event1};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);
    cl_event retEvent;

    //call NDR
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retEvent);
    ASSERT_EQ(retVal, CL_SUCCESS);

    bool callbackCalled = false;
    retVal = clSetEventCallback(event1, CL_COMPLETE, E2Clb::SignalEv2, &callbackCalled);
    ASSERT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(1, event1->updated);

    EXPECT_TRUE(pCmdQ->isQueueBlocked());

    event1->setStatus(CL_COMPLETE);

    // Must wait for event that depend on callback event to ensure callback is called.
    Event::waitForEvents(1, &retEvent);

    EXPECT_TRUE(callbackCalled);
    clReleaseEvent(retEvent);
    event1->release();
}

TEST_F(EventTests, CallBackAfterEnqueue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    UserEvent event1;
    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data)

        {
            UserEvent *event2 = static_cast<UserEvent *>(data);
            event2->setStatus(CL_COMPLETE);
        }
    };

    cl_event retEvent;
    //call NDR
    retVal = callOneWorkItemNDRKernel(nullptr, 0, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clSetEventCallback(retEvent, CL_COMPLETE, E2Clb::SignalEv2, &event1);

    cl_event events[] = {&event1};
    auto result = UserEvent::waitForEvents(sizeof(events) / sizeof(events[0]), events);
    EXPECT_EQ(result, CL_SUCCESS);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, CallBackAfterEnqueueWithoutWait) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    UserEvent event1(context);
    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data)

        {
            UserEvent *event2 = static_cast<UserEvent *>(data);
            event2->setStatus(CL_COMPLETE);
        }
    };

    cl_event retEvent;
    //call NDR
    retVal = callOneWorkItemNDRKernel(nullptr, 0, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clSetEventCallback(retEvent, CL_COMPLETE, E2Clb::SignalEv2, &event1);

    cl_event events[] = {&event1};
    auto result = UserEvent::waitForEvents(sizeof(events) / sizeof(events[0]), events);
    EXPECT_EQ(result, CL_SUCCESS);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MockEventTests, enqueueReadImageBlockedOnUserEvent) {
    cl_event retEvent;
    uEvent = make_releaseable<UserEvent>(context);
    cl_event eventWaitList[] = {uEvent.get()};

    auto image = clUniquePtr(Image2dHelper<>::create(this->context));
    ASSERT_NE(nullptr, image);

    auto retVal = EnqueueReadImageHelper<>::enqueueReadImage(pCmdQ,
                                                             image.get(),
                                                             false,
                                                             EnqueueReadImageTraits::origin,
                                                             EnqueueReadImageTraits::region,
                                                             EnqueueReadImageTraits::rowPitch,
                                                             EnqueueReadImageTraits::slicePitch,
                                                             EnqueueReadImageTraits::hostPtr,
                                                             EnqueueReadImageTraits::mapAllocation,
                                                             1,
                                                             eventWaitList,
                                                             &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clSetUserEventStatus(uEvent.get(), CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, waitForEventsDestroysTemporaryAllocations) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto memoryManager = pCmdQ->getDevice().getMemoryManager();

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());

    GraphicsAllocation *temporaryAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    csr.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryAllocation), TEMPORARY_ALLOCATION);

    EXPECT_EQ(temporaryAllocation, csr.getTemporaryAllocations().peekHead());

    temporaryAllocation->updateTaskCount(10, csr.getOsContext().getContextId());

    Event event(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 3, 11);

    cl_event eventWaitList[] = {&event};

    event.waitForEvents(1, eventWaitList);

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
}

TEST_F(EventTest, UserEvent_Wait_NonBlocking) {
    UserEvent event;
    auto result = event.wait(false, false);
    EXPECT_FALSE(result);
}

TEST_F(EventTest, UserEvent_Wait_Single) {
    UserEvent event1;
    event1.setStatus(CL_COMPLETE);

    cl_event events[] = {&event1};
    auto result = UserEvent::waitForEvents(sizeof(events) / sizeof(events[0]), events);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST_F(EventTest, UserEvent_Wait_Multiple_OutOfOrder_Callbacks) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    UserEvent event1;
    struct E2Clb {
        static void CL_CALLBACK SignalEv2(cl_event e, cl_int status, void *data)

        {
            UserEvent *event2 = static_cast<UserEvent *>(data);
            event2->setStatus(CL_COMPLETE);
        }
    };

    UserEvent event2;
    event2.addCallback(E2Clb::SignalEv2, CL_COMPLETE, &event1);
    event2.setStatus(CL_COMPLETE);
    cl_event events[] = {&event1, &event2};
    auto result = UserEvent::waitForEvents(sizeof(events) / sizeof(events[0]), events);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST_F(EventTests, WhenCalbackWasRegisteredOnCallbackExecutionPassesCorrectExecutionStatus) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    struct HelperClb {
        static void CL_CALLBACK SetClbStatus(cl_event e, cl_int status, void *data)

        {
            cl_int *ret = static_cast<cl_int *>(data);
            *ret = status;
        }
    };

    cl_event retEvent;
    retVal = callOneWorkItemNDRKernel(nullptr, 0, &retEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_int submittedClbExecStatus = -1;
    cl_int runningClbExecStatus = -1;
    cl_int completeClbExecStatus = -1;
    retVal = clSetEventCallback(retEvent, CL_SUBMITTED, HelperClb::SetClbStatus, &submittedClbExecStatus);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(retEvent, CL_RUNNING, HelperClb::SetClbStatus, &runningClbExecStatus);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clSetEventCallback(retEvent, CL_COMPLETE, HelperClb::SetClbStatus, &completeClbExecStatus);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto result = UserEvent::waitForEvents(1, &retEvent);
    ASSERT_EQ(result, CL_SUCCESS);

    EXPECT_EQ(CL_SUBMITTED, submittedClbExecStatus);
    EXPECT_EQ(CL_RUNNING, runningClbExecStatus);
    EXPECT_EQ(CL_COMPLETE, completeClbExecStatus);

    clReleaseEvent(retEvent);
}

TEST_F(EventTests, eventOnQueryReturnsCorrentNumberOfBlockerEvents) {
    UserEvent uEvent1(context);
    UserEvent uEvent2(context);
    UserEvent uEvent3(context);

    EXPECT_EQ(0U, uEvent1.peekNumEventsBlockingThis());
    EXPECT_EQ(0U, uEvent2.peekNumEventsBlockingThis());
    EXPECT_EQ(0U, uEvent3.peekNumEventsBlockingThis());

    cl_event eventWaitList[] = {&uEvent1, &uEvent2, &uEvent3};
    int sizeOfWaitList = sizeof(eventWaitList) / sizeof(cl_event);

    cl_event retClEvent;
    retVal = callOneWorkItemNDRKernel(eventWaitList, sizeOfWaitList, &retClEvent);
    Event *retEvent = (Event *)retClEvent;
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, retEvent);

    EXPECT_EQ(3U, retEvent->peekNumEventsBlockingThis());

    retVal = clSetUserEventStatus(&uEvent1, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2U, retEvent->peekNumEventsBlockingThis());

    retVal = clSetUserEventStatus(&uEvent2, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1U, retEvent->peekNumEventsBlockingThis());

    retVal = clSetUserEventStatus(&uEvent3, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0U, retEvent->peekNumEventsBlockingThis());

    retVal |= clFinish(pCmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EventTests, blockedUserEventPassedToEnqueueNdRangeDoesntRetainCommandQueue) {
    auto userEvent = clCreateUserEvent(pContext, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto uEvent = (UserEvent *)userEvent;
    ASSERT_NE(nullptr, uEvent);

    auto cmdQueue = uEvent->getCommandQueue();
    ASSERT_EQ(nullptr, cmdQueue);

    auto intitialRefCount = pCmdQ->getRefInternalCount();

    auto retVal = callOneWorkItemNDRKernel(&userEvent, 1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cmdQueue = uEvent->getCommandQueue();
    ASSERT_EQ(nullptr, cmdQueue);

    // Virtual event add refference to cmq queue.
    EXPECT_EQ(intitialRefCount + 1, pCmdQ->getRefInternalCount());

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->isQueueBlocked();
    // VirtualEvent should be freed, so refCount should equal initial value
    EXPECT_EQ(intitialRefCount, pCmdQ->getRefInternalCount());
}

TEST_F(EventTests, givenUserEventWhenSetStatusIsDoneThenDeviceMutextisAcquired) {
    struct mockedEvent : public UserEvent {
        using UserEvent::UserEvent;
        bool setStatus(cl_int status) override {
            auto commandStreamReceiverOwnership = ctx->getDevice(0)->getDefaultEngine().commandStreamReceiver->obtainUniqueOwnership();
            mutexProperlyAcquired = commandStreamReceiverOwnership.owns_lock();
            return true;
        }
        bool mutexProperlyAcquired = false;
    };

    mockedEvent mockEvent(this->context);
    clSetUserEventStatus(&mockEvent, CL_COMPLETE);
    EXPECT_TRUE(mockEvent.mutexProperlyAcquired);
}
