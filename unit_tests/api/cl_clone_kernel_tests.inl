/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/file_io.h"
#include "runtime/context/context.h"
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

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
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

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
