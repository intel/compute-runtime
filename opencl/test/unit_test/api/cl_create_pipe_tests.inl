/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClCreatePipeTests = ApiTests;

TEST_F(ClCreatePipeTests, WhenCreatingPipeThenInvalidOperationErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;
    auto pipe = clCreatePipe(pContext, flags, 1, 20, nullptr, &retVal);

    EXPECT_EQ(nullptr, pipe);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}
