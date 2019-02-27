/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/variable_backup.h"

#include "cl_api_tests.h"

using namespace OCLRT;

namespace OCLRT {
extern bool getDevicesResult;
}; // namespace OCLRT

typedef api_tests clGetPlatformIDsTests;

namespace ULT {
TEST_F(clGetPlatformIDsTests, getCount) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, 0u);
}

TEST_F(clGetPlatformIDsTests, getInfo) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(1, &platform, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, platform);
}

TEST_F(clGetPlatformIDsTests, NoPlatformListReturnsError) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(0, &platform, nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clGetPlatformIDsNegativeTests, WhenInitFailedThenErrorIsReturned) {
    VariableBackup<decltype(getDevicesResult)> bkp(&getDevicesResult, false);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformImpl.reset(nullptr);
}
} // namespace ULT
