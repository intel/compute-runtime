/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetKernelArgInfoTests = ApiTests;

namespace ULT {

TEST_F(ClGetKernelArgInfoTests, GivenValidParamsWhenGettingKernelArgInfoThenSuccessAndCorrectSizeAreReturned) {
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
        CompilerOptions::argInfo.data(),
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_kernel kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t returnSize = 0;
    cl_kernel_arg_type_qualifier typeQualifier = CL_KERNEL_ARG_TYPE_NONE;
    retVal = clGetKernelArgInfo(kernel, 0, CL_KERNEL_ARG_TYPE_QUALIFIER,
                                sizeof(typeQualifier), &typeQualifier, &returnSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(returnSize, sizeof(cl_kernel_arg_type_qualifier));

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
