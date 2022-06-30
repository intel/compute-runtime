/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

using namespace NEO;

namespace ULT {

TEST(clUnloadCompilerTests, WhenUnloadingCompilerThenSuccessIsReturned) {
    auto retVal = clUnloadCompiler();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

} // namespace ULT
