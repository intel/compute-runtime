/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;
struct ClGetKernelWorkGroupInfoTest : public ApiFixture, public FixtureWithMockZebin, public ::testing::Test {
    typedef ApiFixture BaseClass;

    void SetUp() override {
        BaseClass::setUp();
        FixtureWithMockZebin::setUp();

        pProgram = clCreateProgramWithSource(
            pContext,
            1,
            sampleKernelSrcs,
            &sampleKernelSize,
            &retVal);
        EXPECT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clBuildProgram(
            pProgram,
            1,
            &testedClDevice,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = clCreateKernel(pProgram, zebinPtr->kernelName, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clReleaseProgram(pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        FixtureWithMockZebin::tearDown();
        BaseClass::tearDown();
    }

    cl_program pProgram = nullptr;
    cl_kernel kernel = nullptr;

    FORBID_REAL_FILE_SYSTEM_CALLS();
};

struct ClGetKernelWorkGroupInfoTests : public ClGetKernelWorkGroupInfoTest,
                                       public ::testing::WithParamInterface<uint32_t> {
};
