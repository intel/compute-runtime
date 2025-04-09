/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;
using ClGetProgramBuildInfoTests = ApiTests;

namespace ULT {
void verifyDevices(cl_program pProgram, size_t expectedNumDevices, cl_device_id *expectedDevices);

TEST_F(ClGetProgramBuildInfoTests, givenSourceWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
    USE_REAL_FILE_SYSTEM();

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
    retVal = clGetProgramBuildInfo(pProgram, testedClDevice, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);

    retVal = clCompileProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetProgramBuildInfo(pProgram, testedClDevice, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clGetProgramBuildInfo(pProgram, testedClDevice, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);

    // try to get program build info for invalid program object - should fail
    retVal = clGetProgramBuildInfo(nullptr, testedClDevice, CL_PROGRAM_BUILD_STATUS, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenCompilingForSpecificDevicesThenOnlySpecificDevicesAndTheirSubDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    cl_device_id devicesForCompilation[] = {context.getDevice(1), context.getDevice(3)};
    cl_device_id associatedSubDevices[] = {context.getDevice(4), context.getDevice(5)};
    cl_device_id devicesNotForCompilation[] = {context.getDevice(0), context.getDevice(2)};

    retVal = clCompileProgram(
        pProgram,
        2,
        devicesForCompilation,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    for (const auto &device : devicesNotForCompilation) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }
    for (const auto &device : devicesForCompilation) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
    }
    for (const auto &device : associatedSubDevices) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenCompilingWithoutInputDevicesThenAllDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clCompileProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetProgramBuildInfoTests, givenElfBinaryWhenclGetProgramBuildInfoIsCalledThenReturnClBuildNone) {
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
        1,
        &testedClDevice,
        &binarySize,
        &elfBinaryTemp,
        &binaryStatus,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);

    cl_build_status buildStatus;
    retVal = clGetProgramBuildInfo(pProgram, testedClDevice, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetProgramBuildInfoTests, givenInvalidDeviceInputWhenGetProgramBuildInfoIsCalledThenInvalidDeviceErrorIsReturned) {
    cl_build_status buildStatus;
    retVal = clGetProgramBuildInfo(pProgram, nullptr, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    retVal = clGetProgramBuildInfo(pProgram, reinterpret_cast<cl_device_id>(pProgram), CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    MockContext context;
    retVal = clGetProgramBuildInfo(pProgram, context.getDevice(0), CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenLinkingForSpecificDevicesThenOnlySpecificDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

    MockProgram *pProgram = nullptr;
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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clCompileProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    pProgram->setBuildStatus(CL_BUILD_NONE);

    cl_device_id devicesForLinking[] = {context.getDevice(1), context.getDevice(3)};
    cl_program programForLinking = pProgram;

    auto outProgram = clLinkProgram(
        &context,
        2,
        devicesForLinking,
        nullptr,
        1,
        &programForLinking,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, outProgram);

    verifyDevices(outProgram, 2, devicesForLinking);

    for (const auto &device : devicesForLinking) {
        retVal = clGetProgramBuildInfo(outProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(outProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenLinkingWithoutInputDevicesThenAllDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

    MockProgram *pProgram = nullptr;
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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clCompileProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    pProgram->setBuildStatus(CL_BUILD_NONE);

    cl_program programForLinking = pProgram;

    auto outProgram = clLinkProgram(
        &context,
        0,
        nullptr,
        nullptr,
        1,
        &programForLinking,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, outProgram);

    std::vector<cl_device_id> contextDevices;
    context.getDevices().toDeviceIDs(contextDevices);
    verifyDevices(outProgram, contextDevices.size(), contextDevices.data());

    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(outProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseProgram(outProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenBuildingForSpecificDevicesThenOnlySpecificDevicesAndTheirSubDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

    MockProgram *pProgram = nullptr;
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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    cl_program_binary_type binaryType;
    cl_program_binary_type expectedBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    cl_device_id devicesForBuild[] = {context.getDevice(1), context.getDevice(3)};
    cl_device_id associatedSubDevices[] = {context.getDevice(4), context.getDevice(5)};
    cl_device_id devicesNotForBuild[] = {context.getDevice(0), context.getDevice(2)};

    retVal = clBuildProgram(
        pProgram,
        2,
        devicesForBuild,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    for (const auto &device : devicesForBuild) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedBinaryType, binaryType);
    }
    for (const auto &device : associatedSubDevices) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedBinaryType, binaryType);
    }
    for (const auto &device : devicesNotForBuild) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramBuildInfoTest, givenMultiDeviceProgramWhenBuildingWithoutInputDevicesThenAllDevicesReportBuildStatus) {
    USE_REAL_FILE_SYSTEM();

    MockProgram *pProgram = nullptr;
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

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    cl_program_binary_type binaryType;
    cl_program_binary_type expectedBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clBuildProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_SUCCESS, buildStatus);
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BINARY_TYPE, sizeof(binaryType), &binaryType, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(expectedBinaryType, binaryType);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT