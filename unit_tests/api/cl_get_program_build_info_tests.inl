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
#include "unit_tests/elflib/elf_binary_simulator.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/kernel_binary_helper.h"

using namespace OCLRT;

typedef api_tests clGetProgramBuildInfoTests;

namespace ULT {

TEST_F(clGetProgramBuildInfoTests, givenSourceWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8");

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

    cl_build_status buildStatus;
    retVal = clGetProgramBuildInfo(pProgram, devices[0], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);

    retVal = clCompileProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetProgramBuildInfo(pProgram, devices[0], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetProgramBuildInfo(pProgram, devices[0], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);

    // try to get program build info for invalid program object - should fail
    retVal = clGetProgramBuildInfo(nullptr, devices[0], CL_PROGRAM_BUILD_STATUS, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
    CompilerInterface::shutdown();
}

TEST_F(clGetProgramBuildInfoTests, givenElfBinaryWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;

    CLElfLib::ElfBinaryStorage elfBinary;
    MockElfBinary(elfBinary);

    const size_t binarySize = elfBinary.size();
    const unsigned char *elfBinaryTemp = reinterpret_cast<unsigned char *>(elfBinary.data());
    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        &elfBinaryTemp,
        &binaryStatus,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);

    cl_build_status buildStatus;
    retVal = clGetProgramBuildInfo(pProgram, devices[0], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    CompilerInterface::shutdown();
}
} // namespace ULT
