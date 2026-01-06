/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cpu_copy_helper.h"

#include "gtest/gtest.h"

TEST(CpuCopyHelperTests, givenWindowsWhenCheckingForSmallBarThenFalseIsReturned) {
    EXPECT_FALSE(NEO::isSmallBarConfigPresent(nullptr));
    EXPECT_FALSE(NEO::isSmallBarConfigPresent(reinterpret_cast<NEO::OSInterface *>(0x1234)));
}
