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

struct clCreateKernelsInProgramTests : public api_tests {
    void SetUp() override {
        api_tests::SetUp();
        std::string testFile;
        retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

        auto binarySize = loadDataFromFile(
            testFile.c_str(),
            pBinary);

        ASSERT_NE(0u, binarySize);
        ASSERT_NE(nullptr, pBinary);

        auto binaryStatus = CL_SUCCESS;
        program = clCreateProgramWithBinary(
            pContext,
            num_devices,
            devices,
            &binarySize,
            (const unsigned char **)&pBinary,
            &binaryStatus,
            &retVal);

        deleteDataReadFromFile(pBinary);
        ASSERT_NE(nullptr, program);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clBuildProgram(
            program,
            num_devices,
            devices,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        api_tests::TearDown();
    }

    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    void *pBinary = nullptr;
};

TEST_F(clCreateKernelsInProgramTests, normalPathReturnsSuccess) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(clCreateKernelsInProgramTests, nullKernelArgReturnsSuccess) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        0,
        nullptr,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
}

TEST_F(clCreateKernelsInProgramTests, nullNumKernelsRetReturnsSuccess) {
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(clCreateKernelsInProgramTests, nullProgram) {
    retVal = clCreateKernelsInProgram(
        nullptr,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    EXPECT_EQ(nullptr, kernel);
}
