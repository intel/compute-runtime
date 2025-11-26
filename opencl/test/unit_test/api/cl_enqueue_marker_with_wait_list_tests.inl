/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueMarkerWithWaitListTests = ApiTests;

TEST_F(ClEnqueueMarkerWithWaitListTests, GivenNullCommandQueueWhenEnqueingMarkerWithWaitListThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueMarkerWithWaitListTests, GivenValidCommandQueueWhenEnqueingMarkerWithWaitListThenSuccessIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueMarkerWithWaitListTests, GivenQueueIncapableWhenEnqueingMarkerWithWaitListThenInvalidOperationIsReturned) {
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MARKER_INTEL);
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(ClEnqueueMarkerWithWaitListTests, GivenEventFromDifferentContextWhenEnqueuingMarkerWithWaitListThenInvalidContextErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    cl_device_id deviceId = pDevice;

    auto context = clCreateContext(nullptr, 1, &deviceId, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, context);

    auto event = clCreateUserEvent(context, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        1,
        &event,
        nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    clReleaseEvent(event);
    clReleaseContext(context);
}
