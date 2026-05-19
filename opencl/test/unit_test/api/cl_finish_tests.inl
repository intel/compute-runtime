/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClFinishTests = ApiTests;

namespace ULT {

TEST_F(ClFinishTests, GivenValidCommandQueueWhenWaitingForFinishThenSuccessIsReturned) {
    retVal = clFinish(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClFinishTests, GivenNullCommandQueueWhenWaitingForFinishThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clFinish(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClFinishTests, WhenClFinishIsCalledThenFinishIsCalledWithResolvePendingL3Flushes) {
    retVal = clFinish(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pCommandQueue->finishCalledWithResolvePendingL3Flushes);
}
} // namespace ULT
