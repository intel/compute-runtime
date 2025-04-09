/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/run_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetProgramInfoTests = ApiTests;

namespace ULT {

static_assert(CL_PROGRAM_IL == CL_PROGRAM_IL_KHR, "Param values are different");

void verifyDevices(cl_program pProgram, size_t expectedNumDevices, cl_device_id *expectedDevices) {
    cl_uint numDevices;
    auto retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedNumDevices, numDevices);

    auto programDevices = std::make_unique<cl_device_id[]>(expectedNumDevices);
    for (auto i = 0u; i < expectedNumDevices; i++) {
        programDevices[i] = nullptr;
    }

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_DEVICES, expectedNumDevices * sizeof(cl_device_id), programDevices.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto i = 0u; i < expectedNumDevices; i++) {
        EXPECT_EQ(expectedDevices[i], programDevices[i]);
    }
}

TEST_F(ClGetProgramInfoTests, GivenSourceWhenBuildingProgramThenGetProgramInfoReturnsCorrectInfo) {
    USE_REAL_FILE_SYSTEM();

    cl_program pProgram = nullptr;
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);

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

    retVal = clBuildProgram(
        pProgram,
        1,
        &testedClDevice,
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
    EXPECT_EQ(testedClDevice, programDevices);

    size_t length = 0;
    char buffer[10240];
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, 0, nullptr, &length);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sourceSize + 1, length);
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, sizeof(buffer), buffer, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(strlen(pSource.get()), strlen(buffer));

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
}

