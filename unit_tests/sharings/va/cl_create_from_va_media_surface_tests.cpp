/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/api/cl_api_tests.h"

#include <CL/cl_va_api_media_sharing_intel.h>

using namespace OCLRT;

typedef api_tests clCreateFromVaMediaSurfaceTests;

namespace ULT {

TEST_F(clCreateFromVaMediaSurfaceTests, givenNullContextWhenCreateIsCalledThenErrorIsReturned) {
    auto memObj = clCreateFromVA_APIMediaSurfaceINTEL(nullptr, CL_MEM_READ_WRITE, nullptr, 0, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}
} // namespace ULT
