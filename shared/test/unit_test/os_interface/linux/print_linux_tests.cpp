/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/print.h"

#include "gtest/gtest.h"

#include <cstdint>

TEST(SimpleSprintfLinuxTest, GivenConversionLongerThanOutputWhenSimpleSprintfIsCalledThenCharactersWrittenAreReturned) {
    char out[8] = {};

    EXPECT_EQ(sizeof(out) - 1, simpleSprintf<int32_t>(out, sizeof(out), "%20d", 1));
    EXPECT_EQ(sizeof(out) - 1, simpleSprintf(out, sizeof(out), "%20s", "value"));
}
