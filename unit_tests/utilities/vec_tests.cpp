/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/vec.h"

#include "gtest/gtest.h"

TEST(VecTest, operators) {
    Vec3<size_t> v0(nullptr);
    Vec3<size_t> v1({1, 2, 3});
    Vec3<size_t> v2(v1);
    Vec3<size_t> v3({0, 0, 0});
    Vec3<size_t> v4({1, 2, 1});

    ASSERT_EQ(v0, v3);
    ASSERT_EQ(v1, v2);
    ASSERT_NE(v1, v3);
    ASSERT_NE(v1, v4);

    v3 = v1;

    ASSERT_EQ(v2, v3);

    size_t arr[3] = {1, 5, 3};
    v3 = arr;

    ASSERT_NE(v1, v3);
}

TEST(VecTest, getSimpliefiedDim) {
    {
        Vec3<size_t> v{3, 3, 3};
        EXPECT_EQ(3U, v.getSimplifiedDim());
    }

    {
        Vec3<size_t> v{3, 3, 1};
        EXPECT_EQ(2U, v.getSimplifiedDim());
    }

    {
        Vec3<size_t> v{1, 1, 1};
        EXPECT_EQ(1U, v.getSimplifiedDim());
    }

    {
        Vec3<size_t> v{0, 0, 0};
        EXPECT_EQ(0U, v.getSimplifiedDim());
    }
}
