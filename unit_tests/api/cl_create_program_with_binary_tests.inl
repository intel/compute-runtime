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

typedef api_tests clCreateProgramWithBinaryTests;
typedef api_tests clCreateProgramWithILTests;
typedef api_tests clCreateProgramWithILKHRTests;

namespace ULT {

TEST_F(clCreateProgramWithBinaryTests, returnsSuccess) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    void *pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

    ASSERT_EQ(true, fileExists(testFile));

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
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);

    deleteDataReadFromFile(pBinary);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithBinary(
        nullptr,
        num_devices,
        devices,
        &binarySize,
        (const unsigned char **)&pBinary,
        &binaryStatus,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clCreateProgramWithILTests, whenContextIsInvalidThenReturnsInvalidValue) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(nullptr, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_INVALID_CONTEXT, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, whenIntermediateRepresentationIsEmptyThenReturnsInvalidValue) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, nullptr, 0, &err);
    EXPECT_EQ(CL_INVALID_VALUE, err);
    EXPECT_EQ(nullptr, prog);

    err = CL_SUCCESS;
    prog = clCreateProgramWithIL(pContext, spirv, 0, &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);

    prog = clCreateProgramWithIL(pContext, spirv, 0, nullptr);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, whenIntermediateRepresentationIsNotSpirvOrLlvmBCThenReturnsInvalidValue) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILKHRTests, whenValidInputParametersThenReturnsSuccessAndProgram) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program program = clCreateProgramWithILKHR(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, program);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithILKHRTests, whenInvalidInputParameterThenReturnsNull) {
    cl_program program = clCreateProgramWithILKHR(pContext, nullptr, 0, nullptr);
    EXPECT_EQ(nullptr, program);
}
} // namespace ULT
