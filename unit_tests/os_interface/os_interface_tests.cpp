/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_interface.h"

#include "gtest/gtest.h"

#include <type_traits>

TEST(OSInterface, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<OCLRT::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_constructible<OCLRT::OSInterface>::value);
}

TEST(OSInterface, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<OCLRT::OSInterface>::value);
    EXPECT_FALSE(std::is_copy_assignable<OCLRT::OSInterface>::value);
}
