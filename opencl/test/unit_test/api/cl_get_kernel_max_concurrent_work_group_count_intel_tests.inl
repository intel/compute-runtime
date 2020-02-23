/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetKernelMaxConcurrentWorkGroupCountTests = api_tests;

namespace ULT {

TEST_F(clGetKernelMaxConcurrentWorkGroupCountTests, GivenInvalidInputWhenCallingGetKernelMaxConcurrentWorkGroupCountThenErrorIsReturned) {
    size_t globalWorkOffset[3];
    size_t localWorkSize[3];
    size_t suggestedWorkGroupCount;
    cl_uint workDim = 1;
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(nullptr, pKernel, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, nullptr, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    pKernel->isPatchedOverride = true;

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, workDim,
                                                         globalWorkOffset, localWorkSize, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, 0,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, 4,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, workDim,
                                                         nullptr, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_GLOBAL_OFFSET, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, workDim,
                                                         globalWorkOffset, nullptr, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

TEST_F(clGetKernelMaxConcurrentWorkGroupCountTests, GivenVariousInputWhenGettingMaxConcurrentWorkGroupCountThenCorrectValuesAreReturned) {
    cl_uint workDim = 3;
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t localWorkSize[] = {8, 8, 8};
    size_t maxConcurrentWorkGroupCount = 0;
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernel, workDim, globalWorkOffset, localWorkSize,
                                                         &maxConcurrentWorkGroupCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t expectedMaxConcurrentWorkGroupCount = pKernel->getMaxWorkGroupCount(workDim, localWorkSize);
    EXPECT_EQ(expectedMaxConcurrentWorkGroupCount, maxConcurrentWorkGroupCount);

    std::unique_ptr<MockKernel> pKernelWithExecutionEnvironmentPatch(MockKernel::create(pCommandQueue->getDevice(), pProgram));
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pKernelWithExecutionEnvironmentPatch.get(), workDim,
                                                         globalWorkOffset, localWorkSize,
                                                         &maxConcurrentWorkGroupCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    expectedMaxConcurrentWorkGroupCount = pKernelWithExecutionEnvironmentPatch->getMaxWorkGroupCount(workDim, localWorkSize);
    EXPECT_EQ(expectedMaxConcurrentWorkGroupCount, maxConcurrentWorkGroupCount);
}

} // namespace ULT
