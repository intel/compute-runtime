/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"

using namespace OCLRT;

typedef api_tests clEnqueueMarkerWithWaitListTests;

TEST_F(clEnqueueMarkerWithWaitListTests, NullCommandQueue_returnsError) {
    auto retVal = clEnqueueMarkerWithWaitList(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueMarkerWithWaitListTests, returnsSuccess) {
    auto retVal = clEnqueueMarkerWithWaitList(
        pCommandQueue,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
