/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/aub/aub_helper_tests.inl"

TEST(AubHelper, GivenHwInfoWhenGetMemBankSizeIsCalledThenItReturnsCorrectValue) {
    EXPECT_EQ(2 * MemoryConstants::gigaByte, AubHelper::getMemBankSize(platformDevices[0]));
}
