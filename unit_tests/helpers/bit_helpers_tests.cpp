/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/helpers/bit_helpers.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(IsBitSetTests, givenDifferentValuesWhenTestingIsBitSetThenCorrectValueIsReturned) {
    size_t field1 = 0;
    size_t field2 = 0b1;
    size_t field3 = 0b1000;
    size_t field4 = 0b1010;

    EXPECT_FALSE(isBitSet(field1, 0));
    EXPECT_FALSE(isBitSet(field1, 1));
    EXPECT_FALSE(isBitSet(field1, 2));
    EXPECT_FALSE(isBitSet(field1, 3));

    EXPECT_TRUE(isBitSet(field2, 0));
    EXPECT_FALSE(isBitSet(field2, 1));
    EXPECT_FALSE(isBitSet(field2, 2));
    EXPECT_FALSE(isBitSet(field2, 3));

    EXPECT_FALSE(isBitSet(field3, 0));
    EXPECT_FALSE(isBitSet(field3, 1));
    EXPECT_FALSE(isBitSet(field3, 2));
    EXPECT_TRUE(isBitSet(field3, 3));

    EXPECT_FALSE(isBitSet(field4, 0));
    EXPECT_TRUE(isBitSet(field4, 1));
    EXPECT_FALSE(isBitSet(field4, 2));
    EXPECT_TRUE(isBitSet(field4, 3));
}

TEST(IsValSetTests, givenDifferentValuesWhenTestingIsValSetThenCorrectValueIsReturned) {
    size_t field1 = 0;
    size_t field2 = 0b1;
    size_t field3 = 0b10;
    size_t field4 = 0b1000;
    size_t field5 = 0b1010;
    size_t field6 = 0b1111;

    EXPECT_FALSE(isValSet(field1, field2));
    EXPECT_FALSE(isValSet(field1, field3));
    EXPECT_FALSE(isValSet(field1, field4));
    EXPECT_FALSE(isValSet(field1, field5));
    EXPECT_FALSE(isValSet(field1, field6));

    EXPECT_TRUE(isValSet(field2, field2));
    EXPECT_FALSE(isValSet(field2, field3));
    EXPECT_FALSE(isValSet(field2, field4));
    EXPECT_FALSE(isValSet(field2, field5));
    EXPECT_FALSE(isValSet(field2, field6));

    EXPECT_FALSE(isValSet(field3, field2));
    EXPECT_TRUE(isValSet(field3, field3));
    EXPECT_FALSE(isValSet(field3, field4));
    EXPECT_FALSE(isValSet(field3, field5));
    EXPECT_FALSE(isValSet(field3, field6));

    EXPECT_FALSE(isValSet(field4, field2));
    EXPECT_FALSE(isValSet(field4, field3));
    EXPECT_TRUE(isValSet(field4, field4));
    EXPECT_FALSE(isValSet(field4, field5));
    EXPECT_FALSE(isValSet(field4, field6));

    EXPECT_FALSE(isValSet(field5, field2));
    EXPECT_TRUE(isValSet(field5, field3));
    EXPECT_TRUE(isValSet(field5, field4));
    EXPECT_TRUE(isValSet(field5, field5));
    EXPECT_FALSE(isValSet(field5, field6));

    EXPECT_TRUE(isValSet(field6, field2));
    EXPECT_TRUE(isValSet(field6, field3));
    EXPECT_TRUE(isValSet(field6, field4));
    EXPECT_TRUE(isValSet(field6, field5));
    EXPECT_TRUE(isValSet(field6, field6));
}
