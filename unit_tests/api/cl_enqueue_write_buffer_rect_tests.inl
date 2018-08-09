/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"

using namespace OCLRT;

typedef api_tests clEnqueueWriteBufferRectTests;

namespace ULT {

TEST_F(clEnqueueWriteBufferRectTests, returnFailure) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10];

    auto retVal = clEnqueueWriteBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueWriteBufferRectTests, NullCommandQueue) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10];

    auto retVal = clEnqueueWriteBufferRect(
        nullptr,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  //bufferRowPitch
        0,   //bufferSlicePitch
        10,  //hostRowPitch
        0,   //hostSlicePitch
        ptr, //hostPtr
        0,   //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueWriteBufferRectTests, NullHostPtr) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueWriteBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,      //bufferRowPitch
        0,       //bufferSlicePitch
        10,      //hostRowPitch
        0,       //hostSlicePitch
        nullptr, //hostPtr
        0,       //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
