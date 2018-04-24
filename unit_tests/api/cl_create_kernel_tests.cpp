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
#include "unit_tests/mocks/mock_program.h"

using namespace OCLRT;

typedef api_tests clCreateKernelTests;

namespace ULT {

TEST_F(clCreateKernelTests, returnsSuccess) {
    cl_kernel kernel = nullptr;
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

TEST_F(clCreateKernelTests, invalidKernel) {
    cl_kernel kernel = nullptr;
    KernelInfo *pKernelInfo = KernelInfo::create();
    pKernelInfo->isValid = false;
    pKernelInfo->name = "CopyBuffer";

    MockProgram *pMockProg = new MockProgram(pContext, false);
    pMockProg->addKernelInfo(pKernelInfo);

    kernel = clCreateKernel(
        pMockProg,
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_KERNEL, retVal);
    ASSERT_EQ(nullptr, kernel);

    delete pMockProg;
}

TEST_F(clCreateKernelTests, invalidParams) {
    cl_kernel kernel = nullptr;
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

    kernel = clCreateKernel(
        pProgram,
        "WrongName",
        &retVal);

    ASSERT_EQ(nullptr, kernel);
    ASSERT_EQ(CL_INVALID_KERNEL_NAME, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateKernelTests, nullProgram) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, invalidProgram) {
    cl_kernel kernel = nullptr;

    kernel = clCreateKernel(
        (cl_program)pContext,
        "CopyBuffer",
        &retVal);

    ASSERT_EQ(CL_INVALID_PROGRAM, retVal);
    ASSERT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelTests, noRet) {
    cl_kernel kernel = nullptr;
    kernel = clCreateKernel(
        nullptr,
        "CopyBuffer",
        nullptr);

    ASSERT_EQ(nullptr, kernel);
}
} // namespace ULT
