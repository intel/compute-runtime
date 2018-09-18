/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clUnloadCompilerTests;

namespace ULT {

TEST_F(clUnloadCompilerTests, notImplemented) {
    auto retVal = clUnloadCompiler();
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}
} // namespace ULT
