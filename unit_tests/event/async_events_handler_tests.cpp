/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/event/async_events_handler.h"
#include "runtime/event/event.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_async_event_handler.h"
#include "test.h"
#include "gmock/gmock.h"

using namespace OCLRT;
using namespace ::testing;

class AsyncEventsHandlerTests : public ::testing::Test {
  public:
    class MyEvent : public Event {
      public:
        MyEvent(CommandQueue *cmdQueue, cl_command_type cmdType, uint32_t taskLevel, uint32_t taskCount)
            : Event(cmdQueue, cmdType, taskLevel, taskCount) {}
        int getExecutionStatus() {
            //return execution status without updating
            return executionStatus.load();
        }
        void setTaskStamp(uint32_t taskLevel, uint32_t taskCount) {
            this->taskLevel.store(taskLevel);
            this->updateTaskCount(taskCount);
        }

        MOCK_METHOD2(wait, bool(bool blocking, bool quickKmdSleep));
    };

    static void CL_CALLBACK callbackFcn(cl_event e, cl_int status, void *data) {
        ++(*(int *)data);
    }

    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        DebugManager.flags.EnableAsyncEventsHandler.set(false);
        handler.reset(new MockHandler());

        event1 = new NiceMock<MyEvent>(nullptr, CL_COMMAND_BARRIER, Event::eventNotReady, Event::eventNotReady);
        event2 = new NiceMock<MyEvent>(nullptr, CL_COMMAND_BARRIER, Event::eventNotReady, Event::eventNotReady);
        event3 = new NiceMock<MyEvent>(nullptr, CL_COMMAND_BARRIER, Event::eventNotReady, Event::eventNotReady);
    }

    void TearDown() override {
        event1->release();
        event2->release();
        event3->release();
    }

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<MockHandler> handler;
    int counter = 0;

    NiceMock<MyEvent> *event1 = nullptr;
    NiceMock<MyEvent> *event2 = nullptr;
    NiceMock<MyEvent> *event3 = nullptr;
};

TEST_F(AsyncEventsHandlerTests, givenEventsWhenListIsProcessedThenUpdateExecutionStatus) {
    event1->setTaskStamp(0, 0);
    event2->setTaskStamp(0, 0);

    handler->registerEvent(event1);
    handler->registerEvent(event2);

    EXPECT_EQ(CL_QUEUED, event1->getExecutionStatus());
    EXPECT_EQ(CL_QUEUED, event2->getExecutionStatus());

    handler->process();

    EXPECT_NE(CL_QUEUED, event1->getExecutionStatus());
    EXPECT_NE(CL_QUEUED, event2->getExecutionStatus());

    EXPECT_TRUE(handler->peekIsListEmpty()); // auto-unregister when no callbacs
}

TEST_F(AsyncEventsHandlerTests, updateEventsRefInternalCount) {
    event1->setTaskStamp(Event::eventNotReady, 0);

    handler->registerEvent(event1);
    EXPECT_EQ(2, event1->getRefInternalCount());
    handler->process();
    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_EQ(1, event1->getRefInternalCount());
}

TEST_F(AsyncEventsHandlerTests, givenNotCalledCallbacksWhenListIsProcessedThenDontUnregister) {
    int submittedCounter(0), completeCounter(0);
    event1->setTaskStamp(Event::eventNotReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &submittedCounter);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &completeCounter);
    handler->registerEvent(event1);

    auto expect = [&](int status, int sCounter, int cCounter, bool empty) {
        EXPECT_EQ(status, event1->getExecutionStatus());
        EXPECT_EQ(sCounter, submittedCounter);
        EXPECT_EQ(cCounter, completeCounter);
        EXPECT_EQ(empty, handler->peekIsListEmpty());
    };

    handler->process();
    expect(CL_QUEUED, 0, 0, false);

    event1->setStatus(CL_SUBMITTED);
    handler->process();
    expect(CL_SUBMITTED, 1, 0, false);

    event1->setStatus(CL_COMPLETE);
    handler->process();
    expect(CL_COMPLETE, 1, 1, true);
}

