/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCompileProgramTests = ApiTests;

namespace ULT {

TEST_F(ClCompileProgramTests, GivenKernelAsSingleSourceWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

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

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCompileProgramTests, GivenKernelAsSourceWithHeaderWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    cl_program pHeader = nullptr;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    auto copyBufferWithHeader = R"===(
#include "simple_header.h"
__kernel void CopyBuffer(){}
            )===";

    auto copyBufferWithHeaderSize = sizeof(copyBufferWithHeader);

    auto header = R"===(
extenr __kernel void AddBuffer();
            )===";

    auto headerSize = sizeof(header);
    auto headerName = "simple_header.h";
    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        &copyBufferWithHeader,
        &copyBufferWithHeaderSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pHeader = clCreateProgramWithSource(
        pContext,
        1,
        &header,
        &headerSize,
        &retVal);

    EXPECT_NE(nullptr, pHeader);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        1,
        &pHeader,
        &headerName,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pHeader);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCompileProgramTests, GivenNullProgramWhenCompilingProgramThenInvalidProgramErrorIsReturned) {
    retVal = clCompileProgram(
        nullptr,
        1,
        nullptr,
        "",
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(ClCompileProgramTests, GivenInvalidCallbackInputWhenCompileProgramThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCompileProgramTests, GivenValidCallbackInputWhenLinkProgramThenCallbackIsInvoked) {
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(pDevice->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        sampleKernelSrcs,
        &sampleKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    char userData = 0;

    retVal = clCompileProgram(
        pProgram,
        1,
        &testedClDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        notifyFuncProgram,
        &userData);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ('a', userData);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clCompileProgramTest, givenProgramWhenCompilingForInvalidDevicesInputThenInvalidDeviceErrorIsReturned) {
    MockUnrestrictiveContextMultiGPU context;
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(context.getDevice(0)->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();

    const char *sourceKernel = "example_kernel(){}";
    size_t sourceKernelSize = std::strlen(sourceKernel) + 1;
    const char *sources[1] = {sourceKernel};

    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    MockContext mockContext;
    cl_device_id nullDeviceInput[] = {context.getDevice(0), nullptr};
    cl_device_id notAssociatedDeviceInput[] = {mockContext.getDevice(0)};
    cl_device_id validDeviceInput[] = {context.getDevice(0)};

    retVal = clCompileProgram(
        pProgram,
        0,
        validDeviceInput,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = clCompileProgram(
        pProgram,
        2,
        nullDeviceInput,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        notAssociatedDeviceInput,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clCompileProgramTest, givenMultiDeviceProgramWithCreatedKernelWhenCompilingThenInvalidOperationErrorIsReturned) {
    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(context.getDevice(0)->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();
    cl_int retVal = CL_INVALID_PROGRAM;

    const char *sourceKernel = "example_kernel(){}";
    size_t sourceKernelSize = std::strlen(sourceKernel) + 1;
    const char *sources[1] = {sourceKernel};

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstSubDevice = context.pSubDevice0;
    cl_device_id secondSubDevice = context.pSubDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(clCompileProgramTest, givenMultiDeviceProgramWithCreatedKernelsWhenCompilingThenInvalidOperationErrorIsReturned) {
    MockSpecializedContext context;
    cl_program pProgram = nullptr;
    MockZebinWrapper zebin(context.getDevice(0)->getHardwareInfo(), 32);
    zebin.setAsMockCompilerReturnedBinary();
    cl_int retVal = CL_INVALID_PROGRAM;

    const char *sourceKernel = "example_kernel(){}";
    size_t sourceKernelSize = std::strlen(sourceKernel) + 1;
    const char *sources[1] = {sourceKernel};

    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceKernelSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_device_id firstSubDevice = context.pSubDevice0;
    cl_device_id secondSubDevice = context.pSubDevice1;

    retVal = clBuildProgram(
        pProgram,
        1,
        &firstSubDevice,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t numKernels = 0;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_KERNELS, sizeof(numKernels), &numKernels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto kernels = std::make_unique<cl_kernel[]>(numKernels);

    retVal = clCreateKernelsInProgram(pProgram, static_cast<cl_uint>(numKernels), kernels.get(), nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    for (auto i = 0u; i < numKernels; i++) {
        retVal = clReleaseKernel(kernels[i]);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    retVal = clCompileProgram(
        pProgram,
        1,
        &secondSubDevice,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT