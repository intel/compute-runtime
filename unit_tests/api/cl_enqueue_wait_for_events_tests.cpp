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

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/event/event.h"
#include "runtime/event/user_event.h"

using namespace OCLRT;

using clEnqueueWaitForEventsTests = api_tests;

TEST_F(clEnqueueWaitForEventsTests, GivenInvalidCommandQueueWhenClEnqueueWaitForEventsIsCalledThenReturnError) {
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

TEST_F(clEnqueueWaitForEventsTests, GivenProperParamsWhenClEnqueueWaitForEventsIsCalledAndEventStatusIsCompleteThenWaitAndReturnSuccess) {
    struct MyEvent : public UserEvent {
        MyEvent(Context *context)
            : UserEvent(context) {
        }
        bool wait(bool blocking, bool quickKmdSleep) override {
            wasWaitCalled = true;
            return true;
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

TEST_F(clEnqueueWaitForEventsTests, GivenProperParamsWhenClEnqueueWaitForEventsIsCalledAndEventStatusIsNotCompleteThenReturnError) {
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

TEST_F(clEnqueueWaitForEventsTests, GivenInvalidEventWhenClEnqueueWaitForEventsIsCalledThenReturnError) {
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
