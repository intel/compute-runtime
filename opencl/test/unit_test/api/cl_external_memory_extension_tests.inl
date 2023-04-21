/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClExternalMemoryExtensionTests = ApiTests;

TEST_F(ClExternalMemoryExtensionTests, GivenValidCommandQueueWhenEnqueingAcquireExternalMemObjectsKHRThenSuccessIsReturned) {
    auto retVal = clEnqueueAcquireExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenValidCommandQueueWhenEnqueingReleaseExternalMemObjectsKHRThenSuccessIsReturned) {
    auto retVal = clEnqueueReleaseExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenInvalidValueWhenEnqueingExternalMemObjectsKHRThenInvalidValueErrorIsReturned) {
    auto retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        1,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        0,
        reinterpret_cast<cl_mem *>(pCommandQueue),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenInvalidMemObjectsWhenEnqueingExternalMemObjectsKHRThenInvalidMemObjectErrorIsReturned) {
    auto retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        1,
        reinterpret_cast<cl_mem *>(pCommandQueue),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenValidInputsWhenEnqueingExternalMemObjectsKHRThenSuccessIsReturned) {
    auto retVal = CL_SUCCESS;

    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        16,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        1,
        &buffer,
        1,
        &userEvent,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenNullCommandQueueWhenEnqueingExternalMemObjectsKHRThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueExternalMemObjectsKHR(
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenInvalidEventWaitListWhenEnqueingExternalMemObjectsKHRThenInvalidEventWaitListErrorIsReturned) {
    auto retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        1,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);

    retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        0,
        reinterpret_cast<cl_event *>(pCommandQueue),
        nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);

    retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        1,
        reinterpret_cast<cl_event *>(pCommandQueue),
        nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}

TEST_F(ClExternalMemoryExtensionTests, GivenUserEventWithErrorStatusWhenEnqueingExternalMemObjectsKHRThenExecStatusErrorForEventsInWaitListIsReturned) {
    auto userEvent = clCreateUserEvent(
        pContext,
        &retVal);

    retVal = clSetUserEventStatus(userEvent, -1);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueExternalMemObjectsKHR(
        pCommandQueue,
        0,
        nullptr,
        1,
        &userEvent,
        nullptr);
    ASSERT_EQ(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
