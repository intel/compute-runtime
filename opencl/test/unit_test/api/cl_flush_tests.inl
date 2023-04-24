/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClFlushTests = ApiTests;

namespace ULT {

TEST_F(ClFlushTests, GivenValidCommandQueueWhenFlushingThenSuccessIsReturned) {
    retVal = clFlush(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClFlushTests, GivenNullCommandQueueWhenFlushingThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clFlush(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace ULT
