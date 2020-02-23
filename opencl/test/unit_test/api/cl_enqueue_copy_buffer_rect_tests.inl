/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueCopyBufferRectTests;

namespace ULT {

TEST_F(clEnqueueCopyBufferRectTests, GivenCorrectParametersWhenEnqueingCopyBufferRectThenSuccessIsReturned) {
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

TEST_F(clEnqueueCopyBufferRectTests, GivenNullCommandQueueWhenEnqueingCopyBufferRectThenInvalidCommandQueueErrorIsReturned) {
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
