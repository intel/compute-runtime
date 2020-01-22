/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetKernelSuggestedLocalWorkSizeTests = api_tests;

namespace ULT {

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenInvalidInputWhenCallingGetKernelSuggestedLocalWorkSizeThenErrorIsReturned) {
    size_t globalWorkOffset[3];
    size_t globalWorkSize[3];
    size_t suggestedLocalWorkSize[3];
    cl_uint workDim = 1;

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(nullptr, pKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, nullptr, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    pKernel->isPatchedOverride = true;

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, 0,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, 4,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, workDim,
                                                    nullptr, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_OFFSET, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, workDim,
                                                    globalWorkOffset, nullptr, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenVariousInputWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pKernel, 1, gws, elws, offset};
    auto expectedLws = computeWorkgroupSize(dispatchInfo);
    EXPECT_GT(expectedLws.x, 1u);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(2);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, 2, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(3);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenKernelWithExecutionEnvironmentPatchedWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
    std::unique_ptr<MockKernel> kernelWithExecutionEnvironmentPatch(MockKernel::create(pCommandQueue->getDevice(), pProgram));

    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};
    cl_uint workDim = 3;

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    const DispatchInfo dispatchInfo{kernelWithExecutionEnvironmentPatch.get(), workDim, gws, elws, offset};
    auto expectedLws = computeWorkgroupSize(dispatchInfo);
    EXPECT_GT(expectedLws.x * expectedLws.y * expectedLws.z, 1u);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, kernelWithExecutionEnvironmentPatch.get(), workDim, globalWorkOffset,
                                                    globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

} // namespace ULT
