/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

#include <CL/cl_va_api_media_sharing_intel.h>

using namespace NEO;

typedef api_tests clCreateFromVaMediaSurfaceTests;

namespace ULT {

TEST_F(clCreateFromVaMediaSurfaceTests, givenNullContextWhenCreateIsCalledThenErrorIsReturned) {
    auto memObj = clCreateFromVA_APIMediaSurfaceINTEL(nullptr, CL_MEM_READ_WRITE, nullptr, 0, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
} // namespace ULT
