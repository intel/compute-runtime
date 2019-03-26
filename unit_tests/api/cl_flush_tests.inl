/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clFlushTests;

namespace ULT {

TEST_F(clFlushTests, GivenValidCommandQueueWhenFlushingThenSuccessIsReturned) {
    retVal = clFlush(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clFlushTests, GivenNullCommandQueueWhenFlushingThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clFlush(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace ULT
