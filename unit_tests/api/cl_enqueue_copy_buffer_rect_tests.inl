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
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/mocks/mock_buffer.h"

using namespace OCLRT;

typedef api_tests clEnqueueCopyBufferRectTests;

namespace ULT {

TEST_F(clEnqueueCopyBufferRectTests, returnSuccess) {
    MockBuffer srcBuffer;
    MockBuffer dstBuffer;
    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};

    auto retVal = clEnqueueCopyBufferRect(
        pCommandQueue,
        &srcBuffer, //srcBuffer
        &dstBuffer, //dstBuffer
        srcOrigin,
        dstOrigin,
        region,
        10, //srcRowPitch
        0,  //srcSlicePitch
        10, //dstRowPitch
        0,  //dstSlicePitch
        0,  //numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueCopyBufferRectTests, NullCommandQueue_returnsError) {
    auto retVal = clEnqueueCopyBufferRect(
        nullptr, //command_queue
        nullptr, //srcBuffer
        nullptr, //dstBuffer
        nullptr, //srcOrigin
        nullptr, //dstOrigin
        nullptr, //retion
        0,       //srcRowPitch
        0,       //srcSlicePitch
        0,       //dstRowPitch
        0,       //dstSlicePitch
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace ULT
