/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/enqueue_fill_buffer_fixture.h"
#include "test.h"

using namespace OCLRT;

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

HWTEST_F(FillBufferEventTests, eventShouldBeReturned) {
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

HWTEST_F(FillBufferEventTests, eventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(nullptr, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 15);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 16);

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
