/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueBarrierTests;

TEST_F(clEnqueueBarrierTests, GivenNullCommandQueueWhenEnqueuingThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueBarrier(
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueBarrierTests, GivenValidCommandQueueWhenEnqueuingThenSuccessIsReturned) {
    auto retVal = clEnqueueBarrier(
        pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
