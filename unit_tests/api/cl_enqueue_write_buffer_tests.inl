/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

typedef api_tests clEnqueueWriteBufferTests;

namespace ULT {

TEST_F(clEnqueueWriteBufferTests, nullCommandQueueReturnsError) {
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

TEST_F(clEnqueueWriteBufferTests, nullBufferReturnsError) {
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
