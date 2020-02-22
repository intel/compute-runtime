/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/additional_extensions.h"
#include "test.h"

using namespace NEO;

TEST(AdditionalExtension, GivenFuncNameWhenGetingFunctionAddressThenReturnNullptr) {
    auto address = getAdditionalExtensionFunctionAddress("clFunction");
    EXPECT_EQ(nullptr, address);
}
