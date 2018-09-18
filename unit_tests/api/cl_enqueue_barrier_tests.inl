/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clEnqueueBarrierTests;

TEST_F(clEnqueueBarrierTests, NullCommandQueuereturnsError) {
    auto retVal = clEnqueueBarrier(
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueBarrierTests, returnsSuccess) {
    auto retVal = clEnqueueBarrier(
        pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
