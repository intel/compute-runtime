/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateProgramWithBinaryTests;
typedef api_tests clCreateProgramWithILTests;
typedef api_tests clCreateProgramWithILKHRTests;

namespace ULT {

TEST_F(clCreateProgramWithBinaryTests, GivenCorrectParametersWhenCreatingProgramWithBinaryThenProgramIsCreatedAndSuccessIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    ASSERT_EQ(true, fileExists(testFile));

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};
    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);

    pBinary.reset();

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithBinary(
        nullptr,
        1,
        &testedClDevice,
        &binarySize,
        binaries,
        &binaryStatus,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clCreateProgramWithBinaryTests, GivenInvalidInputWhenCreatingProgramWithBinaryThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    ASSERT_EQ(true, fileExists(testFile));

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const unsigned char *validBinaries[] = {reinterpret_cast<const unsigned char *>(pBinary.get()), reinterpret_cast<const unsigned char *>(pBinary.get())};
    const unsigned char *invalidBinaries[] = {reinterpret_cast<const unsigned char *>(pBinary.get()), nullptr};
    size_t validSizeBinaries[] = {binarySize, binarySize};
    size_t invalidSizeBinaries[] = {binarySize, 0};

    cl_device_id devicesForProgram[] = {testedClDevice, testedClDevice};

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        validSizeBinaries,
        invalidBinaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        invalidSizeBinaries,
        validBinaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        validSizeBinaries,
        validBinaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);

    clReleaseProgram(pProgram);
}

TEST_F(clCreateProgramWithBinaryTests, GivenDeviceNotAssociatedWithContextWhenCreatingProgramWithBinaryThenInvalidDeviceErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd16_", ".bin");

    ASSERT_EQ(true, fileExists(testFile));

    auto pBinary = loadDataFromFile(
        testFile.c_str(),
        binarySize);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(pBinary.get())};

    MockClDevice invalidDevice(new MockDevice());

    cl_device_id devicesForProgram[] = {&invalidDevice};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        devicesForProgram,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    retVal = CL_INVALID_PROGRAM;
    devicesForProgram[0] = nullptr;

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        devicesForProgram,
        &binarySize,
        binaries,
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clCreateProgramWithILTests, GivenInvalidContextWhenCreatingProgramWithIlThenInvalidContextErrorIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(nullptr, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_INVALID_CONTEXT, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenNullIlWhenCreatingProgramWithIlThenInvalidValueErrorIsReturned) {
    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, nullptr, 0, &err);
    EXPECT_EQ(CL_INVALID_VALUE, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlSizeWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, spirv, 0, &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlAndNoErrorPointerWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), nullptr);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILKHRTests, GivenCorrectParametersWhenCreatingProgramWithIlkhrThenProgramIsCreatedAndSuccessIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program program = clCreateProgramWithILKHR(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, program);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithILKHRTests, GivenProgramCreatedWithILWhenBuildAfterBuildIsCalledThenReturnSuccess) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int err = CL_INVALID_VALUE;
    cl_program program = clCreateProgramWithIL(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, program);
    err = clBuildProgram(program, 0, nullptr, "", nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, err);
    err = clBuildProgram(program, 0, nullptr, "", nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, err);
    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithILKHRTests, GivenNullIlWhenCreatingProgramWithIlkhrThenNullProgramIsReturned) {
    cl_program program = clCreateProgramWithILKHR(pContext, nullptr, 0, nullptr);
    EXPECT_EQ(nullptr, program);
}

TEST_F(clCreateProgramWithILKHRTests, GivenBothFunctionVariantsWhenCreatingProgramWithIlThenCommonLogicIsUsed) {
    VariableBackup<ProgramFunctions::CreateFromILFunc> createFromIlBackup{&ProgramFunctions::createFromIL};

    bool createFromIlCalled;
    Context *receivedContext;
    const void *receivedIl;
    size_t receivedLength;
    auto mockFunction = [&](Context *ctx,
                            const void *il,
                            size_t length,
                            cl_int &errcodeRet) -> Program * {
        createFromIlCalled = true;
        receivedContext = ctx;
        receivedIl = il;
        receivedLength = length;
        return nullptr;
    };
    createFromIlBackup = mockFunction;
    const uint32_t spirv[16] = {0x03022307};

    createFromIlCalled = false;
    receivedContext = nullptr;
    receivedIl = nullptr;
    receivedLength = 0;
    clCreateProgramWithIL(pContext, spirv, sizeof(spirv), nullptr);
    EXPECT_TRUE(createFromIlCalled);
    EXPECT_EQ(pContext, receivedContext);
    EXPECT_EQ(&spirv, receivedIl);
    EXPECT_EQ(sizeof(spirv), receivedLength);

    createFromIlCalled = false;
    receivedContext = nullptr;
    receivedIl = nullptr;
    receivedLength = 0;
    clCreateProgramWithILKHR(pContext, spirv, sizeof(spirv), nullptr);
    EXPECT_TRUE(createFromIlCalled);
    EXPECT_EQ(pContext, receivedContext);
    EXPECT_EQ(&spirv, receivedIl);
    EXPECT_EQ(sizeof(spirv), receivedLength);
}

} // namespace ULT
