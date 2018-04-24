/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/helpers/test_files.h"

using namespace OCLRT;

typedef api_tests clCloneKernelTests;

namespace ULT {

TEST_F(clCloneKernelTests, returnsNullKernel) {
    auto kernel = clCloneKernel(nullptr, nullptr);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(clCloneKernelTests, returnsInvalidKernel) {
    clCloneKernel(nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clCloneKernelTests, returnsSuccess) {
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
