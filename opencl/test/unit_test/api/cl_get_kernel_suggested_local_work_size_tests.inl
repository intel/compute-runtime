/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/api/api.h"
#include "opencl/source/api/dispatch.h"
#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "cl_api_tests.h"

#include <limits>
#include <utility>

using namespace NEO;

using clGetKernelSuggestedLocalWorkSizeCoreTests = ApiTests;

namespace ULT {

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenIcdDispatchTableWhenCheckingFunctionPointerThenCoreFunctionIsRegistered) {
    auto icdStoredFunction = icdGlobalDispatchTable.clGetKernelSuggestedLocalWorkSize;
    auto pFunction = &clGetKernelSuggestedLocalWorkSize;

    EXPECT_EQ(icdStoredFunction, pFunction);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenInvalidInputWhenCallingGetKernelSuggestedLocalWorkSizeThenErrorIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];
    cl_uint workDim = 1;

    retVal = clGetKernelSuggestedLocalWorkSize(nullptr, pMultiDeviceKernel, workDim,
                                               globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, nullptr, workDim,
                                               globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);

    pKernel->isPatchedOverride = false;
    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, workDim,
                                               globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, retVal);
    pKernel->isPatchedOverride = true;

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, workDim,
                                               globalWorkOffset, globalWorkSize, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 0,
                                               globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 4,
                                               globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, workDim,
                                               globalWorkOffset, nullptr, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);

    for (size_t i = 0; i < 3; ++i) {
        globalWorkSize[i] = 0;
        retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 3,
                                                   globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
        EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
        globalWorkSize[i] = 1;
    }
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenKernelFromDifferentContextWhenGettingSuggestedLocalWorkSizeThenInvalidContextIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];
    cl_int kernelContextRetVal = CL_SUCCESS;

    auto kernelContext = Context::create<MockContext>(nullptr, ClDeviceVector(&testedClDevice, 1), nullptr, nullptr, kernelContextRetVal);
    ASSERT_EQ(CL_SUCCESS, kernelContextRetVal);
    ASSERT_NE(nullptr, kernelContext);

    {
        MockKernelWithInternals kernelFromDifferentContext(*kernelContext);
        retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, kernelFromDifferentContext.mockMultiDeviceKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
        EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    }

    kernelContext->release();
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenKernelWithoutExecutableForQueueDeviceWhenGettingSuggestedLocalWorkSizeThenInvalidProgramExecutableIsReturned) {
    auto kernelForRootDeviceZero = MockKernel::create(pCommandQueue->getDevice(), pProgram);
    KernelVectorType kernels;
    kernels.resize(testedRootDeviceIndex + 1);
    kernels[0] = kernelForRootDeviceZero;
    auto kernelInfos = MockKernel::toKernelInfoContainer(kernelForRootDeviceZero->getKernelInfo(), 0u);
    MockMultiDeviceKernel kernelMissingQueueDeviceExecutable(std::move(kernels), kernelInfos);

    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, &kernelMissingQueueDeviceExecutable, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenWorkDimGreaterThanDeviceLimitWhenGettingSuggestedLocalWorkSizeThenInvalidWorkDimensionIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];
    auto originalMaxWorkItemDimensions = pDevice->deviceInfo.maxWorkItemDimensions;
    pDevice->deviceInfo.maxWorkItemDimensions = 2u;

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    pDevice->deviceInfo.maxWorkItemDimensions = originalMaxWorkItemDimensions;
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenGlobalWorkSizeExceedingDeviceSizeTWhenGettingSuggestedLocalWorkSizeThenInvalidGlobalWorkSizeIsReturned) {
    size_t globalWorkOffset[3] = {};
    size_t globalWorkSize[3] = {2, 1, 1};
    size_t suggestedLocalWorkSize[3];
    auto &rootDeviceInfo = static_cast<MockDevice *>(&pDevice->getDevice())->deviceInfo;
    auto originalAddressBits = rootDeviceInfo.addressBits;
    rootDeviceInfo.addressBits = 1u;

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);

    rootDeviceInfo.addressBits = originalAddressBits;
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenGlobalWorkOffsetPlusSizeOverflowWhenGettingSuggestedLocalWorkSizeThenInvalidGlobalOffsetIsReturned) {
    size_t globalWorkOffset[3] = {std::numeric_limits<size_t>::max(), 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3];

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_GLOBAL_OFFSET, retVal);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenVariousInputWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pDevice, pKernel, 1, gws, elws, offset};
    auto expectedLws = computeWorkgroupSize(dispatchInfo);
    EXPECT_GT(expectedLws.x, 1u);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(2);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 2, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(0u, suggestedLocalWorkSize[2]);

    dispatchInfo.setDim(3);
    expectedLws = computeWorkgroupSize(dispatchInfo);
    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, pMultiDeviceKernel, 3, nullptr, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenKernelWithReqdWorkGroupSizeWhenGettingSuggestedLocalWorkSizeThenRequiredWorkSizeIsReturned) {
    size_t globalWorkOffset[] = {0, 0, 0};
    size_t globalWorkSize[] = {128, 128, 128};
    size_t suggestedLocalWorkSize[] = {0, 0, 0};
    uint16_t regdLocalWorkSize[] = {32, 32, 32};

    MockKernelWithInternals mockKernel(*pContext);
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = regdLocalWorkSize[0];
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = regdLocalWorkSize[1];
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = regdLocalWorkSize[2];

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, mockKernel.mockMultiDeviceKernel, 3, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(regdLocalWorkSize[0], suggestedLocalWorkSize[0]);
    EXPECT_EQ(regdLocalWorkSize[1], suggestedLocalWorkSize[1]);
    EXPECT_EQ(regdLocalWorkSize[2], suggestedLocalWorkSize[2]);
}

TEST_F(clGetKernelSuggestedLocalWorkSizeCoreTests, GivenKernelWithExecutionEnvironmentPatchedWhenGettingSuggestedLocalWorkSizeThenCorrectValuesAreReturned) {
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

    retVal = clGetKernelSuggestedLocalWorkSize(pCommandQueue, &multiDeviceKernelWithExecutionEnvironmentPatch, workDim, globalWorkOffset,
                                               globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedLws.x, suggestedLocalWorkSize[0]);
    EXPECT_EQ(expectedLws.y, suggestedLocalWorkSize[1]);
    EXPECT_EQ(expectedLws.z, suggestedLocalWorkSize[2]);
}

} // namespace ULT
