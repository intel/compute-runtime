/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateProgramWithBinaryTests = ApiTests;
using ClCreateProgramWithILTests = ApiTests;
using ClCreateProgramWithILKHRTests = ApiTests;

namespace ULT {

TEST_F(ClCreateProgramWithBinaryTests, GivenCorrectParametersWhenCreatingProgramWithBinaryThenProgramIsCreatedAndSuccessIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, binaryStatus);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithBinary(
        nullptr,
        1,
        &testedClDevice,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(ClCreateProgramWithBinaryTests, GivenInvalidInputWhenCreatingProgramWithBinaryThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    MockZebinWrapper<2> zebin(pDevice->getHardwareInfo(), 32);

    zebin.binaries[1] = nullptr;

    cl_device_id devicesForProgram[] = {testedClDevice, testedClDevice};

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    retVal = CL_INVALID_PROGRAM;

    zebin.binaries[1] = zebin.binaries[0];
    zebin.binarySizes[1] = 0;

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, pProgram);

    zebin.binarySizes[1] = zebin.data.storage.size();

    pProgram = clCreateProgramWithBinary(
        pContext,
        2,
        devicesForProgram,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, pProgram);

    clReleaseProgram(pProgram);
}

TEST_F(ClCreateProgramWithBinaryTests, GivenDeviceNotAssociatedWithContextWhenCreatingProgramWithBinaryThenInvalidDeviceErrorIsReturned) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_INVALID_VALUE;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    MockClDevice invalidDevice(new MockDevice());

    cl_device_id devicesForProgram[] = {&invalidDevice};

    pProgram = clCreateProgramWithBinary(
        pContext,
        1,
        devicesForProgram,
        zebin.binarySizes.data(),
        zebin.binaries.data(),
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
        zebin.binarySizes.data(),
        zebin.binaries.data(),
        &binaryStatus,
        &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(ClCreateProgramWithILTests, GivenInvalidContextWhenCreatingProgramWithIlThenInvalidContextErrorIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(nullptr, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_INVALID_CONTEXT, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILTests, GivenNullIlWhenCreatingProgramWithIlThenInvalidValueErrorIsReturned) {
    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, nullptr, 0, &err);
    EXPECT_EQ(CL_INVALID_VALUE, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILTests, GivenIncorrectIlSizeWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, spirv, 0, &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILTests, GivenIncorrectIlWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_INVALID_BINARY, err);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILTests, GivenIncorrectIlAndNoErrorPointerWhenCreatingProgramWithIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), nullptr);
    EXPECT_EQ(nullptr, prog);
}

TEST_F(ClCreateProgramWithILKHRTests, GivenCorrectParametersWhenCreatingProgramWithIlkhrThenProgramIsCreatedAndSuccessIsReturned) {
    const uint32_t spirv[16] = {0x03022307};

    cl_int err = CL_INVALID_VALUE;
    cl_program program = clCreateProgramWithILKHR(pContext, spirv, sizeof(spirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    EXPECT_NE(nullptr, program);

    retVal = clReleaseProgram(program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateProgramWithILKHRTests, GivenProgramCreatedWithILWhenBuildAfterBuildIsCalledThenReturnSuccess) {
    USE_REAL_FILE_SYSTEM();

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

TEST_F(ClCreateProgramWithILKHRTests, GivenNullIlWhenCreatingProgramWithIlkhrThenNullProgramIsReturned) {
    cl_program program = clCreateProgramWithILKHR(pContext, nullptr, 0, nullptr);
    EXPECT_EQ(nullptr, program);
}

TEST_F(ClCreateProgramWithILKHRTests, GivenBothFunctionVariantsWhenCreatingProgramWithIlThenCommonLogicIsUsed) {
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