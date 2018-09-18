/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
