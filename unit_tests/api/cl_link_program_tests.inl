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
#include "unit_tests/helpers/test_files.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

typedef api_tests clLinkProgramTests;

TEST_F(clLinkProgramTests, GivenValidParamsWhenLinkingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

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

    cl_program program = pProgram;
    cl_program oprog;
    oprog = clLinkProgram(
        pContext,
        num_devices,
        devices,
        nullptr,
        1,
        &program,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(oprog);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}

TEST_F(clLinkProgramTests, GivenCreateLibraryOptionWhenLinkingProgramThenSuccessIsReturned) {
    cl_program pProgram = nullptr;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

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

    cl_program program = pProgram;
    cl_program oprog;
    oprog = clLinkProgram(
        pContext,
        num_devices,
        devices,
        "-create-library",
        1,
        &program,
        nullptr,
        nullptr,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(oprog);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}

TEST_F(clLinkProgramTests, GivenNullContextWhenLinkingProgramThenClInvalidContextErrorIsReturned) {
    cl_program program = {0};
    cl_program oprog;
    oprog = clLinkProgram(
        nullptr,
        num_devices,
        devices,
        nullptr,
        1,
        &program,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, oprog);
}
} // namespace ULT
