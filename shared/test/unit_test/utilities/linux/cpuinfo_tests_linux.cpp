/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CpuInfo, givenEmptyCpuInfoWhenIsCpuFlagPresentIsCalledThenValidValueIsReturned) {
    CpuInfo testCpuInfo{};
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("flag1"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("flag2"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));
}
