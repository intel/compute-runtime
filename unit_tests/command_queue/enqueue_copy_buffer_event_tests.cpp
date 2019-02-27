/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace OCLRT;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueCopyBuffer;

TEST_F(EnqueueCopyBuffer, eventShouldBeReturned) {
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    auto retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        srcBuffer->getSize(),
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
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EnqueueCopyBuffer, eventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 4);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 10);

    cl_event eventWaitList[] = {
        &event1,
        &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        srcBuffer->getSize(),
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u + 1u, pEvent->taskLevel);

    delete pEvent;
}
