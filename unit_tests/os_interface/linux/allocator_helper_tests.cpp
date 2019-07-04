/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/basic_math.h"
#include "runtime/os_interface/linux/allocator_helper.h"

#include "gtest/gtest.h"

TEST(AllocatorHelper, givenExpectedSizeToReserveWhenGetSizeToReserveCalledThenExpectedValueReturned) {
    EXPECT_EQ((4 * 4 + 2 * 4) * GB, NEO::getSizeToReserve());
}
