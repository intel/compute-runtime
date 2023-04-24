/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

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
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);

    pBinary.reset();

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
