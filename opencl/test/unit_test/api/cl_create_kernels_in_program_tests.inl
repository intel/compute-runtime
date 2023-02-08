/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clCreateKernelsInProgramTests : public api_tests {
    void SetUp() override {
        DebugManager.flags.FailBuildProgramWithStatefulAccess.set(0);

        api_tests::SetUp();

        constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
        auto zebinData = std::make_unique<ZebinTestData::ZebinCopyBufferSimdModule<numBits>>(pDevice->getHardwareInfo(), 16);
        const auto &src = zebinData->storage;
        const auto &binarySize = src.size();

        ASSERT_NE(0u, src.size());
        ASSERT_NE(nullptr, src.data());

        auto binaryStatus = CL_SUCCESS;
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(src.data())};
        program = clCreateProgramWithBinary(
            pContext,
            1,
            &testedClDevice,
            &binarySize,
            binaries,
            &binaryStatus,
            &retVal);

        ASSERT_NE(nullptr, program);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clBuildProgram(
            program,
            1,
            &testedClDevice,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        api_tests::TearDown();
    }

    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    std::unique_ptr<char[]> pBinary = nullptr;
    DebugManagerStateRestore restore;
};

TEST_F(clCreateKernelsInProgramTests, GivenValidParametersWhenCreatingKernelObjectsThenKernelsAndSuccessAreReturned) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(clCreateKernelsInProgramTests, GivenNullKernelArgWhenCreatingKernelObjectsThenSuccessIsReturned) {
    cl_uint numKernelsRet = 0;
    retVal = clCreateKernelsInProgram(
        program,
        0,
        nullptr,
        &numKernelsRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numKernelsRet);
}

TEST_F(clCreateKernelsInProgramTests, GivenNullPtrForNumKernelsReturnWhenCreatingKernelObjectsThenSuccessIsReturned) {
    retVal = clCreateKernelsInProgram(
        program,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, kernel);
}

TEST_F(clCreateKernelsInProgramTests, GivenNullProgramWhenCreatingKernelObjectsThenInvalidProgramErrorIsReturn) {
    retVal = clCreateKernelsInProgram(
        nullptr,
        1,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    EXPECT_EQ(nullptr, kernel);
}

TEST_F(clCreateKernelsInProgramTests, GivenTooSmallOutputBufferWhenCreatingKernelObjectsThenInvalidValueErrorIsReturned) {
    retVal = clCreateKernelsInProgram(
        program,
        0,
        &kernel,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, kernel);
}
