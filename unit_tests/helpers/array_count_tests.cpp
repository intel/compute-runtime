/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/array_count.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ArrayCountTests, arrayCount) {
    int a[10];
    EXPECT_EQ(10u, arrayCount(a));
}

TEST(ArrayCountTests, isInRange) {
    int a[10];
    EXPECT_TRUE(isInRange(1, a));
    EXPECT_FALSE(isInRange(10, a));
}

} // namespace NEO