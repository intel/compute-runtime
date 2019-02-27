/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCloneKernelTests;

namespace ULT {

TEST_F(clCloneKernelTests, GivenNullKernelWhenCloningKernelThenNullIsReturned) {
    auto kernel = clCloneKernel(nullptr, nullptr);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(clCloneKernelTests, GivenNullKernelWhenCloningKernelThenInvalidKernelErrorIsReturned) {
    clCloneKernel(nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clCloneKernelTests, GivenValidKernelWhenCloningKernelThenSuccessIsReturned) {
    cl_kernel pSourceKernel = nullptr;
    cl_kernel pClonedKernel = nullptr;
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    void *pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

    binarySize = loadDataFromFile(
        testFile.c_str(),
        pBinary);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        (const unsigned char **)&pBinary,
        &binaryStatus,
        &retVal);

    deleteDataReadFromFile(pBinary);

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
