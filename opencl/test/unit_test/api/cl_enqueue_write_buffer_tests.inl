/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueWriteBufferTests;

namespace ULT {

TEST_F(clEnqueueWriteBufferTests, GivenCorrectArgumentsWhenWritingBufferThenSuccessIsReturned) {
    MockBuffer buffer{};
    auto data = 1;
    auto retVal = clEnqueueWriteBuffer(
        pCommandQueue,
        &buffer,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueWriteBufferTests, GivenQueueIncapableArgumentsWhenWritingBufferThenInvalidOperationIsReturned) {
    MockBuffer buffer{};
    auto data = 1;

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL);
    auto retVal = clEnqueueWriteBuffer(
        pCommandQueue,
        &buffer,
        false,
        0,
        sizeof(data),
        &data,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(clEnqueueWriteBufferTests, GivenNullCommandQueueWhenWritingBufferThenInvalidCommandQueueErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;

    retVal = clEnqueueWriteBuffer(
        nullptr,
        buffer,
        CL_FALSE, //blocking write
        0,        //offset
        0,        //sb
        nullptr,
        0, //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueWriteBufferTests, GivenNullBufferWhenWritingBufferThenInvalidMemObjectErrorIsReturned) {
    void *ptr = nullptr;

    retVal = clEnqueueWriteBuffer(
        pCommandQueue,
        nullptr,
        CL_FALSE, //blocking write
        0,        //offset
        0,        //cb
        ptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
} // namespace ULT
