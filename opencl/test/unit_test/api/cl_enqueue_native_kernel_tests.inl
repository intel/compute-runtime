/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueNativeKernelTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueNativeKernelTests, GivenAnyParametersWhenExecutingNativeKernelThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueNativeKernel(
        nullptr, // commandQueue
        nullptr, // user_func
        nullptr, // args
        0u,      // cb_args
        0,       // num_mem_objects
        nullptr, // mem_list
        nullptr, // args_mem_loc
        0,       // num_events
        nullptr, // event_list
        nullptr  // event
    );
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}
} // namespace ULT
