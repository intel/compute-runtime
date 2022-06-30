/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

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

TEST_F(clEnqueueFillBufferTests, GivenValidArgumentsWhenFillingBufferThenSuccessIsReturned) {
    MockBuffer buffer{};
    cl_float pattern = 1.0f;

    retVal = clEnqueueFillBuffer(
        pCommandQueue,
        &buffer,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueFillBufferTests, GivenQueueIncapableWhenFillingBufferThenInvalidOperationIsReturned) {
    MockBuffer buffer{};
    cl_float pattern = 1.0f;

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL);
    retVal = clEnqueueFillBuffer(
        pCommandQueue,
        &buffer,
        &pattern,
        sizeof(pattern),
        0,
        sizeof(pattern),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
