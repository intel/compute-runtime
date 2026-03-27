/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <fstream>

using namespace NEO;

void mockGetCpuFlags(std::string &cpuFlags) {
    size_t fileSize = 0;
    std::string cpuinfoFile = "cpuinfo";
    auto fileData = loadDataFromVirtualFile(cpuinfoFile.c_str(), fileSize);

    std::string data(fileData.get(), fileSize);
    std::istringstream cpuinfo(data);
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.substr(0, 8) == "Features") {
            cpuFlags = line;
            break;
        }
    }
}

TEST(CpuInfoX86Linux, GivenCpuinfoContentWhenGetCpuFlagsLinuxIsCalledThenFlagsAreExtracted) {
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBkp(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPread)> preadBkp(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr std::string_view content = "processor\t: 0\nflags : sse2 avx\n";
        memcpy_s(buf, count, content.data(), content.size());
        return static_cast<ssize_t>(content.size());
    });
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBkp(&SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    std::string cpuFlags;
    CpuInfo::getCpuFlagsFunc(cpuFlags);
    EXPECT_FALSE(cpuFlags.empty());
    EXPECT_NE(std::string::npos, cpuFlags.find("flags"));
}

TEST(CpuInfo, givenProcCpuinfoFileExistsWhenIsCpuFlagPresentIsCalledThenValidValueIsReturned) {
    VariableBackup<const char *> pathPrefixBackup(&Os::sysFsProcPathPrefix, ".");
    std::string cpuinfoFile = "cpuinfo";
    EXPECT_FALSE(virtualFileExists(cpuinfoFile));
    constexpr std::string_view cpuinfoData = "processor\t\t: 0\nFeatures\t\t: flag1 flag2 flag3\n";
    NEO::writeDataToFile(cpuinfoFile.c_str(), cpuinfoData);
    EXPECT_TRUE(virtualFileExists(cpuinfoFile));

    VariableBackup<decltype(CpuInfo::getCpuFlagsFunc)> funcBackup(&CpuInfo::getCpuFlagsFunc, mockGetCpuFlags);
    CpuInfo testCpuInfo;
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag1"));
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag2"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));

    removeVirtualFile(cpuinfoFile.c_str());
}
