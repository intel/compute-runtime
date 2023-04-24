/*
 * Copyright (C) 2018-2023 Intel Corporation
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
