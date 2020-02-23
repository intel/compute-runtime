/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

#include <CL/cl_va_api_media_sharing_intel.h>

using namespace NEO;

typedef api_tests clEnqueueAcquireVaMediaSurfacesTests;

namespace ULT {

TEST_F(clEnqueueAcquireVaMediaSurfacesTests, givenNullCommandQueueWhenAcquireIsCalledThenInvalidCommandQueueIsReturned) {
    retVal = clEnqueueAcquireVA_APIMediaSurfacesINTEL(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_COMMAND_QUEUE);
}
} // namespace ULT
