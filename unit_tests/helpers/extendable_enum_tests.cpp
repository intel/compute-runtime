/*
 * Copyright (c) 2018, Intel Corporation
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
