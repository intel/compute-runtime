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
#include "compiler_options.h"

using namespace NEO;

typedef api_tests clGetKernelArgInfoTests;

namespace ULT {

TEST_F(clGetKernelArgInfoTests, GivenValidParamsWhenGettingKernelArgInfoThenSuccessAndCorrectSizeAreReturned) {
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
        CompilerOptions::argInfo,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_kernel kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t returnSize = 0;
    cl_kernel_arg_type_qualifier typeQualifier = CL_KERNEL_ARG_TYPE_NONE;
    retVal = clGetKernelArgInfo(kernel, 0, CL_KERNEL_ARG_TYPE_QUALIFIER,
                                sizeof(typeQualifier), &typeQualifier, &returnSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(returnSize, sizeof(cl_kernel_arg_type_qualifier));

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
