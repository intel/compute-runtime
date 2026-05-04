/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include <cstring>

namespace OclocCacheCommandLinux {
NEO::CacheStats storedStats{};
bool returnedEntry = false;

decltype(NEO::SysCalls::sysCallsOpen) mockOpen = [](const char *pathname, int flags) -> int {
    if (strstr(pathname, "stats") != nullptr) {
        return 10;
    }
    errno = ENOENT;
    return -1;
};

decltype(NEO::SysCalls::sysCallsPread) mockPread = [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
    if (count == sizeof(NEO::CacheStats)) {
        memcpy(buf, &storedStats, sizeof(NEO::CacheStats));
        return sizeof(NEO::CacheStats);
    }
    return -1;
};

decltype(NEO::SysCalls::sysCallsPwrite) mockPwrite = [](int fd, const void *buf, size_t count, off_t offset) -> ssize_t {
    if (count == sizeof(NEO::CacheStats)) {
        memcpy(&storedStats, buf, sizeof(NEO::CacheStats));
        return sizeof(NEO::CacheStats);
    }
    return -1;
};

DIR *mockOpendir(const char *path) {
    returnedEntry = false;
    return reinterpret_cast<DIR *>(0x1);
}

struct dirent *mockReaddir(DIR *dirp) {
    static struct dirent entry;
    memset(&entry, 0, sizeof(entry));

    if (!returnedEntry) {
        returnedEntry = true;
        strncpy(entry.d_name, "stats", sizeof(entry.d_name) - 1);
        entry.d_type = DT_REG;
        return &entry;
    }
    return nullptr;
}

int mockClosedir(DIR *dirp) {
    return 0;
}

int mockStat(const std::string &path, struct stat *buf) {
    if (path.find("stats") != std::string::npos) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = S_IFREG;
        buf->st_size = sizeof(NEO::CacheStats);
        buf->st_atime = 1;
        return 0;
    }
    errno = ENOENT;
    return -1;
}

DIR *mockOpendirNotFound(const char *path) {
    errno = ENOENT;
    return nullptr;
}

} // namespace OclocCacheCommandLinux

TEST(OclocApiTestsLinux, GivenCacheCommandWithShowStatsAndNoStatsFileThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, [](const char *, int) -> int {
        errno = ENOENT;
        return -1;
    });

    const char *argv[] = {
        "ocloc",
        "cache",
        "-show-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("No stats found in cache directory."));
}

TEST(OclocApiTestsLinux, GivenCacheCommandWithVerboseAndShowStatsAndNoStatsFileThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> opendirBackup(&NEO::SysCalls::sysCallsOpendir, OclocCacheCommandLinux::mockOpendirNotFound);

    const char *argv[] = {
        "ocloc",
        "cache",
        "-verbose",
        "-show-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("Reading cache statistics from path:"));
    EXPECT_NE(std::string::npos, output.find("No stats found in cache directories."));
}

TEST(OclocApiTestsLinux, GivenCacheCommandWithZeroStatsArgAndNoStatsFileThenSuccessIsReturnedWithInfoMessage) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> opendirBackup(&NEO::SysCalls::sysCallsOpendir, OclocCacheCommandLinux::mockOpendirNotFound);

    const char *argv[] = {
        "ocloc",
        "cache",
        "-zero-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("No stats found to reset."));
}

TEST(OclocApiTestsLinux, GivenCacheCommandWithVerboseAndZeroStatsThenVerboseMessageIsPrinted) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> opendirBackup(&NEO::SysCalls::sysCallsOpendir, OclocCacheCommandLinux::mockOpendirNotFound);

    const char *argv[] = {
        "ocloc",
        "cache",
        "-verbose",
        "-zero-stats"};
    unsigned int argc = sizeof(argv) / sizeof(const char *);

    StreamCapture capture;
    capture.captureStdout();
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(retVal, OCLOC_SUCCESS);
    EXPECT_NE(std::string::npos, output.find("Resetting cache statistics in path:"));
}

TEST(OclocApiTestsLinux, GivenNonZeroStatsWhenCacheCommandZeroStatsThenShowStatsReturnsZeroed) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup(&NEO::SysCalls::sysCallsOpen, OclocCacheCommandLinux::mockOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> preadBackup(&NEO::SysCalls::sysCallsPread, OclocCacheCommandLinux::mockPread);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPwrite)> pwriteBackup(&NEO::SysCalls::sysCallsPwrite, OclocCacheCommandLinux::mockPwrite);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> opendirBackup(&NEO::SysCalls::sysCallsOpendir, OclocCacheCommandLinux::mockOpendir);
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> readdirBackup(&NEO::SysCalls::sysCallsReaddir, OclocCacheCommandLinux::mockReaddir);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> closedirBackup(&NEO::SysCalls::sysCallsClosedir, OclocCacheCommandLinux::mockClosedir);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> statBackup(&NEO::SysCalls::sysCallsStat, OclocCacheCommandLinux::mockStat);

    OclocCacheCommandLinux::storedStats = {42, 17, NEO::CompilerCache::cacheVersion};

    {
        const char *argv[] = {"ocloc", "cache", "-dir", "/test/cache", "-show-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS);
        EXPECT_NE(std::string::npos, output.find("42"));
        EXPECT_NE(std::string::npos, output.find("17"));
    }

    {
        OclocCacheCommandLinux::returnedEntry = false;
        const char *argv[] = {"ocloc", "cache", "-dir", "/test/cache", "-zero-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);

        EXPECT_EQ(retVal, OCLOC_SUCCESS);
    }

    EXPECT_EQ(0u, OclocCacheCommandLinux::storedStats.hits);
    EXPECT_EQ(0u, OclocCacheCommandLinux::storedStats.misses);
    EXPECT_EQ(NEO::CompilerCache::cacheVersion, OclocCacheCommandLinux::storedStats.version);

    {
        const char *argv[] = {"ocloc", "cache", "-dir", "/test/cache", "-show-stats"};
        unsigned int argc = sizeof(argv) / sizeof(const char *);

        StreamCapture capture;
        capture.captureStdout();
        int retVal = oclocInvoke(argc, argv,
                                 0, nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(retVal, OCLOC_SUCCESS);
        EXPECT_EQ(std::string::npos, output.find("42"));
        EXPECT_EQ(std::string::npos, output.find("17"));
        EXPECT_NE(std::string::npos, output.find("0.00%"));
    }
}
