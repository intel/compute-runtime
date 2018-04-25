/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/memory_management.h"

using namespace OCLRT;

typedef api_tests clCompileProgramTests;

namespace ULT {

TEST_F(clCompileProgramTests, CompileSingleSource) {
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
    CompilerInterface::shutdown();
}

TEST_F(clCompileProgramTests, CompileSingleSourceWithHeader) {
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

    CompilerInterface::shutdown();
}

TEST_F(clCompileProgramTests, NullProgram_returnsError) {
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
