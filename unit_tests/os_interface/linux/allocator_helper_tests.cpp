/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/allocator_helper.h"

#include "gtest/gtest.h"

TEST(AllocatorHelper, givenExpectedSizeToMapWhenGetSizetoMapCalledThenExpectedValueReturned) {
    EXPECT_EQ(1 * 1024 * 1024u, OCLRT::getSizeToMap());
}
