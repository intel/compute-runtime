/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clFinishTests;

namespace ULT {

TEST_F(clFinishTests, GivenValidCommandQueueWhenWaitingForFinishThenSuccessIsReturned) {
    retVal = clFinish(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clFinishTests, GivenNullCommandQueueWhenWaitingForFinishThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clFinish(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace ULT