TEST_F(AsyncEventsHandlerTests, givenExternallSynchronizedEventWhenListIsProcessedAndEventIsNotInCompleteStateThenDontUnregister) {
    struct ExternallySynchronizedEvent : Event {
        ExternallySynchronizedEvent(int numUpdatesBeforeCompletion)
            : Event(nullptr, 0, 0, 0), numUpdatesBeforeCompletion(numUpdatesBeforeCompletion) {
        }

        void updateExecutionStatus() override {
            ++updateCount;
            if (updateCount == numUpdatesBeforeCompletion) {
                transitionExecutionStatus(CL_COMPLETE);
            }
        }

        bool isExternallySynchronized() const override {
            return true;
        }

        int updateCount = 0;
        int numUpdatesBeforeCompletion = 1;
    };

    constexpr int numUpdatesBeforeCompletion = 5;
    auto *event = new ExternallySynchronizedEvent(numUpdatesBeforeCompletion);

    handler->registerEvent(event);
    for (int i = 0; i < numUpdatesBeforeCompletion * 2; ++i) {
        handler->process();
    }

    EXPECT_EQ(CL_COMPLETE, event->peekExecutionStatus());
    EXPECT_EQ(numUpdatesBeforeCompletion, event->updateCount);

    event->release();
}

TEST_F(AsyncEventsHandlerTests, givenDoubleRegisteredEventWhenListIsProcessedAndNoCallbacksToProcessThenUnregister) {
    event1->setTaskStamp(Event::eventNotReady - 1, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    handler->registerEvent(event1);
    handler->registerEvent(event1);

    handler->process();
    EXPECT_EQ(CL_SUBMITTED, event1->getExecutionStatus());
    EXPECT_EQ(1, counter);
    EXPECT_TRUE(handler->peekIsListEmpty());
}

TEST_F(AsyncEventsHandlerTests, givenEventsNotHandledByHandlderWhenDestructingThenUnreferenceAll) {
    auto myHandler = new MockHandler();
    event1->setTaskStamp(Event::eventNotReady, 0);
    event2->setTaskStamp(Event::eventNotReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    event2->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    myHandler->registerEvent(event1);
    myHandler->process();
    myHandler->registerEvent(event2);

    EXPECT_FALSE(myHandler->peekIsListEmpty());
    EXPECT_FALSE(myHandler->peekIsRegisterListEmpty());

    EXPECT_EQ(3, event1->getRefInternalCount());
    EXPECT_EQ(3, event2->getRefInternalCount());
    delete myHandler;
    // 1 left because of callbacks
    EXPECT_EQ(2, event1->getRefInternalCount());
    EXPECT_EQ(2, event2->getRefInternalCount());
    // release callbacks
    event1->setStatus(CL_SUBMITTED);
    event2->setStatus(CL_SUBMITTED);
}

TEST_F(AsyncEventsHandlerTests, givenEventsNotHandledByHandlderWhenAsyncExecutionInterruptedThenUnreferenceAll) {
    event1->setTaskStamp(Event::eventNotReady, 0);
    event2->setTaskStamp(Event::eventNotReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    event2->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    handler->registerEvent(event1);
    handler->process();
    handler->registerEvent(event2);

    EXPECT_FALSE(handler->peekIsListEmpty());
    EXPECT_FALSE(handler->peekIsRegisterListEmpty());

    EXPECT_EQ(3, event1->getRefInternalCount());
    EXPECT_EQ(3, event2->getRefInternalCount());
    handler->allowAsyncProcess.store(false);
    MockHandler::asyncProcess(handler.get()); // enter and exit because of allowAsyncProcess == false
    EXPECT_EQ(2, event1->getRefInternalCount());
    EXPECT_EQ(2, event2->getRefInternalCount());
    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_TRUE(handler->peekIsRegisterListEmpty());

    event1->setStatus(CL_SUBMITTED);
    event2->setStatus(CL_SUBMITTED);
}

TEST_F(AsyncEventsHandlerTests, dontCreateThreadByDefault) {
    MockHandler myHandler;
    EXPECT_EQ(nullptr, myHandler.thread.get());
}

TEST_F(AsyncEventsHandlerTests, createThreadOnFirstRegister) {
    event1->setTaskStamp(Event::eventNotReady, 0);

    EXPECT_FALSE(handler->openThreadCalled);
    handler->registerEvent(event1);
    EXPECT_TRUE(handler->openThreadCalled);
}

TEST_F(AsyncEventsHandlerTests, processAsynchronously) {
    DebugManager.flags.EnableAsyncEventsHandler.set(true);

    event1->setTaskStamp(Event::eventNotReady, 0);
    event2->setTaskStamp(Event::eventNotReady, 0);

    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    event2->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    EXPECT_EQ(CL_QUEUED, event1->getExecutionStatus());
    EXPECT_EQ(CL_QUEUED, event2->getExecutionStatus());

    // unblock to submit
    event1->taskLevel.store(0);
    event2->taskLevel.store(0);

    while (event1->getExecutionStatus() == CL_QUEUED || event2->getExecutionStatus() == CL_QUEUED) {
        std::this_thread::yield();
    }

    EXPECT_EQ(CL_SUBMITTED, event1->getExecutionStatus());
    EXPECT_EQ(CL_SUBMITTED, event2->getExecutionStatus());

    platform()->getAsyncEventsHandler()->closeThread();
}

TEST_F(AsyncEventsHandlerTests, callToWakeupAndDeletedWhenDestructed) {
    handler->allowThreadCreating = true;
    handler->openThread();

    // wait for sleep
    while (handler->transferCounter == 0) {
        std::this_thread::yield();
    }
    std::unique_lock<std::mutex> lock(handler->asyncMtx);
    lock.unlock();
    handler->closeThread();
    EXPECT_EQ(nullptr, handler->thread.get());
}

TEST_F(AsyncEventsHandlerTests, givenReadyEventWhenCallbackIsAddedThenDontOpenThread) {
    DebugManager.flags.EnableAsyncEventsHandler.set(true);
    auto myHandler = new MockHandler(true);
    auto oldHandler = platform()->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(myHandler));
    event1->setTaskStamp(0, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    EXPECT_EQ(platform()->getAsyncEventsHandler(), myHandler);
    EXPECT_FALSE(event1->peekHasCallbacks());
    EXPECT_FALSE(myHandler->openThreadCalled);
    platform()->setAsyncEventsHandler(std::move(oldHandler));
}

TEST_F(AsyncEventsHandlerTests, givenUserEventWhenCallbackIsAddedThenDontRegister) {
    DebugManager.flags.EnableAsyncEventsHandler.set(true);
    auto myHandler = new MockHandler(true);
    auto oldHandler = platform()->setAsyncEventsHandler(std::unique_ptr<MockHandler>(myHandler));

    UserEvent userEvent;
    userEvent.addCallback(&this->callbackFcn, CL_COMPLETE, &counter);

    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_TRUE(handler->peekIsRegisterListEmpty());
    EXPECT_TRUE(userEvent.peekHasCallbacks());
    userEvent.decRefInternal();

    platform()->setAsyncEventsHandler(std::move(oldHandler));
}

TEST_F(AsyncEventsHandlerTests, givenRegistredEventsWhenProcessIsCalledThenReturnCandidateWithLowestTaskCount) {
    int event1Counter(0), event2Counter(0), event3Counter(0);

    event1->setTaskStamp(0, 1);
    event2->setTaskStamp(0, 2);
    event3->setTaskStamp(0, 3);

    event2->addCallback(&this->callbackFcn, CL_COMPLETE, &event2Counter);
    handler->registerEvent(event2);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &event1Counter);
    handler->registerEvent(event1);
    event3->addCallback(&this->callbackFcn, CL_COMPLETE, &event3Counter);
    handler->registerEvent(event3);

    auto sleepCandidate = handler->process();
    EXPECT_EQ(event1, sleepCandidate);

    event1->setStatus(CL_COMPLETE);
    event2->setStatus(CL_COMPLETE);
    event3->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, givenEventWithoutCallbacksWhenProcessedThenDontReturnAsSleepCandidate) {
    event1->setTaskStamp(0, 1);
    event2->setTaskStamp(0, 2);

    handler->registerEvent(event1);
    event2->addCallback(&this->callbackFcn, CL_COMPLETE, &counter);
    handler->registerEvent(event2);

    auto sleepCandidate = handler->process();
    EXPECT_EQ(event2, sleepCandidate);

    event2->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, givenSleepCandidateWhenProcessedThenCallWaitWithQuickKmdSleepRequest) {
    event1->setTaskStamp(0, 1);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &counter);
    handler->registerEvent(event1);
    handler->allowAsyncProcess.store(true);

    // break infinite loop after first iteartion
    auto unsetAsyncFlag = [&](bool blocking, bool quickKmdSleep) {
        handler->allowAsyncProcess.store(false);
        return true;
    };

    EXPECT_CALL(*event1, wait(true, true)).Times(1).WillOnce(Invoke(unsetAsyncFlag));

    MockHandler::asyncProcess(handler.get());

    event1->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, asyncProcessCallsProcessListBeforeReturning) {
    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    handler->registerEvent(event);
    handler->allowAsyncProcess.store(false);

    MockHandler::asyncProcess(handler.get());
    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_EQ(1, event->getRefInternalCount());

    event->release();
}
