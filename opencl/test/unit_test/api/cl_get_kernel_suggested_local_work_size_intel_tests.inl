/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetKernelSuggestedLocalWorkSizeTests = api_tests;

namespace ULT {

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenInvalidInputWhenCallingGetKernelSuggestedLocalWorkSizeThenErrorIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];
    cl_uint workDim = 1;

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(nullptr, pMultiDeviceKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, nullptr, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    pKernel->isPatchedOverride = true;

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                    globalWorkOffset, globalWorkSize, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 0,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 4,
                                                    globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, workDim,
                                                    globalWorkOffset, nullptr, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);

    for (size_t i = 0; i < 3; ++i) {
        globalWorkSize[i] = 0;
        retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 3,
                                                        globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
        EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
        globalWorkSize[i] = 1;
    }
}

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenVariousInputWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pDevice, pKernel, 1, gws, elws, offset};
    auto expectedLws = computeWorkgroupSize(dispatchInfo);
    EXPECT_GT(expectedLws.x, 1u);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(2);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 2, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(3);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);

    //null global work offset is fine
    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, pMultiDeviceKernel, 3, nullptr, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenKernelWithReqdWorkGroupSizeWhenGettingSuggestedLocalWorkSizeThenRequiredWorkSizeIsReturned) {
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};
    uint16_t regdLocalWorkSize[] = {32, 32, 32};

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = regdLocalWorkSize[0];
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = regdLocalWorkSize[1];
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = regdLocalWorkSize[2];

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, mockKernel.mockMultiDeviceKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(regdLocalWorkSize[0], suggestedLocalWorkSize[0]);
    EXPECT_EQ(regdLocalWorkSize[1], suggestedLocalWorkSize[1]);
    EXPECT_EQ(regdLocalWorkSize[2], suggestedLocalWorkSize[2]);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeTests, GivenKernelWithExecutionEnvironmentPatchedWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
    auto pKernelWithExecutionEnvironmentPatch = MockKernel::create(pCommandQueue->getDevice(), pProgram);
    auto kernelInfos = MockKernel::toKernelInfoContainer(pKernelWithExecutionEnvironmentPatch->getKernelInfo(), testedRootDeviceIndex);
    MultiDeviceKernel multiDeviceKernelWithExecutionEnvironmentPatch(MockMultiDeviceKernel::toKernelVector(pKernelWithExecutionEnvironmentPatch), kernelInfos);

    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};
    cl_uint workDim = 3;

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    const DispatchInfo dispatchInfo{pDevice, pKernelWithExecutionEnvironmentPatch, workDim, gws, elws, offset};
    auto expectedLws = computeWorkgroupSize(dispatchInfo);
    EXPECT_GT(expectedLws.x * expectedLws.y * expectedLws.z, 1u);

    retVal = clGetKernelSuggestedLocalWorkSizeINTEL(pCommandQueue, &multiDeviceKernelWithExecutionEnvironmentPatch, workDim, globalWorkOffset,
                                                    globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

} // namespace ULT
