/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/additional_extensions.h"

using namespace NEO;

TEST(AdditionalExtension, GivenFuncNameWhenGetingFunctionAddressThenReturnNullptr) {
    auto address = getAdditionalExtensionFunctionAddress("clFunction");
    EXPECT_EQ(nullptr, address);
}
