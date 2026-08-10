/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_event_callbacks_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetEventInfoTests, givenNullEventWhenGetEventInfoThenReturnsCLInvalidEvent) {
    auto retVal = clGetEventInfo(nullptr, CL_EVENT_COMMAND_TYPE, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(RetainReleaseEventTests, givenNullEventWhenRetainEventThenReturnsCLInvalidEvent) {
    auto retVal = clRetainEvent(nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(RetainReleaseEventTests, givenNullEventWhenReleaseEventThenReturnsCLInvalidEvent) {
    auto retVal = clReleaseEvent(nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(SetUserEventStatusTests, givenNullEventWhenSetUserEventStatusThenReturnsCLInvalidEvent) {
    auto retVal = clSetUserEventStatus(nullptr, CL_COMPLETE);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(GetEventProfilingInfoTests, givenNullEventWhenGetEventProfilingInfoThenReturnsCLInvalidEvent) {
    auto retVal = clGetEventProfilingInfo(nullptr, CL_PROFILING_COMMAND_START, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

TEST(SetEventCallbackTests, givenNullFuncNotifyWhenSetEventCallbackThenReturnsCLInvalidValue) {
    auto retVal = clSetEventCallback(nullptr, CL_COMPLETE, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

void CL_CALLBACK dummyEventCallback(cl_event, cl_int, void *) {
}

TEST(SetEventCallbackTests, givenNullEventWhenSetEventCallbackThenReturnsCLInvalidEvent) {
    auto retVal = clSetEventCallback(nullptr, CL_COMPLETE, &dummyEventCallback, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT, retVal);
}

using SetEventCallbackWithContextTests = Test<EventCallbacksFixture>;

TEST_F(SetEventCallbackWithContextTests, givenUnsupportedCallbackTypeWhenSetEventCallbackThenReturnsCLInvalidValue) {
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    EXPECT_EQ(CL_INVALID_VALUE, clSetEventCallback(userEvent, CL_COMPLETE - 1, &dummyEventCallback, nullptr));
    EXPECT_EQ(CL_INVALID_VALUE, clSetEventCallback(userEvent, CL_QUEUED + 1, &dummyEventCallback, nullptr));

    clReleaseEvent(userEvent);
}

TEST_F(SetEventCallbackWithContextTests, givenSupportedCallbackTypesWhenSetEventCallbackThenReturnsSuccess) {
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_QUEUED, &dummyEventCallback, nullptr));
    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_SUBMITTED, &dummyEventCallback, nullptr));
    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_RUNNING, &dummyEventCallback, nullptr));
    EXPECT_EQ(CL_SUCCESS, clSetEventCallback(userEvent, CL_COMPLETE, &dummyEventCallback, nullptr));

    clSetUserEventStatus(userEvent, CL_COMPLETE);
    clReleaseEvent(userEvent);
}

TEST(CreateUserEventTests, givenNullContextWhenCreateUserEventThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    auto event = clCreateUserEvent(nullptr, &errcode);
    EXPECT_EQ(nullptr, event);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(WaitForEventsTests, givenNullEventListWithNonZeroCountWhenWaitForEventsThenReturnsCLInvalidEventWaitList) {
    auto retVal = clWaitForEvents(1, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
