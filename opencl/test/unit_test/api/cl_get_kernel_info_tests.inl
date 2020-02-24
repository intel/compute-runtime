/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/file_io.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetKernelInfoTests;

namespace ULT {

TEST_F(clGetKernelInfoTests, GivenValidParamsWhenGettingKernelInfoThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sources,
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
}
} // namespace ULT
