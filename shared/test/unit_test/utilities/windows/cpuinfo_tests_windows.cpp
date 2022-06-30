/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CpuInfo, givenIsCpuFlagPresentCalledThenFalseIsReturned) {
    CpuInfo testCpuInfo;

    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("fpu"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("vme"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));
}
