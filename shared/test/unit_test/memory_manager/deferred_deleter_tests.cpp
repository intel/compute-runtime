/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferred_deleter.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DeferredDeleter, WhenDeferredDeleterIsCreatedThenItIsNotMoveableOrCopyable) {
    EXPECT_FALSE(std::is_move_constructible<DeferredDeleter>::value);
    EXPECT_FALSE(std::is_copy_constructible<DeferredDeleter>::value);
}

TEST(DeferredDeleter, WhenDeferredDeleterIsCreatedThenItIsNotAssignable) {
    EXPECT_FALSE(std::is_move_assignable<DeferredDeleter>::value);
    EXPECT_FALSE(std::is_copy_assignable<DeferredDeleter>::value);
}

TEST(DeferredDeleter, givenDeferredDeleterWhenBlockingDrainIsCalledThenArElementsReleasedIsCalled) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(true, false);
    EXPECT_NE(0, deleter->areElementsReleasedCalled);
    EXPECT_FALSE(deleter->areElementsReleasedCalledForHostptrs);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST(DeferredDeleter, givenDeferredDeleterWhenBlockingDrainOnlyForHostptrsIsCalledThenArElementsReleasedIsCalledWithHostptrsOnly) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(true, true);
    EXPECT_NE(0, deleter->areElementsReleasedCalled);
    EXPECT_TRUE(deleter->areElementsReleasedCalledForHostptrs);
    EXPECT_EQ(1, deleter->drainCalled);
}

TEST(DeferredDeleter, givenDeferredDeleterWhenNonBlockingDrainIsCalledThenArElementsReleasedIsNotCalled) {
    auto deleter = std::make_unique<MockDeferredDeleter>();
    deleter->drain(false, false);
    EXPECT_EQ(0, deleter->areElementsReleasedCalled);
    EXPECT_EQ(1, deleter->drainCalled);
}
