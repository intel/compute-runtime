/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "test.h"
#include "runtime/utilities/vec.h"

using namespace OCLRT;

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
