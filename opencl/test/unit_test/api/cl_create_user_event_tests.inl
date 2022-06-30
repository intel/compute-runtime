/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateUserEventTests;

namespace ULT {

TEST_F(clCreateUserEventTests, GivenValidContextWhenCreatingUserEventThenEventIsCreated) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, userEvent);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, GivenNullContextWhenCreatingUserEventThenInvalidContextErrorIsReturned) {
    auto userEvent = clCreateUserEvent(
        nullptr,
        &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, userEvent);
}

TEST_F(clCreateUserEventTests, GivenCorrectUserEventWhenGetingEventInfoThenClCommandUserCmdTypeIsReturned) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_QUEUE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_command_queue), retSize);
    auto cmdQueue = reinterpret_cast<cl_command_queue>(static_cast<uintptr_t>(0xdeadbeaf));
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_QUEUE, retSize, &cmdQueue, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, cmdQueue);
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_TYPE, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_event_info), retSize);
    auto cmdType = CL_COMMAND_SVM_UNMAP;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_TYPE, retSize, &cmdType, 0);
    EXPECT_EQ(CL_COMMAND_USER, cmdType);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, GivenUserEventStatusSetToCompleteWhenGettingEventInfoThenStatusIsSetToCompleteAndSuccessReturned) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_int), retSize);
    auto status = CL_SUBMITTED;
    retVal = clGetEventInfo(userEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, retSize, &status, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(CL_COMPLETE, status);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, GivenValidUserEventWhenGettingContextThenValidContextAndSuccessIsReturned) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    size_t retSize;
    retVal = clGetEventInfo(userEvent, CL_EVENT_CONTEXT, 0, nullptr, &retSize);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_context), retSize);
    cl_context oclContext;
    retVal = clGetEventInfo(userEvent, CL_EVENT_CONTEXT, retSize, &oclContext, 0);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(oclContext, pContext);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, GivenCompleteUserEventWhenWaitingForUserEventThenReturnIsImmediate) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &userEvent);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateUserEventTests, GivenUserEventWithErrorStatusWhenWaitingForUserEventThenClExecStatusErrorForEventsInWaitListErrorIsReturned) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, -1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clWaitForEvents(1, &userEvent);
    ASSERT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
