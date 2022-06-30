/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/grf_config.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetKernelMaxConcurrentWorkGroupCountTests = api_tests;

namespace ULT {

TEST_F(clGetKernelMaxConcurrentWorkGroupCountTests, GivenInvalidInputWhenCallingGetKernelMaxConcurrentWorkGroupCountThenErrorIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t localWorkSize[3] = {};
    size_t suggestedWorkGroupCount;
    cl_uint workDim = 1;
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(nullptr, pMultiDeviceKernel, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, nullptr, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    pKernel->isPatchedOverride = true;

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                         globalWorkOffset, localWorkSize, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, 0,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, 4,
                                                         globalWorkOffset, localWorkSize, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                         globalWorkOffset, nullptr, &suggestedWorkGroupCount);
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

TEST_F(clGetKernelMaxConcurrentWorkGroupCountTests, GivenVariousInputWhenGettingMaxConcurrentWorkGroupCountThenCorrectValuesAreReturned) {
    cl_uint workDim = 3;
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t localWorkSize[] = {8, 8, 8};
    size_t maxConcurrentWorkGroupCount = 0;
    const_cast<KernelInfo &>(pKernel->getKernelInfo()).kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, workDim, globalWorkOffset, localWorkSize,
                                                         &maxConcurrentWorkGroupCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    size_t expectedMaxConcurrentWorkGroupCount = pKernel->getMaxWorkGroupCount(workDim, localWorkSize, pCommandQueue);
    EXPECT_EQ(expectedMaxConcurrentWorkGroupCount, maxConcurrentWorkGroupCount);

    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, pMultiDeviceKernel, workDim, nullptr, localWorkSize,
                                                         &maxConcurrentWorkGroupCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedMaxConcurrentWorkGroupCount, maxConcurrentWorkGroupCount);

    auto pKernelWithExecutionEnvironmentPatch = MockKernel::create(pCommandQueue->getDevice(), pProgram);
    auto kernelInfos = MockKernel::toKernelInfoContainer(pKernelWithExecutionEnvironmentPatch->getKernelInfo(), testedRootDeviceIndex);
    MultiDeviceKernel multiDeviceKernelWithExecutionEnvironmentPatch(MockMultiDeviceKernel::toKernelVector(pKernelWithExecutionEnvironmentPatch), kernelInfos);
    retVal = clGetKernelMaxConcurrentWorkGroupCountINTEL(pCommandQueue, &multiDeviceKernelWithExecutionEnvironmentPatch, workDim,
                                                         globalWorkOffset, localWorkSize,
                                                         &maxConcurrentWorkGroupCount);
    EXPECT_EQ(CL_SUCCESS, retVal);
    expectedMaxConcurrentWorkGroupCount = pKernelWithExecutionEnvironmentPatch->getMaxWorkGroupCount(workDim, localWorkSize, pCommandQueue);
    EXPECT_EQ(expectedMaxConcurrentWorkGroupCount, maxConcurrentWorkGroupCount);
}

} // namespace ULT
