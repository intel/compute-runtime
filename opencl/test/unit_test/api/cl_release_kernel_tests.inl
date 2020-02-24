/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/helpers/test_files.h"

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
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    auto binary = loadDataFromFile(testFile.c_str(), binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, binary);

    unsigned const char *binaries[1] = {reinterpret_cast<const unsigned char *>(binary.get())};
    program = clCreateProgramWithBinary(pContext, num_devices, devices, &binarySize,
                                        binaries, &binaryStatus, &retVal);

    binary.reset();

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
