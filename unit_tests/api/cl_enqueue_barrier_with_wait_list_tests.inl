/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clEnqueueBarrierWithWaitListTests;

TEST_F(clEnqueueBarrierWithWaitListTests, GivenNullCommandQueueWhenEnqueuingBarrierWithWaitListThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueBarrierWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueBarrierWithWaitListTests, GivenValidCommandQueueWhenEnqueuingBarrierWithWaitListThenSuccessIsReturned) {
    auto retVal = clEnqueueBarrierWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
