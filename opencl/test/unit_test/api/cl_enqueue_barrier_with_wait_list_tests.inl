/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueBarrierWithWaitListTests = ApiTests;

TEST_F(ClEnqueueBarrierWithWaitListTests, GivenNullCommandQueueWhenEnqueuingBarrierWithWaitListThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueBarrierWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueBarrierWithWaitListTests, GivenValidCommandQueueWhenEnqueuingBarrierWithWaitListThenSuccessIsReturned) {
    auto retVal = clEnqueueBarrierWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueBarrierWithWaitListTests, GivenQueueIncapableWhenEnqueuingBarrierWithWaitListThenInvalidOperationIsReturned) {
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_BARRIER_INTEL);
    auto retVal = clEnqueueBarrierWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}
