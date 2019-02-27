/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clGetKernelInfoTests;

namespace ULT {

TEST_F(clGetKernelInfoTests, success) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");

    sourceSize = loadDataFromFile(
        testFile.c_str(),
        pSource);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        (const char **)&pSource,
        &sourceSize,
        &retVal);

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

    cl_kernel kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t paramValueSizeRet;
    retVal = clGetKernelInfo(
        kernel,
        CL_KERNEL_FUNCTION_NAME,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValueSizeRet, 0u);

    retVal = clGetKernelInfo(
        kernel,
        CL_KERNEL_ATTRIBUTES,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValueSizeRet, 0u);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}
} // namespace ULT
