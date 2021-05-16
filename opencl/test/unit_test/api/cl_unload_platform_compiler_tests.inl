/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using clUnloadPlatformCompilerTests = api_tests;

namespace ULT {

TEST_F(clUnloadPlatformCompilerTests, GivenNullptrPlatformWhenUnloadingPlatformCompilerThenInvalidPlatformErrorIsReturned) {
    auto retVal = clUnloadPlatformCompiler(nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clUnloadPlatformCompilerTests, WhenUnloadingPlatformCompilerThenSuccessIsReturned) {
    auto retVal = clUnloadPlatformCompiler(platform());
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
