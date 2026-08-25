/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
    NEO::writeDataToFile(cpuinfoFile.c_str(), cpuinfoData, false);
    EXPECT_TRUE(virtualFileExists(cpuinfoFile));

    VariableBackup<decltype(CpuInfo::getCpuFlagsFunc)> funcBackup(&CpuInfo::getCpuFlagsFunc, mockGetCpuFlags);
    CpuInfo testCpuInfo;
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag1"));
    EXPECT_TRUE(testCpuInfo.isCpuFlagPresent("flag2"));
    EXPECT_FALSE(testCpuInfo.isCpuFlagPresent("nonExistingCpuFlag"));

    removeVirtualFile(cpuinfoFile.c_str());
}

namespace {

struct FakeSysFs {
    struct Directory {
        std::vector<std::string> entries;
        size_t nextEntry = 0;
        struct dirent entry{};
    };

    struct Cache {
        std::string level;
        std::string size;
        std::string sharedProcessorList;
    };

    std::map<std::string, std::vector<std::string>> directories;
    std::map<std::string, std::string> files;
    std::vector<std::unique_ptr<Directory>> openedDirectories;
    std::map<int, std::string> openedFiles;
    int nextDescriptor = 1;

    void addProcessor(const std::string &name, const std::vector<Cache> &caches) {
        directories[Os::sysFsSystemCpuPathPrefix].push_back(name);
        const std::string cacheDirectoryPath = std::string(Os::sysFsSystemCpuPathPrefix) + "/" + name + "/cache";
        uint32_t index = 0;
        for (const auto &cache : caches) {
            const std::string indexName = "index" + std::to_string(index++);
            directories[cacheDirectoryPath].push_back(indexName);
            const std::string cachePath = cacheDirectoryPath + "/" + indexName;
            files[cachePath + "/level"] = cache.level;
            files[cachePath + "/size"] = cache.size;
            if (!cache.sharedProcessorList.empty()) {
                files[cachePath + "/shared_cpu_list"] = cache.sharedProcessorList;
            }
        }
    }
};

FakeSysFs *fakeSysFs = nullptr;

DIR *fakeOpendir(const char *name) {
    auto directory = fakeSysFs->directories.find(name);
    if (directory == fakeSysFs->directories.end()) {
        return nullptr;
    }
    auto opened = std::make_unique<FakeSysFs::Directory>();
    opened->entries = directory->second;
    auto *raw = opened.get();
    fakeSysFs->openedDirectories.push_back(std::move(opened));
    return reinterpret_cast<DIR *>(raw);
}

struct dirent *fakeReaddir(DIR *dir) {
    auto *opened = reinterpret_cast<FakeSysFs::Directory *>(dir);
    if (opened->nextEntry >= opened->entries.size()) {
        return nullptr;
    }
    const auto &name = opened->entries[opened->nextEntry++];
    std::snprintf(opened->entry.d_name, sizeof(opened->entry.d_name), "%s", name.c_str());
    return &opened->entry;
}

int fakeClosedir(DIR *dir) {
    return 0;
}

int fakeOpen(const char *pathname, int flags) {
    if (fakeSysFs->files.find(pathname) == fakeSysFs->files.end()) {
        return -1;
    }
    const int descriptor = fakeSysFs->nextDescriptor++;
    fakeSysFs->openedFiles[descriptor] = pathname;
    return descriptor;
}

ssize_t fakePread(int fd, void *buf, size_t count, off_t offset) {
    auto descriptor = fakeSysFs->openedFiles.find(fd);
    if (descriptor == fakeSysFs->openedFiles.end()) {
        return -1;
    }
    const auto &content = fakeSysFs->files[descriptor->second];
    const size_t bytesToCopy = std::min(count, content.size());
    std::memcpy(buf, content.data(), bytesToCopy);
    return static_cast<ssize_t>(bytesToCopy);
}

struct LastLevelCacheSizeFixture {
    void setUp() {
        fakeSysFs = &sysFs;
        opendirBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsOpendir)>>(&SysCalls::sysCallsOpendir, fakeOpendir);
        readdirBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsReaddir)>>(&SysCalls::sysCallsReaddir, fakeReaddir);
        closedirBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsClosedir)>>(&SysCalls::sysCallsClosedir, fakeClosedir);
        openBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsOpen)>>(&SysCalls::sysCallsOpen, fakeOpen);
        preadBackup = std::make_unique<VariableBackup<decltype(SysCalls::sysCallsPread)>>(&SysCalls::sysCallsPread, fakePread);
    }

    void tearDown() {
        fakeSysFs = nullptr;
    }

    size_t detectLastLevelCacheSize() {
        return CpuInfo::getLastLevelCacheSizeFunc();
    }

    FakeSysFs sysFs;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsOpendir)>> opendirBackup;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsReaddir)>> readdirBackup;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsClosedir)>> closedirBackup;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsOpen)>> openBackup;
    std::unique_ptr<VariableBackup<decltype(SysCalls::sysCallsPread)>> preadBackup;
};

