/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

typedef api_tests clEnqueueFillBufferTests;

namespace ULT {

TEST_F(clEnqueueFillBufferTests, GivenNullCommandQueueWhenFillingBufferThenInvalidCommandQueueErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;
    cl_float pattern = 1.0f;

    retVal = clEnqueueFillBuffer(
        nullptr,
        buffer,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueFillBufferTests, GivenNullBufferWhenFillingBufferThenInvalidMemObjectErrorIsReturned) {
    cl_float pattern = 1.0f;

    retVal = clEnqueueFillBuffer(
        pCommandQueue,
        nullptr,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
} // namespace ULT
