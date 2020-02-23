/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clUnloadPlatformCompilerTests;

namespace ULT {

TEST_F(clUnloadPlatformCompilerTests, WhenUnloadingPlatformCompilerThenOutOfHostMemoryErrorIsReturned) {
    auto retVal = clUnloadPlatformCompiler(nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}
} // namespace ULT
