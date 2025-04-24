/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateKernelTests = ApiTests;

namespace ULT {

TEST_F(ClCreateKernelTests, GivenCorrectKernelInProgramWhenCreatingNewKernelThenKernelIsCreatedAndSuccessIsReturned) {
    cl_kernel kernel = nullptr;
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

    kernel = clCreateKernel(
        pProgram,
        "CopyBuffer",
        &retVal);

    EXPECT_NE(nullptr, kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateKernelTests, GivenInvalidKernelNameWhenCreatingNewKernelThenInvalidKernelNameErrorIsReturned) {
    cl_kernel kernel = nullptr;
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

    kernel = clCreateKernel(
        pProgram,
        "WrongName",
        &retVal);

    ASSERT_EQ(nullptr, kernel);
    ASSERT_EQ(CL_INVALID_KERNEL_NAME, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateKernelTests, GivenNullProgramWhenCreatingNewKernelThenInvalidProgramErrorIsReturned) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelTests, GivenNullKernelNameWhenCreatingNewKernelThenInvalidValueErrorIsReturned) {
    cl_kernel kernel = nullptr;
    KernelInfo *pKernelInfo = new KernelInfo();

    std::unique_ptr<MockProgram> pMockProg = std::make_unique<MockProgram>(pContext, false, toClDeviceVector(*pDevice));
    pMockProg->addKernelInfo(pKernelInfo, testedRootDeviceIndex);

    kernel = clCreateKernel(
        pMockProg.get(),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelTests, GivenInvalidProgramWhenCreatingNewKernelThenInvalidProgramErrorIsReturned) {
    cl_kernel kernel = nullptr;

    kernel = clCreateKernel(
        reinterpret_cast<cl_program>(pContext),
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelTests, GivenProgramWithBuildErrorWhenCreatingNewKernelThenInvalidProgramExecutableErrorIsReturned) {
    cl_kernel kernel = nullptr;
    std::unique_ptr<MockProgram> pMockProg = std::make_unique<MockProgram>(pContext, false, toClDeviceVector(*pDevice));
    pMockProg->setBuildStatus(CL_BUILD_ERROR);

    kernel = clCreateKernel(
        pMockProg.get(),
        "",
        &retVal);

    EXPECT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(ClCreateKernelTests, GivenNullPtrForReturnWhenCreatingNewKernelThenKernelIsCreated) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        nullptr);

    ASSERT_EQ(nullptr, kernel);
}
} // namespace ULT
