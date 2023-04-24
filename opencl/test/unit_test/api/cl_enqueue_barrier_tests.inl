/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueBarrierTests = ApiTests;

TEST_F(ClEnqueueBarrierTests, GivenNullCommandQueueWhenEnqueuingThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueBarrier(
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueBarrierTests, GivenValidCommandQueueWhenEnqueuingBarrierThenSuccessIsReturned) {
    auto retVal = clEnqueueBarrier(
        pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueBarrierTests, GivenQueueIncapableWhenEnqueuingBarrierThenInvalidOperationIsReturned) {
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_BARRIER_INTEL);
    auto retVal = clEnqueueBarrier(
        pCommandQueue);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}
