/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueReadBuffer;

TEST_F(EnqueueReadBuffer, GivenPointerToEventListWhenReadingBufferThenEventIsReturned) {
    cl_bool blockingRead = CL_TRUE;
    size_t offset = 0;
    size_t size = sizeof(cl_float);
    cl_float pDestMemory[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto retVal = pCmdQ->enqueueReadBuffer(
        srcBuffer.get(),
        blockingRead,
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
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EnqueueReadBuffer, WhenReadingBufferThenEventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_bool blockingRead = CL_TRUE;
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

    auto retVal = pCmdQ->enqueueReadBuffer(
        srcBuffer.get(),
        blockingRead,
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
    EXPECT_EQ(19u + 1u, pEvent->taskLevel);

    delete pEvent;
}
TEST_F(EnqueueReadBuffer, givenInOrderQueueAndForcedCpuCopyOnReadBufferAndDstPtrEqualSrcPtrWithEventsNotBlockedWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
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
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
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

TEST_F(EnqueueReadBuffer, givenInOrderQueueAndForcedCpuCopyOnReadBufferAndDstPtrEqualSrcPtrWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    cl_bool blockingRead = CL_TRUE;
    size_t size = sizeof(cl_float);

    cl_event event = nullptr;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    void *ptr = srcBuffer->getCpuAddressForMemoryTransfer();
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
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

TEST_F(EnqueueReadBuffer, givenOutOfOrderQueueAndForcedCpuCopyOnReadBufferAndDstPtrEqualSrcPtrWithEventsNotBlockedWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));
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
    retVal = pCmdOOQ->enqueueReadBuffer(srcBuffer.get(),
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

TEST_F(EnqueueReadBuffer, givenInOrderQueueAndForcedCpuCopyOnReadBufferAndEventNotReadyWhenReadBufferIsExecutedThenTaskLevelShouldBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(1);
    cl_int retVal = CL_SUCCESS;
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, CompletionStamp::notReady, 4);

    cl_bool blockingRead = CL_FALSE;
    size_t size = sizeof(cl_float);
    cl_event eventWaitList[] = {&event1};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    cl_float mem[4];

    retVal = pCmdQ->enqueueReadBuffer(dstBuffer.get(),
                                      blockingRead,
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
    EXPECT_EQ(CompletionStamp::notReady, pEvent->taskLevel);
    EXPECT_EQ(CompletionStamp::notReady, pCmdQ->taskLevel);
    event1.taskLevel = 20;
    event1.setStatus(CL_COMPLETE);
    pEvent->updateExecutionStatus();
    pCmdQ->isQueueBlocked();
    pEvent->release();
}

TEST_F(EnqueueReadBuffer, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
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
    retVal = pCmdQ->enqueueReadBuffer(srcBuffer.get(),
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
TEST_F(EnqueueReadBuffer, givenOutOfOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
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

    retVal = pCmdOOQ->enqueueReadBuffer(srcBuffer.get(),
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
    auto &csr = pCmdOOQ->getGpgpuCommandStreamReceiver();
    if (csr.peekTimestampPacketWriteEnabled()) {
        auto taskLevel = taskLevelEvent2;
        if (!csr.isUpdateTagFromWaitEnabled()) {
            taskLevel++;
        }
        EXPECT_EQ(taskLevel, pCmdOOQ->taskLevel);
        EXPECT_EQ(taskLevel, pEvent->taskLevel);
    } else {
        EXPECT_EQ(19u, pCmdOOQ->taskLevel);
        EXPECT_EQ(19u, pEvent->taskLevel);
    }

    pEvent->release();
}
