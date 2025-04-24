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

using ClReleaseKernelTests = ApiTests;

namespace ULT {

TEST_F(ClReleaseKernelTests, GivenNullKernelWhenReleasingKernelThenClInvalidKernelErrorIsReturned) {
    retVal = clReleaseKernel(nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(ClReleaseKernelTests, GivenRetainedKernelWhenReleasingKernelThenKernelIsCorrectlyReleased) {
    cl_kernel kernel = nullptr;
    cl_program program = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);

    program = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, program);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(program, 1, &testedClDevice, nullptr, nullptr, nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    kernel = clCreateKernel(program, "CopyBuffer", &retVal);

    EXPECT_NE(nullptr, kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clRetainKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clRetainKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3u, theRef);

    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClReleaseKernelTests, GivenInvalidKernelWhenTerdownWasCalledThenSuccessReturned) {
    wasPlatformTeardownCalled = true;
    auto retVal = clReleaseKernel(nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    wasPlatformTeardownCalled = false;
}

} // namespace ULT
