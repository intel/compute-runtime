/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/unit_test/helpers/test_files.h"

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

TEST_F(clCreateProgramWithILTests, GivenInvalidContextWhenCreatingProgramWithIlThenInvalidContextErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(nullptr, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_INVALID_CONTEXT, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenNullIlWhenCreatingProgramWithIlThenInvalidValueErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, nullptr, 0, &err);
    EXPECT_EQ(CL_INVALID_VALUE, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenContextWithDevicesThatDontSupportILWhenCreatingProgramWithILThenErrorIsReturned) {
    cl_int err = CL_SUCCESS;
    const uint32_t spirv[16] = {0x03022307};
    cl_program prog = clCreateProgramWithIL(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, prog);
    clReleaseProgram(prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlSizeWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, spirv, 0, &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILTests, GivenIncorrectIlAndNoErrorPointerWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), nullptr);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(clCreateProgramWithILKHRTests, GivenCorrectParametersWhenCreatingProgramWithIlkhrThenProgramIsCreatedAndSuccessIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program program = clCreateProgramWithILKHR(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, program);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateProgramWithILKHRTests, GivenProgramCreatedWithILWhenBuildAfterBuildIsCalledThenReturnSuccess) {
    REQUIRE_OCL_21_OR_SKIP(pContext);
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
    REQUIRE_OCL_21_OR_SKIP(pContext);
    cl_program program = clCreateProgramWithILKHR(pContext, nullptr, 0, nullptr);
    EXPECT_EQ(nullptr, program);
}
} // namespace ULT
