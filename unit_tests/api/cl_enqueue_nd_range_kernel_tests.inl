/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

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

TEST_F(clEnqueueNDRangeKernelTests, GivenConcurrentKernelWhenExecutingKernelThenInvalidKernelErrorIsReturned) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    pKernel->executionType = KernelExecutionType::Concurrent;

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

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}
TEST_F(clEnqueueNDRangeKernelTests, GivenKernelWithAllocateSyncBufferPatchWhenExecutingKernelThenInvalidKernelErrorIsReturned) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    SPatchAllocateSyncBuffer patchAllocateSyncBuffer;
    pProgram->mockKernelInfo.patchInfo.pAllocateSyncBuffer = &patchAllocateSyncBuffer;

    EXPECT_TRUE(pKernel->isUsingSyncBuffer());

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

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}
} // namespace ULT
