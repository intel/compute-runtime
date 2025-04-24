/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCloneKernelTests = ApiTests;

namespace ULT {

TEST_F(ClCloneKernelTests, GivenNullKernelWhenCloningKernelThenNullIsReturned) {
    auto kernel = clCloneKernel(nullptr, nullptr);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(ClCloneKernelTests, GivenNullKernelWhenCloningKernelThenInvalidKernelErrorIsReturned) {
    clCloneKernel(nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(ClCloneKernelTests, GivenValidKernelWhenCloningKernelThenSuccessIsReturned) {
    cl_kernel pSourceKernel = nullptr;
    cl_kernel pClonedKernel = nullptr;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
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

    pSourceKernel = clCreateKernel(
        pProgram,
        "CopyBuffer",
        &retVal);

    EXPECT_NE(nullptr, pSourceKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pClonedKernel = clCloneKernel(
        pSourceKernel,
        &retVal);

    EXPECT_NE(nullptr, pClonedKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(pClonedKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(pSourceKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
