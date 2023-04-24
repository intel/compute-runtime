/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClReleaseContextTests = ApiTests;

namespace ULT {

TEST_F(ClReleaseContextTests, GivenValidContextWhenReleasingContextThenSuccessIsReturned) {
    auto context = clCreateContext(
        nullptr,
        1u,
        &testedClDevice,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, context);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClReleaseContextTests, GivenNullContextWhenReleasingContextThenClInvalidContextIsReturned) {
    auto retVal = clReleaseContext(nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
} // namespace ULT
