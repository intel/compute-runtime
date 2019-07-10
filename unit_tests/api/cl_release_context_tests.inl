/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clReleaseContextTests;

namespace ULT {

TEST_F(clReleaseContextTests, GivenValidContextWhenReleasingContextThenSuccessIsReturned) {
    auto context = clCreateContext(
        nullptr,
        num_devices,
        devices,
        nullptr,
        nullptr,
        &retVal);

    EXPECT_NE(nullptr, context);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clReleaseContextTests, GivenNullContextWhenReleasingContextThenClInvalidContextIsReturned) {
    auto retVal = clReleaseContext(nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
} // namespace ULT
