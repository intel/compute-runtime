/*
 * Copyright (C) 2022 Intel Corporation
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
