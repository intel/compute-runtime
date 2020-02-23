/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "gtest/gtest.h"

#include <type_traits>

TEST(OSInterface, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<NEO::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_constructible<NEO::OSInterface>::value);
}

TEST(OSInterface, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<NEO::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_assignable<NEO::OSInterface>::value);
}
