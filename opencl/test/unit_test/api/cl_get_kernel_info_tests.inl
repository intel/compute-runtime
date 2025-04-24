/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetKernelInfoTests = ApiTests;

namespace ULT {

TEST_F(ClGetKernelInfoTests, GivenValidParamsWhenGettingKernelInfoThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_kernel kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t paramValueSizeRet;
    retVal = clGetKernelInfo(
        kernel,
        CL_KERNEL_FUNCTION_NAME,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValueSizeRet, 0u);

    retVal = clGetKernelInfo(
        kernel,
        CL_KERNEL_ATTRIBUTES,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValueSizeRet, 0u);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
