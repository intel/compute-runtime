/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;
struct ClGetKernelWorkGroupInfoTest : public ApiFixture<>,
                                      public ::testing::Test {
    typedef ApiFixture BaseClass;

    void SetUp() override {
        USE_REAL_FILE_SYSTEM();
        BaseClass::setUp();

        std::unique_ptr<char[]> pSource = nullptr;
        size_t sourceSize = 0;
        std::string testFile;

        kbHelper = new KernelBinaryHelper("CopyBuffer_simd16", false);
        testFile.append(clFiles);
        testFile.append("CopyBuffer_simd16.cl");
        ASSERT_EQ(true, fileExists(testFile));

        pSource = loadDataFromFile(
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

        pSource.reset();

        retVal = clBuildProgram(
            pProgram,
            1,
            &testedClDevice,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clReleaseProgram(pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete kbHelper;
        BaseClass::tearDown();
    }

    cl_program pProgram = nullptr;
    cl_kernel kernel = nullptr;
    KernelBinaryHelper *kbHelper;
};

struct ClGetKernelWorkGroupInfoTests : public ClGetKernelWorkGroupInfoTest,
                                       public ::testing::WithParamInterface<uint32_t> {
};