using LastLevelCacheSizeTest = Test<LastLevelCacheSizeFixture>;

} // namespace

TEST_F(LastLevelCacheSizeTest, WhenLastLevelCacheIsReportedThenItsSizeIsReturned) {
    sysFs.addProcessor("cpu0", {{"1", "48K"}, {"2", "2048K"}, {"3", "107520K"}});

    EXPECT_EQ(105u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenLowerLevelCacheLargerThanLastLevelCacheWhenDetectingThenLastLevelCacheSizeIsReturned) {
    sysFs.addProcessor("cpu0", {{"2", "8192K"}, {"3", "4096K"}});

    EXPECT_EQ(4u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenProcessorWithoutLastLevelCacheEnumeratedFirstWhenDetectingThenLargerCacheOfLaterProcessorIsReturned) {
    sysFs.addProcessor("cpu0", {{"1", "32K"}, {"2", "2048K"}});
    sysFs.addProcessor("cpu1", {{"1", "48K"}, {"2", "2048K"}, {"3", "12288K"}});

    EXPECT_EQ(12u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenProcessorsReportingDifferentLastLevelCacheSizesWhenDetectingThenAllProcessorsAreScannedAndLargestIsReturned) {
    sysFs.addProcessor("cpu0", {{"1", "32K"}, {"2", "1024K"}, {"3", "32768K"}});
    sysFs.addProcessor("cpu1", {{"1", "32K"}, {"2", "1024K"}, {"3", "98304K"}});

    const int openedDirectoriesBefore = SysCalls::opendirCalled;

    EXPECT_EQ(96u * MemoryConstants::megaByte, detectLastLevelCacheSize());

    constexpr int scannedProcessorCount = 2;
    EXPECT_EQ(1 + scannedProcessorCount, SysCalls::opendirCalled - openedDirectoriesBefore);
}

TEST_F(LastLevelCacheSizeTest, GivenProcessorsSharingLastLevelCacheWhenDetectingThenSharedProcessorsAreNotScanned) {
    sysFs.addProcessor("cpu0", {{"1", "32K"}, {"3", "8192K", "0-3"}});
    sysFs.addProcessor("cpu1", {{"1", "32K"}, {"3", "8192K"}});
    sysFs.addProcessor("cpu2", {{"1", "32K"}, {"3", "8192K"}});
    sysFs.addProcessor("cpu3", {{"1", "32K"}, {"3", "8192K"}});

    const int openedDirectoriesBefore = SysCalls::opendirCalled;

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());

    constexpr int scannedProcessorCount = 1;
    EXPECT_EQ(1 + scannedProcessorCount, SysCalls::opendirCalled - openedDirectoriesBefore);
}

TEST_F(LastLevelCacheSizeTest, GivenSharedProcessorListWithRangesAndSingleEntriesWhenDetectingThenOnlyListedProcessorsAreExcluded) {
    sysFs.addProcessor("cpu0", {{"3", "8192K", "0,2-3"}});
    sysFs.addProcessor("cpu1", {{"3", "8192K"}});
    sysFs.addProcessor("cpu2", {{"3", "8192K"}});
    sysFs.addProcessor("cpu3", {{"3", "8192K"}});

    const int openedDirectoriesBefore = SysCalls::opendirCalled;

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());

    constexpr int scannedProcessorCount = 2;
    EXPECT_EQ(1 + scannedProcessorCount, SysCalls::opendirCalled - openedDirectoriesBefore);
}

TEST_F(LastLevelCacheSizeTest, GivenNoProcessorReportsLastLevelCacheWhenDetectingThenAllProcessorsAreScannedAndLargestSeenCacheIsReturned) {
    constexpr uint32_t processorCount = 32u;
    for (uint32_t processor = 0; processor < processorCount; ++processor) {
        sysFs.addProcessor("cpu" + std::to_string(processor), {{"1", "32K"}, {"2", "4096K"}});
    }

    const int openedDirectoriesBefore = SysCalls::opendirCalled;

    EXPECT_EQ(4u * MemoryConstants::megaByte, detectLastLevelCacheSize());

    EXPECT_EQ(1 + static_cast<int>(processorCount), SysCalls::opendirCalled - openedDirectoriesBefore);
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedInKilobytesWhenDetectingThenValueIsScaled) {
    sysFs.addProcessor("cpu0", {{"3", "12288K"}});

    EXPECT_EQ(12288u * MemoryConstants::kiloByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedInMegabytesWhenDetectingThenValueIsScaled) {
    sysFs.addProcessor("cpu0", {{"3", "96M"}});

    EXPECT_EQ(96u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedInGigabytesWhenDetectingThenValueIsScaled) {
    sysFs.addProcessor("cpu0", {{"3", "2G"}});

    EXPECT_EQ(2u * MemoryConstants::gigaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedInUnexpectedFormatWhenDetectingThenCacheIsIgnored) {
    sysFs.addProcessor("cpu0", {{"3", "1048576"}});

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedAsZeroWhenDetectingThenCacheIsIgnored) {
    sysFs.addProcessor("cpu0", {{"3", "0K"}});

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenSizeReportedWithoutDigitsWhenDetectingThenCacheIsIgnored) {
    sysFs.addProcessor("cpu0", {{"3", "unknown"}});

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenTopologyDirectoryUnavailableWhenDetectingThenZeroIsReturned) {
    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenProcessorWithoutCacheDirectoryWhenDetectingThenProcessorIsSkipped) {
    sysFs.directories[Os::sysFsSystemCpuPathPrefix].push_back("cpu0");
    sysFs.addProcessor("cpu1", {{"3", "8192K"}});

    const int openedDirectoriesBefore = SysCalls::opendirCalled;

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());

    constexpr int scannedProcessorCount = 2;
    EXPECT_EQ(1 + scannedProcessorCount, SysCalls::opendirCalled - openedDirectoriesBefore);
}

TEST_F(LastLevelCacheSizeTest, GivenNonProcessorEntriesWhenDetectingThenTheyAreIgnored) {
    sysFs.directories[Os::sysFsSystemCpuPathPrefix].push_back("cpufreq");
    sysFs.directories[Os::sysFsSystemCpuPathPrefix].push_back("cpuidle");
    sysFs.directories[Os::sysFsSystemCpuPathPrefix].push_back("online");
    sysFs.directories[Os::sysFsSystemCpuPathPrefix].push_back("power");
    sysFs.addProcessor("cpu0", {{"3", "8192K"}});

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenUnreadableCacheAttributesWhenDetectingThenEntryIsSkipped) {
    sysFs.addProcessor("cpu0", {{"3", "8192K"}});
    sysFs.files.erase(std::string(Os::sysFsSystemCpuPathPrefix) + "/cpu0/cache/index0/size");

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenUnreadableCacheLevelWhenDetectingThenEntryIsSkipped) {
    sysFs.addProcessor("cpu0", {{"3", "8192K"}});
    sysFs.files.erase(std::string(Os::sysFsSystemCpuPathPrefix) + "/cpu0/cache/index0/level");

    EXPECT_EQ(0u, detectLastLevelCacheSize());
}

TEST_F(LastLevelCacheSizeTest, GivenNonCacheEntriesInCacheDirectoryWhenDetectingThenTheyAreIgnored) {
    sysFs.addProcessor("cpu0", {{"3", "8192K"}});
    sysFs.directories[std::string(Os::sysFsSystemCpuPathPrefix) + "/cpu0/cache"].push_back("uevent");

    EXPECT_EQ(8u * MemoryConstants::megaByte, detectLastLevelCacheSize());
}
