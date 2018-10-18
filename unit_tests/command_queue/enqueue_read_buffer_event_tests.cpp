/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "gtest/gtest.h"
#include <memory>

using namespace OCLRT;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueReadBuffer;

TEST_F(EnqueueReadBuffer, eventShouldBeReturned) {
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

TEST_F(EnqueueReadBuffer, eventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
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
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u + 1u, pEvent->taskLevel);

    delete pEvent;
}
TEST_F(EnqueueReadBuffer, givenInOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(true);
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
TEST_F(EnqueueReadBuffer, givenOutOfOrderQueueAndEnabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(true);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
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
TEST_F(EnqueueReadBuffer, givenInOrderQueueAndDisabledSupportCpuCopiesAndDstPtrEqualSrcPtrWithEventsWhenReadBufferIsExecutedThenTaskLevelShouldNotBeIncreased) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(false);
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
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(false);
    std::unique_ptr<CommandQueue> pCmdOOQ(createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE));
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
                                        numEventsInWaitList,
                                        eventWaitList,
                                        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u, pEvent->taskLevel);
    EXPECT_EQ(19u, pCmdOOQ->taskLevel);

    pEvent->release();
}
