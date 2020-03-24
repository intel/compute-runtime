/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub/aub_helper_tests.inl"

TEST(AubHelper, GivenHwInfoWhenGetMemBankSizeIsCalledThenItReturnsCorrectValue) {
    EXPECT_EQ(2 * MemoryConstants::gigaByte, AubHelper::getMemBankSize(defaultHwInfo.get()));
}
