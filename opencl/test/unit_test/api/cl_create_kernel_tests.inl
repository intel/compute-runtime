/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

#include "opencl/source/context/context.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/test/unit_test/helpers/test_files.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateKernelTests;

namespace ULT {

TEST_F(clCreateKernelTests, GivenCorrectKernelInProgramWhenCreatingNewKernelThenKernelIsCreatedAndSuccessIsReturned) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    pBinary.reset();

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
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

TEST_F(clCreateKernelTests, GivenInvalidKernelNameWhenCreatingNewKernelThenInvalidKernelNameErrorIsReturned) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    pBinary.reset();

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
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

TEST_F(clCreateKernelTests, GivenNullProgramWhenCreatingNewKernelThenInvalidProgramErrorIsReturned) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, GivenNullKernelNameWhenCreatingNewKernelThenInvalidValueErrorIsReturned) {
    cl_kernel kernel = nullptr;
    KernelInfo *pKernelInfo = new KernelInfo();

    std::unique_ptr<MockProgram> pMockProg = std::make_unique<MockProgram>(*pPlatform->peekExecutionEnvironment(), pContext, false, nullptr);
    pMockProg->addKernelInfo(pKernelInfo);

    kernel = clCreateKernel(
        pMockProg.get(),
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, GivenInvalidProgramWhenCreatingNewKernelThenInvalidProgramErrorIsReturned) {
    cl_kernel kernel = nullptr;

    kernel = clCreateKernel(
        reinterpret_cast<cl_program>(pContext),
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, GivenProgramWithBuildErrorWhenCreatingNewKernelThenInvalidProgramExecutableErrorIsReturned) {
    cl_kernel kernel = nullptr;
    std::unique_ptr<MockProgram> pMockProg = std::make_unique<MockProgram>(*pPlatform->peekExecutionEnvironment(), pContext, false, nullptr);
    pMockProg->SetBuildStatus(CL_BUILD_ERROR);

    kernel = clCreateKernel(
        pMockProg.get(),
        "",
        &retVal);

    EXPECT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, GivenNullPtrForReturnWhenCreatingNewKernelThenKernelIsCreated) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        nullptr);

    ASSERT_EQ(nullptr, kernel);
}
} // namespace ULT
