/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ArrayCountTests, WhenGettingArrayCountThenCorrectCountIsReturned) {
    int a[10];
    EXPECT_EQ(10u, arrayCount(a));
}

TEST(ArrayCountTests, WhenGettingInRangeThenOnlyIndexInRangeReturnsTrue) {
    int a[10];
    EXPECT_TRUE(isInRange(1, a));
    EXPECT_FALSE(isInRange(10, a));
}

} // namespace NEO