/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueWriteBufferRectTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueWriteBufferRectTests, GivenInvalidBufferWhenWritingRectangularRegionThenInvalidMemObjectErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10] = {};

    auto retVal = clEnqueueWriteBufferRect(
        pCommandQueue,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  // bufferRowPitch
        0,   // bufferSlicePitch
        10,  // hostRowPitch
        0,   // hostSlicePitch
        ptr, // hostPtr
        0,   // numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(ClEnqueueWriteBufferRectTests, GivenNullCommandQueueWhenWritingRectangularRegionThenInvalidCommandQueueErrorIsReturned) {
    auto buffer = (cl_mem)ptrGarbage;
    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 0};
    char ptr[10] = {};

    auto retVal = clEnqueueWriteBufferRect(
        nullptr,
        buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  // bufferRowPitch
        0,   // bufferSlicePitch
        10,  // hostRowPitch
        0,   // hostSlicePitch
        ptr, // hostPtr
        0,   // numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueWriteBufferRectTests, GivenNullHostPtrWhenWritingRectangularRegionThenInvalidValueErrorIsReturned) {
    auto buffer = clCreateBuffer(
        pContext,
        CL_MEM_READ_WRITE,
        100,
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
        10,      // bufferRowPitch
        0,       // bufferSlicePitch
        10,      // hostRowPitch
        0,       // hostSlicePitch
        nullptr, // hostPtr
        0,       // numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueWriteBufferRectTests, GivenCorrectParametersWhenWritingRectangularRegionThenSuccessIsReturned) {
    MockBuffer buffer{};
    buffer.size = 100;
    char ptr[10] = {};

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    auto retVal = clEnqueueWriteBufferRect(
        pCommandQueue,
        &buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  // bufferRowPitch
        0,   // bufferSlicePitch
        10,  // hostRowPitch
        0,   // hostSlicePitch
        ptr, // hostPtr
        0,   // numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueWriteBufferRectTests, GivenQueueIncapableWhenWritingRectangularRegionThenInvalidOperationIsReturned) {
    MockBuffer buffer{};
    buffer.size = 100;
    char ptr[10] = {};

    size_t buffOrigin[] = {0, 0, 0};
    size_t hostOrigin[] = {0, 0, 0};
    size_t region[] = {10, 10, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL);
    auto retVal = clEnqueueWriteBufferRect(
        pCommandQueue,
        &buffer,
        CL_FALSE,
        buffOrigin,
        hostOrigin,
        region,
        10,  // bufferRowPitch
        0,   // bufferSlicePitch
        10,  // hostRowPitch
        0,   // hostSlicePitch
        ptr, // hostPtr
        0,   // numEventsInWaitList
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

} // namespace ULT
