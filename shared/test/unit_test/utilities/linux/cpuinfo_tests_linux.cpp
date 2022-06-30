/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace NEO;

TEST(CpuInfo, givenProcCpuinfoFileIsNotExistsWhenIsCpuFlagPresentIsCalledThenValidValueIsReturned) {
    std::string cpuinfoFile = "test_files/linux/proc/cpuinfo";
    EXPECT_FALSE(fileExists(cpuinfoFile));

    CpuInfo testCpuInfo;
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("flag1"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("flag2"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));
}
