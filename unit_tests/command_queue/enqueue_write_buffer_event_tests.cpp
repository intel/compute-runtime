/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/buffer_operations_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

TEST_F(EnqueueWriteBufferTypeTest, eventShouldBeReturned) {
    cl_bool blockingWrite = CL_TRUE;
    size_t offset = 0;
    size_t size = sizeof(cl_float);
    cl_float pDestMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    retVal = pCmdQ->enqueueWriteBuffer(
        srcBuffer.get(),
        blockingWrite,
        offset,
        size,
        pDestMemory,
        nullptr,
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EnqueueWriteBufferTypeTest, eventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingWrite = CL_TRUE;
    size_t offset = 0;
    size_t size = sizeof(cl_float);
    cl_float pDestMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto retVal = pCmdQ->enqueueWriteBuffer(
        srcBuffer.get(),
        blockingWrite,
        offset,
        size,
        pDestMemory,
        nullptr,
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_LE(19u, pEvent->taskLevel);

    delete pEvent;
}

TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndForcedCpuCopyOnWriteBufferAndDstPtrEqualSrcPtrWithEventsNotBlockedWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingRead,
                                       0,
                                       size,
                                       ptr,
                                       nullptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(17u, pCmdQ->taskLevel);

    pEvent->release();
}

TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndForcedCpuCopyOnWriteBufferAndEventNotReadyWhenWriteBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, Event::eventNotReady, 4);

    cl_bool blockingWrite = CL_FALSE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] = {&event1};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    cl_float mem[4];

    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingWrite,
                                       0,
                                       size,
                                       mem,
                                       nullptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(Event::eventNotReady, pEvent->taskLevel);
    EXPECT_EQ(Event::eventNotReady, pCmdQ->taskLevel);
    event1.taskLevel = 20;
    event1.setStatus(CL_COMPLETE);
    pEvent->updateExecutionStatus();
    pCmdQ->isQueueBlocked();
    pEvent->release();
}

TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndForcedCpuCopyOnWriteBufferAndDstPtrEqualSrcPtrWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingRead,
                                       0,
                                       size,
                                       ptr,
                                       nullptr,
                                       0,
                                       nullptr,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(17u, pEvent->taskLevel);
    EXPECT_EQ(17u, pCmdQ->taskLevel);

    pEvent->release();
}

TEST_F(EnqueueWriteBufferTypeTest, givenOutOfOrderQueueAndForcedCpuCopyOnWriteBufferAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdOOQ->enqueueWriteBuffer(srcBuffer.get(),
                                         blockingRead,
                                         0,
                                         size,
                                         ptr,
                                         nullptr,
                                         numEventsInWaitList,
                                         eventWaitList,
                                         &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(17u, pCmdOOQ->taskLevel);

    pEvent->release();
}
TEST_F(EnqueueWriteBufferTypeTest, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueWriteBuffer(srcBuffer.get(),
                                       blockingRead,
                                       0,
                                       size,
                                       ptr,
                                       nullptr,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdQ->taskLevel);

    pEvent->release();
}
TEST_F(EnqueueWriteBufferTypeTest, givenOutOfOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenWriteBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pClDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdOOQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdOOQ.get(), CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();

    retVal = pCmdOOQ->enqueueWriteBuffer(srcBuffer.get(),
                                         blockingRead,
                                         0,
                                         size,
                                         ptr,
                                         nullptr,
                                         numEventsInWaitList,
                                         eventWaitList,
                                         &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = castToObject<Event>(event);
    if (pCmdOOQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(taskLevelEvent2 + 1, pCmdOOQ->taskLevel);
        EXPECT_EQ(taskLevelEvent2 + 1, pEvent->taskLevel);
    } else {
        EXPECT_EQ(19u, pCmdOOQ->taskLevel);
        EXPECT_EQ(19u, pEvent->taskLevel);
    }

    pEvent->release();
}
