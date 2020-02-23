/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetProgramBuildInfoTests;

namespace ULT {

TEST_F(clGetProgramBuildInfoTests, givenSourceWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
    cl_program pProgram = nullptr;
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16");

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    pSource = loadDataFromFile(
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

    cl_build_status buildStatus;
    retVal = clGetProgramBuildInfo(pProgram, devices[testedRootDeviceIndex], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
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

    retVal = clGetProgramBuildInfo(pProgram, devices[testedRootDeviceIndex], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
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

    retVal = clGetProgramBuildInfo(pProgram, devices[testedRootDeviceIndex], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);

    // try to get program build info for invalid program object - should fail
    retVal = clGetProgramBuildInfo(nullptr, devices[testedRootDeviceIndex], CL_PROGRAM_BUILD_STATUS, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetProgramBuildInfoTests, givenElfBinaryWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    const uint8_t data[4] = {};
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions, data);
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfBinary = elfEncoder.encode();

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
    retVal = clGetProgramBuildInfo(pProgram, devices[testedRootDeviceIndex], CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
