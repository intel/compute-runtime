/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueMarkerWithWaitListTests;

TEST_F(clEnqueueMarkerWithWaitListTests, GivenNullCommandQueueWhenEnqueingMarkerWithWaitListThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueMarkerWithWaitListTests, GivenValidCommandQueueWhenEnqueingMarkerWithWaitListThenSuccessIsReturned) {
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueMarkerWithWaitListTests, GivenQueueIncapableWhenEnqueingMarkerWithWaitListThenInvalidOperationIsReturned) {
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MARKER_INTEL);
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}
