/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {
class CommandQueue;
class Context;
} // namespace NEO

using namespace NEO;
using namespace ::testing;

class AsyncEventsHandlerTests : public ::testing::Test {
  public:
    class MyEvent : public Event {
      public:
        MyEvent(Context *ctx, CommandQueue *cmdQueue, cl_command_type cmdType, TaskCountType taskLevel, TaskCountType taskCount)
            : Event(ctx, cmdQueue, cmdType, taskLevel, taskCount) {
            handler.reset(new MockHandler());
        }
        int getExecutionStatus() {
            // return execution status without updating
            return executionStatus.load();
        }
        void setTaskStamp(TaskCountType taskLevel, TaskCountType taskCount) {
            this->taskLevel.store(taskLevel);
            this->updateTaskCount(taskCount, 0);
        }

        WaitStatus wait(bool blocking, bool quickKmdSleep) override {
            waitCalled++;
            handler->allowAsyncProcess.store(false);
            return waitResult;
        }

        uint32_t waitCalled = 0u;
        WaitStatus waitResult = WaitStatus::ready;
        std::unique_ptr<MockHandler> handler;
    };

    static void CL_CALLBACK callbackFcn(cl_event e, cl_int status, void *data) {
        ++(*(int *)data);
    }

    void SetUp() override {
        dbgRestore.reset(new DebugManagerStateRestore());
        debugManager.flags.EnableAsyncEventsHandler.set(false);
        handler.reset(new MockHandler());
        context = makeReleaseable<MockContext>();

        commandQueue = makeReleaseable<MockCommandQueue>(context.get(), context->getDevice(0), nullptr, false);

        *(commandQueue->getGpgpuCommandStreamReceiver().getTagAddress()) = commandQueue->getHeaplessStateInitEnabled() ? 1 : 0;

        event1 = makeReleaseable<MyEvent>(context.get(), commandQueue.get(), CL_COMMAND_BARRIER, CompletionStamp::notReady, CompletionStamp::notReady);
        event2 = makeReleaseable<MyEvent>(context.get(), commandQueue.get(), CL_COMMAND_BARRIER, CompletionStamp::notReady, CompletionStamp::notReady);
        event3 = makeReleaseable<MyEvent>(context.get(), commandQueue.get(), CL_COMMAND_BARRIER, CompletionStamp::notReady, CompletionStamp::notReady);
    }

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<MockHandler> handler;
    int counter = 0;

    ReleaseableObjectPtr<MockContext> context;
    ReleaseableObjectPtr<MockCommandQueue> commandQueue;
    ReleaseableObjectPtr<MyEvent> event1;
    ReleaseableObjectPtr<MyEvent> event2;
    ReleaseableObjectPtr<MyEvent> event3;
};

TEST_F(AsyncEventsHandlerTests, givenEventsWhenListIsProcessedThenUpdateExecutionStatus) {

    event1->setTaskStamp(0, 0);
    event2->setTaskStamp(0, 0);

    handler->registerEvent(event1.get());
    handler->registerEvent(event2.get());

    EXPECT_EQ(CL_QUEUED, event1->getExecutionStatus());
    EXPECT_EQ(CL_QUEUED, event2->getExecutionStatus());

    handler->process();

    EXPECT_NE(CL_QUEUED, event1->getExecutionStatus());
    EXPECT_NE(CL_QUEUED, event2->getExecutionStatus());

    EXPECT_TRUE(handler->peekIsListEmpty()); // auto-unregister when no callbacs
}

TEST_F(AsyncEventsHandlerTests, WhenProcessIsCompletedThenRefInternalCountIsDecremented) {
    event1->setTaskStamp(CompletionStamp::notReady, 0);

    handler->registerEvent(event1.get());
    EXPECT_EQ(2, event1->getRefInternalCount());
    handler->process();
    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_EQ(1, event1->getRefInternalCount());
}

TEST_F(AsyncEventsHandlerTests, givenNotCalledCallbacksWhenListIsProcessedThenDontUnregister) {
    int submittedCounter(0), completeCounter(0);
    event1->setTaskStamp(CompletionStamp::notReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &submittedCounter);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &completeCounter);
    handler->registerEvent(event1.get());

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
    event1->setTaskStamp(CompletionStamp::notReady - 1, CompletionStamp::notReady + 1);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    handler->registerEvent(event1.get());
    handler->registerEvent(event1.get());

    handler->process();
    EXPECT_EQ(CL_SUBMITTED, event1->getExecutionStatus());
    EXPECT_EQ(1, counter);
    EXPECT_TRUE(handler->peekIsListEmpty());
}