TEST(clGetProgramInfoTest, GivenMultiDeviceProgramCreatedWithSourceWhenGettingDevicesThenCorrectDevicesAreReturned) {
    MockUnrestrictiveContextMultiGPU context;

    auto expectedNumDevices = context.getNumDevices();

    auto devicesForProgram = std::make_unique<cl_device_id[]>(expectedNumDevices);

    for (auto i = 0u; i < expectedNumDevices; i++) {
        devicesForProgram[i] = context.getDevice(i);
    }

    auto pSource = "//";
    size_t sourceSize = 2;
    const char *sources[1] = {pSource};
    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    verifyDevices(pProgram, expectedNumDevices, devicesForProgram.get());

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetProgramInfoTests, GivenIlWhenBuildingProgramThenGetProgramInfoReturnsCorrectInfo) {
    const size_t binarySize = 16;
    const uint32_t spirv[binarySize] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program pProgram = clCreateProgramWithIL(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, pProgram);

    uint32_t output[binarySize] = {};
    size_t outputSize = 0;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_IL, sizeof(output), output, &outputSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(spirv), outputSize);
    EXPECT_EQ(0, memcmp(spirv, output, outputSize));

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramInfoTest, GivenMultiDeviceProgramCreatedWithBinaryWhenGettingDevicesThenCorrectDevicesAreReturned) {
    USE_REAL_FILE_SYSTEM();

    MockUnrestrictiveContextMultiGPU context;

    auto numDevicesForProgram = 2u;
    cl_device_id devicesForProgram[] = {context.getDevice(1), context.getDevice(3)};

    std::unique_ptr<char[]> pBinary0 = nullptr;
    std::unique_ptr<char[]> pBinary1 = nullptr;
    size_t binarySize0 = 0;
    size_t binarySize1 = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary0 = loadDataFromFile(
        testFile.c_str(),
        binarySize0);

    retrieveBinaryKernelFilename(testFile, "copybuffer_", ".bin");

    pBinary1 = loadDataFromFile(
        testFile.c_str(),
        binarySize1);
    ASSERT_NE(0u, binarySize0);
    ASSERT_NE(0u, binarySize1);
    ASSERT_NE(nullptr, pBinary0);
    ASSERT_NE(nullptr, pBinary1);

    EXPECT_NE(binarySize0, binarySize1);

    const unsigned char *binaries[] = {reinterpret_cast<const unsigned char *>(pBinary0.get()), reinterpret_cast<const unsigned char *>(pBinary1.get())};
    size_t sizeBinaries[] = {binarySize0, binarySize1};

    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;
    cl_int binaryStaus = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithBinary(
        &context,
        numDevicesForProgram,
        devicesForProgram,
        sizeBinaries,
        binaries,
        &binaryStaus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    verifyDevices(pProgram, numDevicesForProgram, devicesForProgram);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramInfoTest, GivenMultiDeviceProgramCreatedWithBinaryWhenGettingBinariesThenCorrectBinariesAreReturned) {
    USE_REAL_FILE_SYSTEM();

    MockUnrestrictiveContextMultiGPU context;

    auto numDevicesForProgram = 2u;
    cl_device_id devicesForProgram[] = {context.getDevice(1), context.getDevice(3)};

    std::unique_ptr<char[]> pBinary0 = nullptr;
    std::unique_ptr<char[]> pBinary1 = nullptr;
    size_t binarySize0 = 0;
    size_t binarySize1 = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary0 = loadDataFromFile(
        testFile.c_str(),
        binarySize0);

    retrieveBinaryKernelFilename(testFile, "copybuffer_", ".bin");

    pBinary1 = loadDataFromFile(
        testFile.c_str(),
        binarySize1);
    ASSERT_NE(0u, binarySize0);
    ASSERT_NE(0u, binarySize1);
    ASSERT_NE(nullptr, pBinary0);
    ASSERT_NE(nullptr, pBinary1);

    EXPECT_NE(binarySize0, binarySize1);

    const unsigned char *binaries[] = {reinterpret_cast<const unsigned char *>(pBinary0.get()), reinterpret_cast<const unsigned char *>(pBinary1.get())};
    size_t sizeBinaries[] = {binarySize0, binarySize1};

    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;
    cl_int binaryStaus = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithBinary(
        &context,
        numDevicesForProgram,
        devicesForProgram,
        sizeBinaries,
        binaries,
        &binaryStaus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t programBinarySizes[2] = {};
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_BINARY_SIZES, numDevicesForProgram * sizeof(size_t), programBinarySizes, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto i = 0u; i < numDevicesForProgram; i++) {
        EXPECT_EQ(sizeBinaries[i], programBinarySizes[i]);
    }

    auto programBinary0 = std::make_unique<unsigned char[]>(binarySize0);
    memset(programBinary0.get(), 0, binarySize0);

    auto programBinary1 = std::make_unique<unsigned char[]>(binarySize1);
    memset(programBinary1.get(), 0, binarySize1);

    unsigned char *programBinaries[] = {programBinary0.get(), programBinary1.get()};

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_BINARIES, numDevicesForProgram * sizeof(unsigned char *), programBinaries, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto i = 0u; i < numDevicesForProgram; i++) {
        for (auto j = 0u; j < programBinarySizes[i]; j++) {
            EXPECT_EQ(programBinaries[i][j], binaries[i][j]);
        }
    }

    memset(programBinary1.get(), 0, binarySize1);
    programBinaries[0] = nullptr;

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_BINARIES, numDevicesForProgram * sizeof(unsigned char *), programBinaries, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto j = 0u; j < programBinarySizes[1]; j++) {
        EXPECT_EQ(programBinaries[1][j], binaries[1][j]);
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetProgramInfoTests, GivenSPIRVProgramWhenGettingProgramSourceThenReturnNullString) {
    const size_t binarySize = 16;
    const uint32_t spirv[binarySize] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program pProgram = clCreateProgramWithIL(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, pProgram);

    size_t outputSize = 0;
    uint32_t output[binarySize] = {};
    const char reference[sizeof(output)] = {};

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, 0, nullptr, &outputSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, outputSize);

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_SOURCE, sizeof(output), output, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(output, reference, sizeof(output)));

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramInfoTest, GivenMultiDeviceProgramCreatedWithILWhenGettingDevicesThenCorrectDevicesAreReturned) {
    MockUnrestrictiveContextMultiGPU context;

    auto expectedNumDevices = context.getNumDevices();

    auto devicesForProgram = std::make_unique<cl_device_id[]>(expectedNumDevices);

    for (auto i = 0u; i < expectedNumDevices; i++) {
        devicesForProgram[i] = context.getDevice(i);
    }

    const size_t binarySize = 16;
    const uint32_t spirv[binarySize] = {0x03022307};
    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithIL(
        &context,
        spirv,
        binarySize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    verifyDevices(pProgram, expectedNumDevices, devicesForProgram.get());

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramInfoTest, GivenMultiDeviceBuiltInProgramCreatedWithGenBinaryWhenGettingDevicesThenCorrectDevicesAreReturned) {
    USE_REAL_FILE_SYSTEM();

    MockUnrestrictiveContextMultiGPU context;

    auto expectedNumDevices = context.getNumDevices();

    auto devicesForProgram = std::make_unique<cl_device_id[]>(expectedNumDevices);

    for (auto i = 0u; i < expectedNumDevices; i++) {
        devicesForProgram[i] = context.getDevice(i);
    }

    std::unique_ptr<char[]> pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;
    pProgram = Program::createBuiltInFromGenBinary(&context, context.getDevices(), pBinary.get(), binarySize, &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    verifyDevices(pProgram, expectedNumDevices, devicesForProgram.get());

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clGetProgramInfoTest, GivenMultiDeviceBuiltInProgramCreatedWithGenBinaryWhenGettingDevicesThenCorrectBinariesAreReturned) {
    USE_REAL_FILE_SYSTEM();

    MockUnrestrictiveContextMultiGPU context;

    auto expectedNumDevices = context.getNumDevices();

    auto devicesForProgram = std::make_unique<cl_device_id[]>(expectedNumDevices);

    for (auto i = 0u; i < expectedNumDevices; i++) {
        devicesForProgram[i] = context.getDevice(i);
    }

    std::unique_ptr<char[]> pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    cl_program pProgram = nullptr;

    cl_int retVal = CL_INVALID_PROGRAM;
    pProgram = Program::createBuiltInFromGenBinary(&context, context.getDevices(), pBinary.get(), binarySize, &retVal);

    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto programBinarySizes = std::make_unique<size_t[]>(expectedNumDevices);
    memset(programBinarySizes.get(), 0, expectedNumDevices * sizeof(size_t));

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_BINARY_SIZES, expectedNumDevices * sizeof(size_t), programBinarySizes.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto i = 0u; i < expectedNumDevices; i++) {
        EXPECT_EQ(binarySize, programBinarySizes[i]);
    }

    auto programBinaries = std::make_unique<unsigned char *[]>(expectedNumDevices);

    auto binariesBuffer = std::make_unique<unsigned char[]>(expectedNumDevices * binarySize);
    memset(binariesBuffer.get(), 0, expectedNumDevices * binarySize);

    for (auto i = 0u; i < expectedNumDevices; i++) {
        programBinaries[i] = ptrOffset(binariesBuffer.get(), i * binarySize);
    }

    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_BINARIES, expectedNumDevices * sizeof(unsigned char *), programBinaries.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto i = 0u; i < expectedNumDevices; i++) {
        EXPECT_EQ(0, memcmp(programBinaries[i], pBinary.get(), binarySize));
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT