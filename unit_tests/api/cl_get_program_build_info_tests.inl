/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
}
} // namespace ULT
