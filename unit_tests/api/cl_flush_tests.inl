/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clFlushTests;

namespace ULT {

TEST_F(clFlushTests, returnsSuccess) {
    retVal = clFlush(pCommandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clFlushTests, NullCommandQueue_returnsError) {
    auto retVal = clFlush(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace ULT
