/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/context/context.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCompileProgramTests;

namespace ULT {

TEST_F(clCompileProgramTests, GivenKernelAsSingleSourceWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("copybuffer", false);
    testFile.append(clFiles);
    testFile.append("copybuffer.cl");

    auto pSource = loadDataFromFile(
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

TEST_F(clCompileProgramTests, GivenKernelAsSourceWithHeaderWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    cl_program pHeader = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    const char *simpleHeaderName = "simple_header.h";

    testFile.append(clFiles);
    testFile.append("/copybuffer_with_header.cl");

    auto pSource = loadDataFromFile(
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

    testFile.clear();
    testFile.append(clFiles);
    testFile.append("simple_header.h");
    auto pHeaderSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pHeaderSource);

    const char *headerSources[1] = {pHeaderSource.get()};
    pHeader = clCreateProgramWithSource(
        pContext,
        1,
        headerSources,
        &sourceSize,
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
        &simpleHeaderName,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pHeader);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCompileProgramTests, GivenNullProgramWhenCompilingProgramThenInvalidProgramErrorIsReturned) {
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

TEST_F(clCompileProgramTests, GivenInvalidCallbackInputWhenCompileProgramThenInvalidValueErrorIsReturned) {
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
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

TEST_F(clCompileProgramTests, GivenValidCallbackInputWhenLinkProgramThenCallbackIsInvoked) {
    cl_program pProgram = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
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
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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

    auto kernel = clCreateKernel(pProgram, "fullCopy", &retVal);

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
    size_t sourceSize = 0;
    cl_int retVal = CL_INVALID_PROGRAM;
    std::string testFile;

    testFile.append(clFiles);
    testFile.append("copybuffer.cl");
    auto pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        &context,
        1,
        sources,
        &sourceSize,
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
