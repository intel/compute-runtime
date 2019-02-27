/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/api/cl_api_tests.h"

#include <CL/cl_va_api_media_sharing_intel.h>

using namespace OCLRT;

typedef api_tests clEnqueueReleaseVaMediaSurfacesTests;

namespace ULT {

TEST_F(clEnqueueReleaseVaMediaSurfacesTests, givenNullCommandQueueWhenReleaseObjectsIsCalledThenInvalidCommandQueueIsReturned) {
    retVal = clEnqueueReleaseVA_APIMediaSurfacesINTEL(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_COMMAND_QUEUE);
}
} // namespace ULT
