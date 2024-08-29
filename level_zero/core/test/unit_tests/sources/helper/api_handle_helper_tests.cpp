/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include "gtest/gtest.h"

struct MockTypeForTest {
    const uint64_t objMagic = objMagicValue;
};
using MockPointerTypeForTest = MockTypeForTest *;

TEST(ApiHandleHelperTest, givenNullptrWhenTranslatingToInternalTypeThenNullptrIsReturned) {
    MockPointerTypeForTest input = nullptr;
    EXPECT_EQ(nullptr, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenValidObjecctWhenTranslatingToInternalTypeThenInputIsRetuned) {
    MockTypeForTest dummy{};
    MockPointerTypeForTest input = &dummy;
    EXPECT_EQ(input, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenPointerToValidObjecctWhenTranslatingToInternalTypeThenValidObjectIsRetuned) {
    MockTypeForTest dummy{};
    MockPointerTypeForTest ptrToDummy = &dummy;
    MockPointerTypeForTest input = reinterpret_cast<MockPointerTypeForTest>(&ptrToDummy);
    EXPECT_EQ(ptrToDummy, toInternalType(input));
}

TEST(ApiHandleHelperTest, givenInvalidPointerWhenTranslatingToInternalTypeThenNullptrIsRetuned) {
    uint64_t dummy = 0XDEAFBEEF;
    MockPointerTypeForTest ptrToDummy = reinterpret_cast<MockPointerTypeForTest>(&dummy);
    MockPointerTypeForTest input = reinterpret_cast<MockPointerTypeForTest>(&ptrToDummy);
    EXPECT_EQ(nullptr, toInternalType(input));
}
