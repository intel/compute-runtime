/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/array_count.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_defs.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

#include <queue>

using namespace NEO;

using ClEnqueueWaitForEventsTests = ApiTests;

TEST_F(ClEnqueueWaitForEventsTests, GivenInvalidCommandQueueWhenClEnqueueWaitForEventsIsCalledThenReturnError) {
    auto retVal = CL_SUCCESS;

    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueWaitForEvents(
        nullptr,
        1,
        &userEvent);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseEvent(userEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueWaitForEventsTests, GivenQueueIncapableWhenEnqueingWaitForEventsThenInvalidOperationReturned) {
    MockEvent<Event> events[] = {
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    const cl_event waitList[] = {events, events + 1, events + 2};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    auto retVal = clEnqueueWaitForEvents(pCommandQueue, waitListSize, waitList);
    EXPECT_EQ(CL_SUCCESS, retVal);

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL);
    retVal = clEnqueueWaitForEvents(pCommandQueue, waitListSize, waitList);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(ClEnqueueWaitForEventsTests, GivenProperParamsWhenClEnqueueWaitForEventsIsCalledAndEventStatusIsCompleteThenWaitAndReturnSuccess) {
    struct MyEvent : public UserEvent {
        MyEvent(Context *context)
            : UserEvent(context) {
        }
        WaitStatus wait(bool blocking, bool quickKmdSleep) override {
            wasWaitCalled = true;
            return WaitStatus::ready;
        };
        bool wasWaitCalled = false;
    };
    auto retVal = CL_SUCCESS;

    auto event = std::make_unique<MyEvent>(pContext);
    cl_event clEvent = static_cast<cl_event>(event.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueWaitForEvents(
        pCommandQueue,
        1,
        &clEvent);
    EXPECT_EQ(true, event->wasWaitCalled);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueWaitForEventsTests, GivenProperParamsWhenClEnqueueWaitForEventsIsCalledAndEventStatusIsNotCompleteThenReturnError) {
    auto retVal = CL_SUCCESS;

    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetUserEventStatus(userEvent, -1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueWaitForEvents(
        pCommandQueue,
        1,
        &userEvent);

    EXPECT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    retVal = clReleaseEvent(userEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueWaitForEventsTests, GivenInvalidEventWhenClEnqueueWaitForEventsIsCalledThenReturnError) {
    auto retVal = CL_SUCCESS;

    auto validUserEvent = clCreateUserEvent(
        pContext,
        &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto ptr = std::make_unique<char[]>(sizeof(Event));
    cl_event invalidEvent = reinterpret_cast<cl_event>(ptr.get());
    cl_event events[]{validUserEvent, invalidEvent, validUserEvent};

    retVal = clEnqueueWaitForEvents(
        pCommandQueue,
        3,
        events);

    EXPECT_EQ(CL_INVALID_EVENT, retVal);

    retVal = clReleaseEvent(validUserEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(ClEnqueueWaitForEventsTests, givenOoqWhenWaitingForEventThenCallWaitForTimestamps) {
    MockCommandQueueHw<FamilyType> commandQueueHw(pContext, pDevice, nullptr);

    DebugManagerStateRestore restore;
    debugManager.flags.EnableTimestampWaitForQueues.set(4);
    commandQueueHw.setOoqEnabled();

    MockEvent<Event> event(&commandQueueHw, CL_COMMAND_READ_BUFFER, 0, 0);
    event.timestampPacketContainer = std::make_unique<MockTimestampPacketContainer>(*pDevice->getUltCommandStreamReceiver<FamilyType>().getTimestampPacketAllocator(), 1);

    auto node = event.timestampPacketContainer->peekNodes()[0];
    auto contextEnd = ptrOffset(node->getCpuBase(), node->getContextEndOffset());

    *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd) = 0;

    cl_event hEvent = &event;

    auto retVal = clEnqueueWaitForEvents(&commandQueueHw, 1, &hEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(commandQueueHw.waitForTimestampsCalled);
    EXPECT_TRUE(commandQueueHw.latestWaitForTimestampsStatus);
}

struct ClEnqueueWaitForTimestampsTests : public ClEnqueueWaitForEventsTests {
    void SetUp() override {
        debugManager.flags.EnableTimestampWaitForQueues.set(4);
        debugManager.flags.EnableTimestampWaitForEvents.set(4);
        debugManager.flags.EnableTimestampPacket.set(1);

        ClEnqueueWaitForEventsTests::SetUp();
    }

    DebugManagerStateRestore restore;
};

HWTEST_F(ClEnqueueWaitForTimestampsTests, givenIoqWhenWaitingForLatestEventThenDontCheckQueueCompletion) {
    MockCommandQueueHw<FamilyType> commandQueueHw(pContext, pDevice, nullptr);

    MockKernelWithInternals kernel(*pDevice);

    cl_event event0, event1;

    const size_t gws[] = {1, 1, 1};
    commandQueueHw.enqueueKernel(kernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event0);
    commandQueueHw.enqueueKernel(kernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &event1);

    auto eventObj0 = castToObjectOrAbort<Event>(event0);
    auto eventObj1 = castToObjectOrAbort<Event>(event1);

    auto node0 = eventObj0->getTimestampPacketNodes()->peekNodes()[0];
    auto node1 = eventObj1->getTimestampPacketNodes()->peekNodes()[0];

    auto contextEnd0 = ptrOffset(node0->getCpuBase(), node0->getContextEndOffset());
    auto contextEnd1 = ptrOffset(node1->getCpuBase(), node1->getContextEndOffset());

    *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd0) = 0;
    *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd1) = 0;

    EXPECT_EQ(0u, commandQueueHw.isCompletedCalled);

    EXPECT_EQ(CL_SUCCESS, clEnqueueWaitForEvents(&commandQueueHw, 1, &event0));
    EXPECT_EQ(1u, commandQueueHw.isCompletedCalled);

    EXPECT_EQ(CL_SUCCESS, clEnqueueWaitForEvents(&commandQueueHw, 1, &event1));
    EXPECT_EQ(1u, commandQueueHw.isCompletedCalled);

    commandQueueHw.setOoqEnabled();
    EXPECT_EQ(CL_SUCCESS, clEnqueueWaitForEvents(&commandQueueHw, 1, &event0));
    EXPECT_EQ(2u, commandQueueHw.isCompletedCalled);

    EXPECT_EQ(CL_SUCCESS, clEnqueueWaitForEvents(&commandQueueHw, 1, &event1));
    EXPECT_EQ(3u, commandQueueHw.isCompletedCalled);

    clReleaseEvent(event0);
    clReleaseEvent(event1);
}

HWTEST_F(ClEnqueueWaitForEventsTests, givenAlreadyCompletedEventWhenWaitForCompletionThenCheckGpuStateOnce) {
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto csrTagAddress = ultCsr.getTagAddress();

    TaskCountType eventTaskCount = 5;

    *csrTagAddress = eventTaskCount - 1;

    MockEvent<Event> event1(pCommandQueue, CL_COMMAND_READ_BUFFER, 0, eventTaskCount);
    MockEvent<Event> event2(pCommandQueue, CL_COMMAND_READ_BUFFER, 0, eventTaskCount);
    cl_event hEvent1 = &event1;
    cl_event hEvent2 = &event2;

    EXPECT_EQ(0u, pCommandQueue->isCompletedCalled);

    // Event 1
    event1.updateExecutionStatus();
    EXPECT_EQ(1u, pCommandQueue->isCompletedCalled);

    event1.updateExecutionStatus();
    EXPECT_EQ(2u, pCommandQueue->isCompletedCalled);

    *csrTagAddress = eventTaskCount;

    event1.updateExecutionStatus();
    EXPECT_EQ(3u, pCommandQueue->isCompletedCalled);

    event1.updateExecutionStatus();
    EXPECT_EQ(3u, pCommandQueue->isCompletedCalled);

    auto retVal = clEnqueueWaitForEvents(pCommandQueue, 1, &hEvent1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(4u, pCommandQueue->isCompletedCalled);

    // Event 2
    retVal = clEnqueueWaitForEvents(pCommandQueue, 1, &hEvent2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // clEnqueueWaitForEvents signals completion before isCompletedCalled()
    EXPECT_EQ(5u, pCommandQueue->isCompletedCalled);

    retVal = clEnqueueWaitForEvents(pCommandQueue, 1, &hEvent2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(6u, pCommandQueue->isCompletedCalled);

    event2.updateExecutionStatus();
    EXPECT_EQ(6u, pCommandQueue->isCompletedCalled);

    event2.isCompleted();
    EXPECT_EQ(6u, pCommandQueue->isCompletedCalled);
}

struct GTPinMockCommandQueue : MockCommandQueue {
    GTPinMockCommandQueue(Context *context, MockClDevice *device) : MockCommandQueue(context, device, nullptr, false) {}
    WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
        return MockCommandQueue::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, true, true);
    }

    inline static bool onCommandBufferCompleteCalled = false;
};

void onCommandBufferComplete(gtpin::command_buffer_handle_t cb) {
    GTPinMockCommandQueue::onCommandBufferCompleteCalled = true;
}

namespace NEO {
extern bool isGTPinInitialized;
extern gtpin::ocl::gtpin_events_t gtpinCallbacks;
extern std::deque<gtpinkexec_t> kernelExecQueue;
TEST_F(ClEnqueueWaitForEventsTests, WhenGTPinIsInitializedAndEnqueingWaitForEventsThenGTPinIsNotified) {

    auto gtpinMockCommandQueue = new GTPinMockCommandQueue(pContext, pDevice);
    pCommandQueue->release();
    pCommandQueue = static_cast<MockCommandQueue *>(gtpinMockCommandQueue);

    auto retVal = CL_SUCCESS;
    isGTPinInitialized = true;

    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    gtpin::resource_handle_t resource = 0;
    gtpin::command_buffer_handle_t commandBuffer = 0;
    gtpinkexec_t kExec;
    kExec.pKernel = pKernel;
    kExec.gtpinResource = (cl_mem)resource;
    kExec.isTaskCountValid = true;
    kExec.commandBuffer = commandBuffer;
    kExec.pCommandQueue = (CommandQueue *)pCommandQueue;
    kernelExecQueue.push_back(kExec);

    MockEvent<Event> events[] = {
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
        {pCommandQueue, CL_COMMAND_READ_BUFFER, 0, 0},
    };
    MockEvent<Event> event = {pCommandQueue,
                              CL_COMMAND_READ_BUFFER,
                              0,
                              0};
    const cl_event waitList[] = {events, events + 1, events + 2};
    const cl_uint waitListSize = static_cast<cl_uint>(arrayCount(waitList));

    ASSERT_FALSE(GTPinMockCommandQueue::onCommandBufferCompleteCalled);

    retVal = clEnqueueWaitForEvents(
        pCommandQueue,
        waitListSize,
        waitList);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_TRUE(GTPinMockCommandQueue::onCommandBufferCompleteCalled);

    isGTPinInitialized = false;
    kernelExecQueue.clear();
    GTPinMockCommandQueue::onCommandBufferCompleteCalled = false;
}
} // namespace NEO
