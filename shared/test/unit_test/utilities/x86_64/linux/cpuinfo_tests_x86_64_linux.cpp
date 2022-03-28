/*
 * Copyright (C) 2022 Intel Corporation
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

TEST(CpuInfo, givenProcCpuinfoFileExistsWhenIsCpuFlagPresentIsCalledThenValidValueIsReturned) {
    VariableBackup<const char *> pathPrefixBackup(&Os::sysFsProcPathPrefix, "./test_files");
    std::string cpuinfoFile = "./test_files/cpuinfo";
    EXPECT_FALSE(fileExists(cpuinfoFile));

    {
        std::ofstream cpuinfo(cpuinfoFile);
        cpuinfo << "processor\t\t: 0\nflags\t\t: flag1 flag2 flag3\n";
    }

    EXPECT_TRUE(fileExists(cpuinfoFile));

    CpuInfo testCpuInfo;
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag1"));
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag2"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));

    std::remove(cpuinfoFile.c_str());
}
