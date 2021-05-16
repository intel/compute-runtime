/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "gtest/gtest.h"

#include <type_traits>

TEST(OSInterface, WhenInterfaceIsCreatedThenItIsNonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<NEO::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_constructible<NEO::OSInterface>::value);
}

TEST(OSInterface, WhenInterfaceIsCreatedThenItIsNonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<NEO::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_assignable<NEO::OSInterface>::value);
}
