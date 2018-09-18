/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "runtime/os_interface/linux/allocator_helper.h"

TEST(AllocatorHelper, givenExpectedSizeToMapWhenGetSizetoMapCalledThenExpectedValueReturned) {
    EXPECT_EQ(1 * 1024 * 1024u, OCLRT::getSizeToMap());
}
