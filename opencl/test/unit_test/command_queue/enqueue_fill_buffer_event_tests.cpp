/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/enqueue_fill_buffer_fixture.h"
#include "test.h"

using namespace NEO;

namespace ULT {

struct FillBufferEventTests : public EnqueueFillBufferFixture,
                              public ::testing::Test {
    typedef EnqueueFillBufferFixture BaseClass;

    void SetUp() override {
        BaseClass::SetUp();
    }

    void TearDown() override {
        BaseClass::TearDown();
    }
};

HWTEST_F(FillBufferEventTests, WhenEnqueingFillBufferThenEventHasCorrectCommandType) {
    float pattern[] = {1.0f};
    size_t patternSize = sizeof(pattern);
    size_t offset = 0;
    size_t size = 2 * patternSize;
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto retVal = pCmdQ->enqueueFillBuffer(
        buffer,
        pattern,
        patternSize,
        offset,
        size,
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_FILL_BUFFER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

HWTEST_F(FillBufferEventTests, GivenMultipleEventsWhenEnqueingFillBufferThenReturnedEventShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 16);

    float pattern[] = {1.0f};
    size_t patternSize = sizeof(pattern);
    size_t offset = 0;
    size_t size = 2 * patternSize;
    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto retVal = pCmdQ->enqueueFillBuffer(
        buffer,
        pattern,
        patternSize,
        offset,
        size,
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u + 1u, pEvent->taskLevel);

    delete pEvent;
}
} // namespace ULT
