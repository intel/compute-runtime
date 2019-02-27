/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCompileProgramTests;

namespace ULT {

TEST_F(clCompileProgramTests, GivenKernelAsSingleSourceWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("copybuffer", false);
    testFile.append(clFiles);
    testFile.append("copybuffer.cl");

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

    retVal = clCompileProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}

TEST_F(clCompileProgramTests, GivenKernelAsSourceWithHeaderWhenCompilingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    cl_program pHeader = nullptr;
    void *pSource = nullptr;
    void *pHeaderSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    const char *simpleHeaderName = "simple_header.h";

    testFile.append(clFiles);
    testFile.append("/copybuffer_with_header.cl");

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

    testFile.clear();
    testFile.append(clFiles);
    testFile.append("simple_header.h");
    sourceSize = loadDataFromFile(
        testFile.c_str(),
        pHeaderSource);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pHeaderSource);

    pHeader = clCreateProgramWithSource(
        pContext,
        1,
        (const char **)&pHeaderSource,
        &sourceSize,
        &retVal);

    EXPECT_NE(nullptr, pHeader);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clCompileProgram(
        pProgram,
        num_devices,
        devices,
        nullptr,
        1,
        &pHeader,
        &simpleHeaderName,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    deleteDataReadFromFile(pSource);

    retVal = clReleaseProgram(pHeader);
    EXPECT_EQ(CL_SUCCESS, retVal);
    deleteDataReadFromFile(pHeaderSource);
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
} // namespace ULT
