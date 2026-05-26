/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/event/event.h"
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
