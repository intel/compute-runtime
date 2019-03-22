/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/additional_extensions.h"
#include "test.h"

using namespace NEO;

TEST(AdditionalExtension, GivenFuncNameWhenGetingFunctionAddressThenReturnNullptr) {
    auto address = getAdditionalExtensionFunctionAddress("clFunction");
    EXPECT_EQ(nullptr, address);
}
