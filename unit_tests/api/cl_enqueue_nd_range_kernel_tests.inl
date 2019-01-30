/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace OCLRT;

typedef api_tests clEnqueueNDRangeKernelTests;

namespace ULT {

TEST_F(clEnqueueNDRangeKernelTests, GivenValidParametersWhenExecutingKernelThenSuccessIsReturned) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = clEnqueueNDRangeKernel(
        pCommandQueue,
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clEnqueueNDRangeKernelTests, GivenNullCommandQueueWhenExecutingKernelThenInvalidCommandQueueErrorIsReturned) {
    size_t globalWorkSize[3] = {1, 1, 1};

    retVal = clEnqueueNDRangeKernel(
        nullptr,
        pKernel,
        1,
        nullptr,
        globalWorkSize,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueNDRangeKernelTests, GivenNonZeroEventsAndEmptyEventWaitListWhenExecutingKernelThenInvalidEventWaitListErrorIsReturned) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 1;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    retVal = clEnqueueNDRangeKernel(
        pCommandQueue,
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);
}
} // namespace ULT
