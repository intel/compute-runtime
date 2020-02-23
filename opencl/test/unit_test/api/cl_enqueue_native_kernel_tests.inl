/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueNativeKernelTests;

namespace ULT {

TEST_F(clEnqueueNativeKernelTests, GivenAnyParametersWhenExecutingNativeKernelThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clEnqueueNativeKernel(
        nullptr, // commandQueue
        nullptr, // user_func
        nullptr, // args
        0u,      // cb_args
        0,       // num_mem_objects
        nullptr, // mem_list
        nullptr, // args_mem_loc
        0,       // num_events
        nullptr, //event_list
        nullptr  // event
    );
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}
} // namespace ULT
