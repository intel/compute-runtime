/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/file_io.h"
#include "runtime/context/context.h"
#include "unit_tests/helpers/test_files.h"

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
        num_devices,
        devices,
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
        num_devices,
        devices,
        &binarySize,
        binaries,
        &binaryStatus,
        nullptr);
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
} // namespace ULT
