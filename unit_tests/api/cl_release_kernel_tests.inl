/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clReleaseKernelTests;

namespace ULT {

TEST_F(clReleaseKernelTests, GivenNullKernelWhenReleasingKernelThenClInvalidKernelErrorIsReturned) {
    retVal = clReleaseKernel(nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(clReleaseKernelTests, GivenRetainedKernelWhenReleasingKernelThenKernelIsCorrectlyReleased) {
    cl_kernel kernel = nullptr;
    cl_program program = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    void *binary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

    binarySize = loadDataFromFile(testFile.c_str(), binary);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, binary);

    program = clCreateProgramWithBinary(pContext, num_devices, devices, &binarySize,
                                        (const unsigned char **)&binary, &binaryStatus, &retVal);

    deleteDataReadFromFile(binary);

    EXPECT_NE(nullptr, program);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(program, num_devices, devices, nullptr, nullptr, nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    kernel = clCreateKernel(program, "CopyBuffer", &retVal);

    EXPECT_NE(nullptr, kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clRetainKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clRetainKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3u, theRef);

    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetKernelInfo(kernel, CL_KERNEL_REFERENCE_COUNT, sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);

    retVal = clReleaseKernel(kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
