/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "runtime/program/program.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_compilers.h"

using namespace OCLRT;

typedef api_tests clBuildProgramTests;

namespace ULT {

TEST_F(clBuildProgramTests, GivenSourceAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");

    sourceSize = loadDataFromFile(
        testFile.c_str(),
        pSource);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    pProgram = clCreateProgramWithSource(
        pContext,
        1,
        (const char **)&pSource,
        &sourceSize,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
    CompilerInterface::shutdown();

    pProgram = clCreateProgramWithSource(
        nullptr,
        1,
        (const char **)&pSource,
        &sourceSize,
        nullptr);
    EXPECT_EQ(nullptr, pProgram);
}

TEST_F(clBuildProgramTests, GivenBinaryAsInputWhenCreatingProgramWithSourceThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    void *pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

    binarySize = loadDataFromFile(
        testFile.c_str(),
        pBinary);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        (const unsigned char **)&pBinary,
        &binaryStatus,
        &retVal);

    deleteDataReadFromFile(pBinary);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    CompilerInterface::shutdown();
}

TEST_F(clBuildProgramTests, GivenProgramCreatedFromBinaryWhenBuildProgramWithOptionsIsCalledThenStoredOptionsAreUsed) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    void *pBinary = nullptr;
    size_t binarySize = 0;
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, "CopyBuffer_simd8_", ".bin");

    binarySize = loadDataFromFile(
        testFile.c_str(),
        pBinary);

    ASSERT_NE(0u, binarySize);
    ASSERT_NE(nullptr, pBinary);

    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        (const unsigned char **)&pBinary,
        &binaryStatus,
        &retVal);

    auto pInternalProgram = castToObject<Program>(pProgram);

    deleteDataReadFromFile(pBinary);
    auto storedOptionsSize = pInternalProgram->getOptions().size();

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const char *newBuildOption = "cl-fast-relaxed-math";

    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
        newBuildOption,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto optionsAfterBuildSize = pInternalProgram->getOptions().size();

    EXPECT_EQ(optionsAfterBuildSize, storedOptionsSize);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    CompilerInterface::shutdown();
}

TEST_F(clBuildProgramTests, GivenSpirAsInputWhenCreatingProgramFromBinaryThenProgramBuildSucceeds) {
    cl_program pProgram = nullptr;
    cl_int binaryStatus = CL_SUCCESS;
    char llvm[16] = "BC\xc0\xde";
    void *binary = llvm;
    size_t binarySize = sizeof(llvm);

    pProgram = clCreateProgramWithBinary(
        pContext,
        num_devices,
        devices,
        &binarySize,
        (const unsigned char **)&binary,
        &binaryStatus,
        &retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    MockCompilerDebugVars igcDebugVars;
    SProgramBinaryHeader progBin = {};
    progBin.Magic = iOpenCL::MAGIC_CL;
    progBin.Version = iOpenCL::CURRENT_ICBE_VERSION;
    progBin.Device = pContext->getDevice(0)->getHardwareInfo().pPlatform->eRenderCoreFamily;
    igcDebugVars.binaryToReturn = &progBin;
    igcDebugVars.binaryToReturnSize = sizeof(progBin);
    auto prevDebugVars = getIgcDebugVars();
    setIgcDebugVars(igcDebugVars);
    retVal = clBuildProgram(
        pProgram,
        num_devices,
        devices,
        "-x spir -spir-std=1.2",
        nullptr,
        nullptr);
    setIgcDebugVars(prevDebugVars);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    CompilerInterface::shutdown();
}

TEST_F(clBuildProgramTests, GivenNullAsInputWhenCreatingProgramThenInvalidProgramErrorIsReturned) {
    retVal = clBuildProgram(
        nullptr,
        1,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}
} // namespace ULT
