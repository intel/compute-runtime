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
#include "runtime/device/device.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/kernel_binary_helper.h"

using namespace OCLRT;

typedef api_tests clGetProgramInfoTests;

namespace ULT {

TEST_F(clGetProgramInfoTests, SuccessfulProgramWithSource) {
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

    cl_uint numDevices;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);

    cl_device_id programDevices;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_DEVICES, sizeof(programDevices), &programDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(devices[0], programDevices);

    size_t length = 0;
    char buffer[10240];
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, 0, nullptr, &length);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sourceSize + 1, length);
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, sizeof(buffer), buffer, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(strlen(buffer), strlen((const char *)pSource));

    // try to get program info for invalid program object - should fail
    retVal = clGetProgramInfo(nullptr, CL_PROGRAM_SOURCE, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // set paramValueSizeRet to 0 for IL program queries on non-IL programs
    size_t sourceSizeRet = sourceSize;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_IL, 0, nullptr, &sourceSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, sourceSizeRet);

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_IL, sourceSizeRet, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
    CompilerInterface::shutdown();
}

TEST_F(clGetProgramInfoTests, SuccessfulProgramWithIL) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program pProgram = clCreateProgramWithIL(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, pProgram);

    uint32_t output[16] = {};
    size_t outputSize = 0;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_IL, sizeof(output), output, &outputSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(spirv), outputSize);
    EXPECT_EQ(0, memcmp(spirv, output, outputSize));

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
