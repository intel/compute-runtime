/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/extendable_enum.h"

#include "gtest/gtest.h"

namespace ExtendableEnumTest {
struct Type : ExtendableEnum {
  public:
    constexpr Type(uint32_t val) : ExtendableEnum(val) {}
};
constexpr Type testEnum0{0};
constexpr Type testEnum1{1};
} // namespace ExtendableEnumTest

TEST(ExtendableEnumTest, givenExtendableEnumWhenValuesAreCheckedThenCorrectValuesAreCorrect) {
    EXPECT_EQ(0u, ExtendableEnumTest::testEnum0);
    EXPECT_EQ(1u, ExtendableEnumTest::testEnum1);
}

TEST(ExtendableEnumTest, givenExtendableEnumVariableWhenValueIsAssignedThenCorrectValueIsStored) {
    ExtendableEnumTest::Type enumVal0 = ExtendableEnumTest::testEnum0;

    EXPECT_EQ(ExtendableEnumTest::testEnum0, enumVal0);
}

namespace ExtendableEnumTest {
constexpr Type testEnum2{2};
}

TEST(ExtendableEnumTest, givenExtendableEnumWhenNewValuesAreAddedThenCorrectValuesAreAssigned) {
    EXPECT_EQ(2u, ExtendableEnumTest::testEnum2);
}