TEST_F(AsyncEventsHandlerTests, givenEventsNotHandledByHandlderWhenDestructingThenUnreferenceAll) {
    auto myHandler = new MockHandler();
    event1->setTaskStamp(CompletionStamp::notReady, 0);
    event2->setTaskStamp(CompletionStamp::notReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    event2->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    myHandler->registerEvent(event1.get());
    myHandler->process();
    myHandler->registerEvent(event2.get());

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
    event1->setTaskStamp(CompletionStamp::notReady, 0);
    event2->setTaskStamp(CompletionStamp::notReady, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);
    event2->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    handler->registerEvent(event1.get());
    handler->process();
    handler->registerEvent(event2.get());

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

TEST_F(AsyncEventsHandlerTests, WhenHandlerIsCreatedThenThreadIsNotCreatedByDefault) {
    MockHandler myHandler;
    EXPECT_EQ(nullptr, myHandler.thread.get());
}

TEST_F(AsyncEventsHandlerTests, WhenHandlerIsRegisteredThenThreadIsCreated) {
    event1->setTaskStamp(CompletionStamp::notReady, 0);

    EXPECT_FALSE(handler->openThreadCalled);
    handler->registerEvent(event1.get());
    EXPECT_TRUE(handler->openThreadCalled);
}

TEST_F(AsyncEventsHandlerTests, WhenProcessingAsynchronouslyThenBothThreadsCompelete) {
    debugManager.flags.EnableAsyncEventsHandler.set(true);

    event1->setTaskStamp(CompletionStamp::notReady, CompletionStamp::notReady + 1);
    event2->setTaskStamp(CompletionStamp::notReady, CompletionStamp::notReady + 1);

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

    context->getAsyncEventsHandler().closeThread();
}

TEST_F(AsyncEventsHandlerTests, WhenThreadIsDestructedThenGetThreadReturnsNull) {
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
    debugManager.flags.EnableAsyncEventsHandler.set(true);
    auto myHandler = new MockHandler(true);
    context->getAsyncEventsHandlerUniquePtr().reset(myHandler);
    event1->setTaskStamp(0, 0);
    event1->addCallback(&this->callbackFcn, CL_SUBMITTED, &counter);

    EXPECT_EQ(static_cast<MockHandler *>(&context->getAsyncEventsHandler()), myHandler);
    EXPECT_FALSE(event1->peekHasCallbacks());
    EXPECT_FALSE(myHandler->openThreadCalled);
}

TEST_F(AsyncEventsHandlerTests, givenUserEventWhenCallbackIsAddedThenDontRegister) {
    debugManager.flags.EnableAsyncEventsHandler.set(true);
    auto myHandler = new MockHandler(true);
    context->getAsyncEventsHandlerUniquePtr().reset(myHandler);

    UserEvent userEvent;
    userEvent.addCallback(&this->callbackFcn, CL_COMPLETE, &counter);

    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_TRUE(handler->peekIsRegisterListEmpty());
    EXPECT_TRUE(userEvent.peekHasCallbacks());
    userEvent.decRefInternal();
}

TEST_F(AsyncEventsHandlerTests, givenRegisteredEventsWhenProcessIsCalledThenReturnCandidateWithLowestTaskCount) {
    int event1Counter(0), event2Counter(0), event3Counter(0);

    event1->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 2 : 1);
    event2->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 3 : 2);
    event3->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 4 : 3);

    event2->addCallback(&this->callbackFcn, CL_COMPLETE, &event2Counter);
    handler->registerEvent(event2.get());
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &event1Counter);
    handler->registerEvent(event1.get());
    event3->addCallback(&this->callbackFcn, CL_COMPLETE, &event3Counter);
    handler->registerEvent(event3.get());

    auto sleepCandidate = handler->process();
    EXPECT_EQ(event1.get(), sleepCandidate);

    event1->setStatus(CL_COMPLETE);
    event2->setStatus(CL_COMPLETE);
    event3->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, givenEventWithoutCallbacksWhenProcessedThenDontReturnAsSleepCandidate) {
    event1->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 2 : 1);
    event2->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 3 : 2);

    handler->registerEvent(event1.get());
    event2->addCallback(&this->callbackFcn, CL_COMPLETE, &counter);
    handler->registerEvent(event2.get());

    auto sleepCandidate = handler->process();
    EXPECT_EQ(event2.get(), sleepCandidate);

    event2->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, givenNoGpuHangAndSleepCandidateWhenProcessedThenCallWaitWithQuickKmdSleepRequest) {
    event1->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 2 : 1);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &counter);
    event1->handler->registerEvent(event1.get());
    event1->handler->allowAsyncProcess.store(true);

    MockHandler::asyncProcess(event1->handler.get());

    EXPECT_EQ(1u, event1->waitCalled);
    EXPECT_NE(Event::executionAbortedDueToGpuHang, event1->peekExecutionStatus());

    event1->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, givenSleepCandidateAndGpuHangWhenProcessedThenCallWaitAndSetExecutionStatusToAbortedDueToGpuHang) {
    event1->setTaskStamp(0, commandQueue->getHeaplessStateInitEnabled() ? 2 : 1);
    event1->addCallback(&this->callbackFcn, CL_COMPLETE, &counter);
    event1->handler->registerEvent(event1.get());
    event1->handler->allowAsyncProcess.store(true);
    event1->waitResult = WaitStatus::gpuHang;

    MockHandler::asyncProcess(event1->handler.get());

    EXPECT_EQ(1u, event1->waitCalled);
    EXPECT_EQ(Event::executionAbortedDueToGpuHang, event1->peekExecutionStatus());

    event1->setStatus(CL_COMPLETE);
}

TEST_F(AsyncEventsHandlerTests, WhenReturningThenAsyncProcessWillCallProcessList) {
    Event *event = new Event(nullptr, CL_COMMAND_NDRANGE_KERNEL, 0, 0);

    handler->registerEvent(event);
    handler->allowAsyncProcess.store(false);

    MockHandler::asyncProcess(handler.get());
    EXPECT_TRUE(handler->peekIsListEmpty());
    EXPECT_EQ(1, event->getRefInternalCount());

    event->release();
}
