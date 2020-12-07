/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/cpu_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CpuInfo, givenProcCpuinfoFileExistsWhenIsCpuFlagPresentIsCalledThenValidValueIsReturned) {
    std::string cpuinfoFile = "test_files/linux/proc/cpuinfo";
    EXPECT_TRUE(fileExists(cpuinfoFile));

    CpuInfo testCpuInfo;
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("fpu"));
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("vme"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));
}
